/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "fingerprint.h"
#include "et7xx.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
#include <linux/smc.h>
#endif
#include <linux/sysfs.h>

static DECLARE_BITMAP(minors, N_SPI_MINORS);

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

#ifdef ENABLE_SENSORS_FPRINT_SECURE
int fpsensor_goto_suspend = 0;
#endif

static int gpio_irq;
static struct etspi_data *g_data;
static DECLARE_WAIT_QUEUE_HEAD(interrupt_waitq);
static unsigned int bufsiz = 1024;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
int fps_resume_set(void) {
	int ret = 0;

	if (fpsensor_goto_suspend) {
		fpsensor_goto_suspend = 0;
#if !defined(CONFIG_TZDEV)
		ret = exynos_smc(MC_FC_FP_PM_RESUME, 0, 0, 0);
		pr_info("etspi %s : smc ret = %d\n", __func__, ret);
#endif
	}
	return ret;
}
#endif

static irqreturn_t etspi_fingerprint_interrupt(int irq, void *dev_id)
{
	struct etspi_data *etspi = (struct etspi_data *)dev_id;

	etspi->int_count++;
	etspi->finger_on = 1;
	disable_irq_nosync(gpio_irq);
	wake_up_interruptible(&interrupt_waitq);
	wake_lock_timeout(&etspi->fp_signal_lock, 1 * HZ);
	pr_info("%s FPS triggered.int_count(%d) On(%d)\n", __func__,
		etspi->int_count, etspi->finger_on);
	return IRQ_HANDLED;
}

int etspi_Interrupt_Init(
		struct etspi_data *etspi,
		int int_ctrl,
		int detect_period,
		int detect_threshold)
{
	int status = 0;

	etspi->finger_on = 0;
	etspi->int_count = 0;
	pr_info("%s int_ctrl = %d detect_period = %d detect_threshold = %d\n",
				__func__,
				int_ctrl,
				detect_period,
				detect_threshold);

	etspi->detect_period = detect_period;
	etspi->detect_threshold = detect_threshold;
	gpio_irq = gpio_to_irq(etspi->drdyPin);

	if (gpio_irq < 0) {
		pr_err("%s gpio_to_irq failed\n", __func__);
		status = gpio_irq;
		goto done;
	}

	if (etspi->drdy_irq_flag == DRDY_IRQ_DISABLE) {
		if (request_irq
			(gpio_irq, etspi_fingerprint_interrupt
			, int_ctrl, "etspi_irq", etspi) < 0) {
			pr_err("%s drdy request_irq failed\n", __func__);
			status = -EBUSY;
			goto done;
		} else {
			enable_irq_wake(gpio_irq);
			etspi->drdy_irq_flag = DRDY_IRQ_ENABLE;
		}
	}
done:
	return status;
}

int etspi_Interrupt_Free(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		if (etspi->drdy_irq_flag == DRDY_IRQ_ENABLE) {
			if (!etspi->int_count)
				disable_irq_nosync(gpio_irq);

			disable_irq_wake(gpio_irq);
			free_irq(gpio_irq, etspi);
			etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		}
		etspi->finger_on = 0;
		etspi->int_count = 0;
	}
	return 0;
}

void etspi_Interrupt_Abort(struct etspi_data *etspi)
{
	wake_up_interruptible(&interrupt_waitq);
}

unsigned int etspi_fps_interrupt_poll(
		struct file *file,
		struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct etspi_data *etspi = file->private_data;

	pr_debug("%s FPS fps_interrupt_poll, finger_on(%d), int_count(%d)\n",
		__func__, etspi->finger_on, etspi->int_count);

	if (!etspi->finger_on)
		poll_wait(file, &interrupt_waitq, wait);

	if (etspi->finger_on) {
		mask |= POLLIN | POLLRDNORM;
		etspi->finger_on = 0;
	}
	return mask;
}

/*-------------------------------------------------------------------------*/

static void etspi_reset(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	gpio_set_value(etspi->sleepPin, 0);
	usleep_range(1050, 1100);
	gpio_set_value(etspi->sleepPin, 1);
}

static void etspi_reset_control(struct etspi_data *etspi, int status)
{
	pr_info("%s\n", __func__);
	gpio_set_value(etspi->sleepPin, status);
}

void etspi_pin_control(struct etspi_data *etspi, bool pin_set)
{
	int status = 0;

	if (IS_ERR(etspi->p))
		return;

	etspi->p->state = NULL;
	if (pin_set) {
		if (!IS_ERR(etspi->pins_poweron)) {
			status = pinctrl_select_state(etspi->p,
				etspi->pins_poweron);
			if (status)
				pr_err("%s: can't set pin default state\n",
					__func__);
			pr_info("%s idle\n", __func__);
		}
	} else {
		if (!IS_ERR(etspi->pins_poweroff)) {
			status = pinctrl_select_state(etspi->p,
				etspi->pins_poweroff);
			if (status)
				pr_err("%s: can't set pin sleep state\n",
					__func__);
			pr_info("%s sleep\n", __func__);
		}
	}
}

static void etspi_power_control(struct etspi_data *etspi, int status)
{
	int rc = 0;

	if (etspi->ldo_enabled == status)
	{
		pr_err("%s called duplicate\n", __func__);
		return;
	}
	pr_info("%s status = %d\n", __func__, status);
	if (status == 1) {
		if (etspi->regulator_3p3) {
			rc = regulator_enable(etspi->regulator_3p3);
			if (rc)
				pr_err("%s regulator enable failed rc=%d\n", __func__, rc);
			etspi->ldo_enabled = 1;
			usleep_range(2000, 2000);
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 1);
			etspi->ldo_enabled = 1;
			usleep_range(2000, 2000);
		}
		if (etspi->sleepPin) {
			gpio_set_value(etspi->sleepPin, 1);
			usleep_range(1100, 1150);
		}
		if (etspi->drdyPin)
			gpio_direction_output(etspi->drdyPin, 0);
		etspi_pin_control(etspi, true);
	} else if (status == 0) {
		etspi_pin_control(etspi, false);
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
#if !defined(CONFIG_TZDEV)
		pr_info("%s: cs_set smc ret = %d\n", __func__,
			exynos_smc(MC_FC_FP_CS_SET, 0, 0, 0));
#endif
#endif
		if (etspi->sleepPin)
			gpio_set_value(etspi->sleepPin, 0);
		if (etspi->regulator_3p3) {
			rc = regulator_disable(etspi->regulator_3p3);
			if (rc)
				pr_err("%s regulator disable failed rc=%d\n", __func__, rc);
			etspi->ldo_enabled = 0;
		} else if (etspi->ldo_pin) {
			gpio_set_value(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}
	} else {
		pr_err("%s can't support this value. %d\n", __func__, status);
	}
}

static ssize_t etspi_read(struct file *filp,
						char __user *buf,
						size_t count,
						loff_t *f_pos)
{
	/*Implement by vendor if needed*/
	return 0;
}

static ssize_t etspi_write(struct file *filp,
						const char __user *buf,
						size_t count,
						loff_t *f_pos)
{
/*Implement by vendor if needed*/
	return 0;
}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
static int etspi_sec_spi_prepare(struct sec_spi_info *spi_info,
		struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(fp_spi_pclk)) {
		pr_err("%s Can't get fp_spi_pclk\n", __func__);
		return PTR_ERR(fp_spi_pclk);
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");
	if (IS_ERR(fp_spi_sclk)) {
		pr_err("%s Can't get fp_spi_sclk\n", __func__);
		return PTR_ERR(fp_spi_sclk);
	}
	clk_prepare_enable(fp_spi_pclk);
	clk_prepare_enable(fp_spi_sclk);
#if defined(CONFIG_SOC_EXYNOS9610)
/* There is a quarter-multiplier before the SPI */
	clk_set_rate(fp_spi_sclk, spi_info->speed * 4);
#else
	clk_set_rate(fp_spi_sclk, spi_info->speed * 2);
#endif

	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);
	return 0;
}

static int etspi_sec_spi_unprepare(struct sec_spi_info *spi_info,
		struct spi_device *spi)
{
	struct clk *fp_spi_pclk, *fp_spi_sclk;

	fp_spi_pclk = clk_get(NULL, "fp-spi-pclk");
	if (IS_ERR(fp_spi_pclk)) {
		pr_err("%s Can't get fp_spi_pclk\n", __func__);
		return PTR_ERR(fp_spi_pclk);
	}

	fp_spi_sclk = clk_get(NULL, "fp-spi-sclk");
	if (IS_ERR(fp_spi_sclk)) {
		pr_err("%s Can't get fp_spi_sclk\n", __func__);
		return PTR_ERR(fp_spi_sclk);
	}
	clk_disable_unprepare(fp_spi_pclk);
	clk_disable_unprepare(fp_spi_sclk);
	clk_put(fp_spi_pclk);
	clk_put(fp_spi_sclk);

	return 0;
}
#endif

static long etspi_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, retval = 0;
	struct etspi_data *etspi;
	struct spi_device *spi;
	u32 tmp;
	struct egis_ioc_transfer *ioc = NULL;
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	struct egis_ioc_transfer_32 *ioc_32 = NULL;
	u64 tx_buffer_64, rx_buffer_64;
#endif
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	u8 *buf, *address, *result, *fr;
#endif
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	struct sec_spi_info *spi_info = NULL;
#endif
	/* Check type and command number */
	if (_IOC_TYPE(cmd) != EGIS_IOC_MAGIC) {
		pr_err("%s _IOC_TYPE(cmd) != EGIS_IOC_MAGIC", __func__);
		return -ENOTTY;
	}

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
						(void __user *)arg,
						_IOC_SIZE(cmd));
	if (err) {
		pr_err("%s err", __func__);
		return -EFAULT;
	}

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	etspi = filp->private_data;
	spin_lock_irq(&etspi->spi_lock);
	spi = spi_dev_get(etspi->spi);
	spin_unlock_irq(&etspi->spi_lock);

	if (spi == NULL) {
		pr_err("%s spi == NULL", __func__);
		return -ESHUTDOWN;
	}

	mutex_lock(&etspi->buf_lock);

	/* segmented and/or full-duplex I/O request */
	if (_IOC_NR(cmd) != _IOC_NR(EGIS_IOC_MESSAGE(0))
					|| _IOC_DIR(cmd) != _IOC_WRITE) {
		retval = -ENOTTY;
		goto out;
	}

	/*
	 *	If platform is 32bit and kernel is 64bit
	 *	We will alloc egis_ioc_transfer for 64bit and 32bit
	 *	We use ioc_32(32bit) to get data from user mode.
	 *	Then copy the ioc_32 to ioc(64bit).
	 */
#ifdef CONFIG_SENSORS_FINGERPRINT_32BITS_PLATFORM_ONLY
	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer_32)) != 0) {
		pr_err("%s ioc_32 size error\n", __func__);
		retval = -EINVAL;
		goto out;
	}
	ioc_32 = kmalloc(tmp, GFP_KERNEL);
	if (ioc_32 == NULL) {
		retval = -ENOMEM;
		pr_err("%s ioc_32 kmalloc error\n", __func__);
		goto out;
	}
	if (__copy_from_user(ioc_32, (void __user *)arg, tmp)) {
		retval = -EFAULT;
		pr_err("%s ioc_32 copy_from_user error\n", __func__);
		goto out;
	}
	ioc = kmalloc(sizeof(struct egis_ioc_transfer), GFP_KERNEL);
	if (ioc == NULL) {
		retval = -ENOMEM;
		pr_err("%s ioc kmalloc error\n", __func__);
		goto out;
	}
	tx_buffer_64 = (u64)ioc_32->tx_buf;
	rx_buffer_64 = (u64)ioc_32->rx_buf;
	ioc->tx_buf = (u8 *)tx_buffer_64;
	ioc->rx_buf = (u8 *)rx_buffer_64;
	ioc->len = ioc_32->len;
	ioc->speed_hz = ioc_32->speed_hz;
	ioc->delay_usecs = ioc_32->delay_usecs;
	ioc->bits_per_word = ioc_32->bits_per_word;
	ioc->cs_change = ioc_32->cs_change;
	ioc->opcode = ioc_32->opcode;
	memcpy(ioc->pad, ioc_32->pad, 3);
	kfree(ioc_32);
#else
	tmp = _IOC_SIZE(cmd);
	if ((tmp == 0) || (tmp % sizeof(struct egis_ioc_transfer)) != 0) {
		pr_err("%s ioc size error\n", __func__);
		retval = -EINVAL;
		goto out;
	}
	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc) {
		retval = -ENOMEM;
		goto out;
	}
	if (__copy_from_user(ioc, (void __user *)arg, tmp)) {
		pr_err("%s __copy_from_user error\n", __func__);
		retval = -EFAULT;
		goto out;
	}
#endif

	switch (ioc->opcode) {
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	/*
	 * Read register
	 * tx_buf include register address will be read
	 */
	case FP_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_debug("etspi FP_REGISTER_READ\n");
		retval = etspi_io_read_register(etspi, address, result);
		if (retval < 0)	{
			pr_err("%s FP_REGISTER_READ error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BREAD:
		pr_debug("%s FP_REGISTER_BREAD\n", __func__);
		retval = etspi_io_burst_read_register(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BREAD error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BREAD_BACKWARD:
		pr_debug("%s FP_REGISTER_BREAD_BACKWARD\n", __func__);
		retval = etspi_io_burst_read_register_backward(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BREAD_BACKWARD error retval = %d\n"
				, __func__, retval);
		}
		break;
	/*
	 * Write data to register
	 * tx_buf includes address and value will be wrote
	 */
	case FP_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_debug("%s FP_REGISTER_WRITE\n", __func__);
		retval = etspi_io_write_register(etspi, buf);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_WRITE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BWRITE:
		pr_debug("%s FP_REGISTER_BWRITE\n", __func__);
		retval = etspi_io_burst_write_register(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BWRITE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_REGISTER_BWRITE_BACKWARD:
		pr_debug("%s FP_REGISTER_BWRITE_BACKWARD\n", __func__);
		retval = etspi_io_burst_write_register_backward(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_REGISTER_BWRITE_BACKWARD error retval = %d\n"
				, __func__, retval);
		}
		break;

	case FP_EFUSE_READ:
		pr_info("%s FP_EFUSE_READ\n", __func__);
		retval = etspi_io_read_efuse(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_EFUSE_READ error retval = %d\n"
				, __func__, retval);
		}
		break;
	case FP_EFUSE_WRITE:
		pr_info("%s FP_EFUSE_WRITE\n", __func__);
		retval = etspi_io_write_efuse(etspi, ioc);
		if (retval < 0) {
			pr_err("%s FP_EFUSE_WRITE error retval = %d\n"
				, __func__, retval);
		}
		break;
	case FP_GET_IMG:
		fr = ioc->rx_buf;
		pr_info("%s FP_GET_IMG\n", __func__);
		retval = etspi_io_get_frame(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_GET_IMG error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_WRITE_IMG:
		fr = ioc->tx_buf;
		pr_info("%s FP_WRITE_IMG\n", __func__);
		retval = etspi_io_write_frame(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_WRITE_IMG error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_GET_ZAVG:
		fr = ioc->rx_buf;
		pr_info("%s FP_GET_ZAVG\n", __func__);
		retval = etspi_io_get_zone_average(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_GET_ZAVG error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_GET_HSTG:
		fr = ioc->rx_buf;
		pr_info("%s FP_GET_HSTG\n", __func__);
		retval = etspi_io_get_histogram(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_GET_HSTG error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_CIS_REGISTER_READ:
		address = ioc->tx_buf;
		result = ioc->rx_buf;
		pr_info("etspi FP_CIS_REGISTER_READ\n");
		retval = etspi_io_read_cis_register(etspi, address, result);
		if (retval < 0)	{
			pr_err("%s FP_CIS_REGISTER_READ error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_CIS_REGISTER_WRITE:
		buf = ioc->tx_buf;
		pr_info("%s FP_CIS_REGISTER_WRITE\n", __func__);
		retval = etspi_io_write_cis_register(etspi, buf);
		if (retval < 0) {
			pr_err("%s FP_CIS_REGISTER_WRITE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_CIS_PRE_CAPTURE:
		pr_info("%s FP_CIS_PRE_CAPTURE\n", __func__);
		retval = etspi_io_pre_capture(etspi);
		if (retval < 0) {
			pr_err("%s FP_CIS_PRE_CAPTURE error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_GET_CIS_FRAME:
		fr = ioc->rx_buf;
		pr_info("%s FP_GET_FRAME\n", __func__);
		retval = etspi_io_get_cis_frame(etspi, fr, ioc->len);
		if (retval < 0) {
			pr_err("%s FP_GET_FRAME error retval = %d\n"
			, __func__, retval);
		}
		break;
	case FP_TRANSFER_COMMAND:
		pr_info("%s FP_TRANSFER_COMMAND\n", __func__);
		retval = etspi_io_transfer_command(etspi, ioc->tx_buf, ioc->rx_buf,
											ioc->len);
		if (retval < 0) {
			pr_err("%s FP_TRANSFER_COMMAND error retval = %d\n", __func__,
				   retval);
		}
		break;
	case FP_EEPROM_READ:
		pr_info("%s FP_EEPROM_READ\n", __func__);
		etspi_eeprom_read(etspi, ioc);
		break;
	case FP_EEPROM_HIGH_SPEED_READ:
		pr_info("%s FP_EEPROM_HIGH_SPEED_READ\n", __func__);
		etspi_eeprom_high_speed_read(etspi, ioc);
		break;
	case FP_EEPROM_WRITE:
		pr_info("%s FP_EEPROM_WRITE\n", __func__);
		etspi_eeprom_write(etspi, ioc);
		break;
	case FP_EEPROM_CHIP_ERASE:
		pr_info("%s FP_EEPROM_CHIP_ERASE\n", __func__);
		etspi_eeprom_chip_erase(etspi);
		break;
	case FP_EEPROM_SECTOR_ERASE:
		pr_info("%s FP_EEPROM_SECTOR_ERASE\n", __func__);
		etspi_eeprom_sector_erase(etspi, ioc);
		break;
	case FP_EEPROM_BLOCK_ERASE:
		pr_info("%s FP_EEPROM_BLOCK_ERASE\n", __func__);
		etspi_eeprom_block_erase(etspi, ioc);
		break;
	case FP_EEPROM_WREN:
		pr_info("%s FP_EEPROM_WREN\n", __func__);
		etspi_eeprom_write_controller(etspi, 1);
		break;
	case FP_EEPROM_WRDI:
		pr_info("%s FP_EEPROM_WRDI\n", __func__);
		etspi_eeprom_write_controller(etspi, 0);
		break;
	case FP_EEPROM_RSDR:
		pr_info("%s FP_EEPROM_RSDR\n", __func__);
		etspi_eeprom_rdsr(etspi, ioc->rx_buf);
		break;
	case FP_EEPROM_WRITE_IN_NON_TZ:
		pr_info("%s FP_EEPROM_WRITE_IN_NON_TZ\n", __func__);
		etspi_eeprom_write_in_non_tz(etspi, ioc);
		break;
#endif
	case FP_SENSOR_RESET:
		pr_info("%s FP_SENSOR_RESET\n", __func__);
		etspi_reset(etspi);
		break;

	case FP_RESET_CONTROL:
		pr_info("%s FP_RESET_CONTROL, status = %d\n", __func__,
			ioc->len);
		etspi_reset_control(etspi, ioc->len);
		break;

	case FP_POWER_CONTROL:
		pr_info("%s FP_POWER_CONTROL, status = %d\n", __func__,
				ioc->len);
		etspi_power_control(etspi, ioc->len);
		break;
	case FP_SET_SPI_CLOCK:
		pr_info("%s FP_SET_SPI_CLOCK, clock = %d\n", __func__,
				ioc->speed_hz);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		if (etspi->enabled_clk) {
			if (spi->max_speed_hz == ioc->speed_hz) {
				pr_info("%s already enabled same clock.\n",
					__func__);
				break;
			}
			pr_info("%s already enabled. DISABLE_SPI_CLOCK\n",
				__func__);
			retval = etspi_sec_spi_unprepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: couldn't disable spi clks\n",
					__func__);
			wake_unlock(&etspi->fp_spi_lock);
			etspi->enabled_clk = false;
		}
		spi->max_speed_hz = ioc->speed_hz;
		spi_info = kmalloc(sizeof(struct sec_spi_info),
			GFP_KERNEL);
		if (spi_info != NULL) {
			pr_info("%s ENABLE_SPI_CLOCK\n", __func__);

			spi_info->speed = spi->max_speed_hz;
			retval = etspi_sec_spi_prepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: Unable to enable spi clk\n",
					__func__);
			kfree(spi_info);
			wake_lock(&etspi->fp_spi_lock);
			etspi->enabled_clk = true;
		} else
			retval = -ENOMEM;
#else
		spi->max_speed_hz = ioc->speed_hz;
#endif
		break;

	/*
	 * Trigger initial routine
	 */
	case INT_TRIGGER_INIT:
		pr_debug("%s Trigger function init\n", __func__);
		if (!etspi->drdyPin) {
			retval = etspi_Interrupt_Init(
					etspi,
					(int)ioc->pad[0],
					(int)ioc->pad[1],
					(int)ioc->pad[2]);
		}
		break;

	/* trigger */
	case INT_TRIGGER_CLOSE:
		pr_debug("%s Trigger function close\n", __func__);
		if (!etspi->drdyPin)
			retval = etspi_Interrupt_Free(etspi);
		break;

	/* Poll Abort */
	case INT_TRIGGER_ABORT:
		pr_debug("%s Trigger function abort\n", __func__);
		if (!etspi->drdyPin)
			etspi_Interrupt_Abort(etspi);
		break;

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	case FP_DISABLE_SPI_CLOCK:
		pr_info("%s FP_DISABLE_SPI_CLOCK\n", __func__);
		if (etspi->enabled_clk) {
			pr_info("%s DISABLE_SPI_CLOCK\n", __func__);

			retval = etspi_sec_spi_unprepare(spi_info, spi);
			if (retval < 0)
				pr_err("%s: couldn't disable spi clks\n",
					__func__);
			wake_unlock(&etspi->fp_spi_lock);
			etspi->enabled_clk = false;
		}
		break;

	case FP_CPU_SPEEDUP:
		pr_info("%s FP_CPU_SPEEDUP\n", __func__);
		if (ioc->len) {
			u8 retry_cnt = 0;

			pr_info("%s FP_CPU_SPEEDUP ON:%d, retry: %d\n",
				__func__, ioc->len, retry_cnt);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			do {
				retval = secos_booster_start(ioc->len - 1);
				retry_cnt++;
				if (retval) {
					pr_err("%s: booster start failed. (%d) retry: %d\n"
						, __func__, retval, retry_cnt);
					if (retry_cnt < 7)
						usleep_range(500, 510);
				}
			} while (retval && retry_cnt < 7);
#elif defined(CONFIG_TZDEV_BOOST)
			tz_boost_enable();
#endif
		} else {
			pr_info("%s FP_CPU_SPEEDUP OFF\n", __func__);
#if defined(CONFIG_SECURE_OS_BOOSTER_API)
			retval = secos_booster_stop();
			if (retval)
				pr_err("%s: booster stop failed. (%d)\n"
					, __func__, retval);
#elif defined(CONFIG_TZDEV_BOOST)
			tz_boost_disable();
#endif
		}
		break;

	case FP_SET_SENSOR_TYPE:
		if ((int)ioc->len >= SENSOR_OOO &&
				(int)ioc->len < SENSOR_MAXIMUM) {
			if ((int)ioc->len == SENSOR_OOO &&
					etspi->sensortype == SENSOR_FAILED) {
				pr_info("%s maintain type check from out of order :%s\n",
					__func__,
					sensor_status[g_data->sensortype + 2]);
			} else {
				etspi->sensortype = (int)ioc->len;
				pr_info("%s FP_SET_SENSOR_TYPE :%s\n",
					__func__,
					sensor_status[g_data->sensortype + 2]);
			}
		} else {
			pr_err("%s FP_SET_SENSOR_TYPE invalid value %d\n",
					__func__, (int)ioc->len);
			etspi->sensortype = SENSOR_UNKNOWN;
		}
		break;

	case FP_SET_LOCKSCREEN:
		pr_info("%s FP_SET_LOCKSCREEN\n", __func__);
		break;

	case FP_SET_WAKE_UP_SIGNAL:
		pr_info("%s FP_SET_WAKE_UP_SIGNAL\n", __func__);
		break;
#endif

	case FP_SENSOR_ORIENT:
		pr_info("%s: orient is %d\n", __func__, etspi->orient);

		retval = put_user(etspi->orient, (u8 __user *) (uintptr_t)ioc->rx_buf);
		if (retval != 0)
			pr_err("%s FP_SENSOR_ORIENT put_user fail: %d\n",
											__func__, retval);
		break;

	case FP_SPI_VALUE:
		etspi->spi_value = ioc->len;
		pr_info("%s spi_value: 0x%x\n", __func__, etspi->spi_value);
		break;

	case FP_MODEL_INFO:
		pr_info("%s: modelinfo is %s\n", __func__, etspi->model_info);

		retval = copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf,
												etspi->model_info, 10);
		if (retval != 0)
			pr_err("%s FP_IOCTL_MODEL_INFO copy_to_user failed: %d\n",
											__func__, retval);
		break;

	case FP_IOCTL_RESERVED_01:
	case FP_IOCTL_RESERVED_02:
		break;

	default:
		retval = -EFAULT;
		break;

	}

out:
	if (ioc != NULL)
		kfree(ioc);

	mutex_unlock(&etspi->buf_lock);
	spi_dev_put(spi);
	if (retval < 0)
		pr_err("%s retval = %d\n", __func__, retval);
	return retval;
}

#ifdef CONFIG_COMPAT
static long etspi_compat_ioctl(struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	return etspi_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define etspi_compat_ioctl NULL
#endif
/* CONFIG_COMPAT */

static int etspi_open(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;
	int	status = -ENXIO;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);

	list_for_each_entry(etspi, &device_list, device_entry) {
		if (etspi->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}
	if (status == 0) {
		if (etspi->buf == NULL) {
			etspi->buf = kmalloc(bufsiz, GFP_KERNEL);
			if (etspi->buf == NULL) {
				dev_dbg(&etspi->spi->dev, "open/ENOMEM\n");
				status = -ENOMEM;
			}
		}
		if (status == 0) {
			etspi->users++;
			filp->private_data = etspi;
			nonseekable_open(inode, filp);
			etspi->bufsiz = bufsiz;
		}
	} else
		pr_debug("%s nothing for minor %d\n"
			, __func__, iminor(inode));

	mutex_unlock(&device_list_lock);
	return status;
}

static int etspi_release(struct inode *inode, struct file *filp)
{
	struct etspi_data *etspi;

	pr_info("%s\n", __func__);
	mutex_lock(&device_list_lock);
	etspi = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	etspi->users--;
	if (etspi->users == 0) {
		int	dofree;

		kfree(etspi->buf);
		etspi->buf = NULL;

		/* ... after we unbound from the underlying device? */
		spin_lock_irq(&etspi->spi_lock);
		dofree = (etspi->spi == NULL);
		spin_unlock_irq(&etspi->spi_lock);

		if (dofree)
			kfree(etspi);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

int etspi_platformInit(struct etspi_data *etspi)
{
	int status = 0;

	pr_info("%s\n", __func__);
	/* gpio setting for ldo, ldo2, sleep, drdy pin */
	if (etspi != NULL) {
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;

		if (etspi->btp_vdd) {
			etspi->regulator_3p3 = regulator_get(NULL, etspi->btp_vdd);
			if (IS_ERR(etspi->regulator_3p3)) {
				pr_err("%s regulator get failed\n", __func__);
				etspi->regulator_3p3 = NULL;
				goto etspi_platformInit_ldo_failed;
			} else {
				pr_info("btp_regulator ok\n");
				etspi->ldo_enabled = 0;
			}
		} else if (etspi->ldo_pin) {
			status = gpio_request(etspi->ldo_pin, "etspi_ldo_en");
			if (status < 0) {
				pr_err("%s gpio_request etspi_ldo_en failed\n",
					__func__);
				goto etspi_platformInit_ldo_failed;
			}
			gpio_direction_output(etspi->ldo_pin, 0);
			etspi->ldo_enabled = 0;
		}
		status = gpio_request(etspi->sleepPin, "etspi_sleep");
		if (status < 0) {
			pr_err("%s gpio_requset etspi_sleep failed\n",
				__func__);
			goto etspi_platformInit_sleep_failed;
		}

		gpio_direction_output(etspi->sleepPin, 0);
		if (status < 0) {
			pr_err("%s gpio_direction_output SLEEP failed\n",
					__func__);
			status = -EBUSY;
			goto etspi_platformInit_sleep_failed;
		}

		if (etspi->drdyPin) {
			status = gpio_request(etspi->drdyPin, "etspi_drdy");
			if (status < 0) {
				pr_err("%s gpio_request etspi_drdy failed\n",
					__func__);
				goto etspi_platformInit_drdy_failed;
			}
			status = gpio_direction_input(etspi->drdyPin);
			if (status < 0) {
				pr_err("%s gpio_direction_input DRDY failed\n",
					__func__);
				goto etspi_platformInit_gpio_init_failed;
			}
		}

		if (etspi->sleepPin)
			pr_info("%s sleep value =%d\n", __func__,
					gpio_get_value(etspi->sleepPin));
		if (etspi->ldo_pin)
			pr_info("%s ldo en value =%d\n", __func__,
					gpio_get_value(etspi->ldo_pin));

	} else {
		status = -EFAULT;
	}

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	wake_lock_init(&etspi->fp_spi_lock,
		WAKE_LOCK_SUSPEND, "etspi_wake_lock");
#endif
	wake_lock_init(&etspi->fp_signal_lock,
				WAKE_LOCK_SUSPEND, "etspi_sigwake_lock");

	pr_info("%s successful status=%d\n", __func__, status);
	return status;
etspi_platformInit_gpio_init_failed:
	if (etspi->drdyPin)
		gpio_free(etspi->drdyPin);
etspi_platformInit_drdy_failed:
	gpio_free(etspi->sleepPin);
etspi_platformInit_sleep_failed:
	if (etspi->ldo_pin)
		gpio_free(etspi->ldo_pin);
etspi_platformInit_ldo_failed:
	pr_err("%s is failed\n", __func__);
	return status;
}

void etspi_platformUninit(struct etspi_data *etspi)
{
	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		disable_irq_wake(gpio_irq);
		disable_irq(gpio_irq);
		etspi_pin_control(etspi, false);
		free_irq(gpio_irq, etspi);
		etspi->drdy_irq_flag = DRDY_IRQ_DISABLE;
		if (etspi->regulator_3p3)
			regulator_put(etspi->regulator_3p3);
		else if (etspi->ldo_pin)
			gpio_free(etspi->ldo_pin);
		gpio_free(etspi->sleepPin);
		gpio_free(etspi->drdyPin);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		wake_lock_destroy(&etspi->fp_spi_lock);
#endif
		wake_lock_destroy(&etspi->fp_signal_lock);
	}
}

static int etspi_parse_dt(struct device *dev, struct etspi_data *data)
{
	struct device_node *np = dev->of_node;
	enum of_gpio_flags flags;
	int errorno = 0;
	int gpio;

	gpio = of_get_named_gpio_flags(np, "etspi-sleepPin",
		0, &flags);
	if (gpio < 0) {
		errorno = gpio;
		goto dt_exit;
	} else {
		data->sleepPin = gpio;
		pr_info("%s: sleepPin=%d\n",
			__func__, data->sleepPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-drdyPin",
		0, &flags);
	if (gpio < 0) {
		data->drdyPin = 0;
		pr_err("%s: fail to get drdy_pin\n", __func__);
	} else {
		data->drdyPin = gpio;
		pr_info("%s: drdyPin=%d\n",
			__func__, data->drdyPin);
	}
	gpio = of_get_named_gpio_flags(np, "etspi-ldoPin",
		0, &flags);
	if (gpio < 0) {
		data->ldo_pin = 0;
		pr_err("%s: fail to get ldo_pin\n", __func__);
	} else {
		data->ldo_pin = gpio;
		pr_info("%s: ldo_pin=%d\n",
			__func__, data->ldo_pin);
	}
	if (of_property_read_string(np, "etspi-regulator", &data->btp_vdd) < 0) {
		pr_info("not use btp_regulator\n");
		data->btp_vdd = NULL;
	}
	pr_info("%s: regulator: %s\n", __func__, data->btp_vdd);

	if (of_property_read_string_index(np, "etspi-chipid", 0,
			(const char **)&data->chipid)) {
		data->chipid = NULL;
	}
	pr_info("%s: chipid: %s\n", __func__, data->chipid);

	if (of_property_read_string_index(np, "etspi-modelinfo", 0,
			(const char **)&data->model_info)) {
		data->model_info = "NONE";
	}
	pr_info("%s: modelinfo: %s\n", __func__, data->model_info);

	if (of_property_read_string_index(np, "etspi-position", 0,
			(const char **)&data->sensor_position)) {
		data->sensor_position = "0.00,0.00,0.00,0.00,0.00,0.00,0.00,0.00";
	}
	pr_info("%s: position: %s\n", __func__, data->sensor_position);

	if (of_property_read_u32(np, "etspi-orient", &data->orient))
		data->orient = 0;
	pr_info("%s: orient: %d\n", __func__, data->orient);


	data->p = pinctrl_get_select_default(dev);
	if (!IS_ERR(data->p)) {
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
		data->pins_poweroff = pinctrl_lookup_state(data->p, "pins_poweroff");
#else
		data->pins_poweroff = pinctrl_lookup_state(data->p, "pins_poweroff_tz");
#endif
		if (IS_ERR(data->pins_poweroff)) {
			pr_err("%s : could not get pins sleep_state (%li)\n",
				__func__, PTR_ERR(data->pins_poweroff));
		}
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) || defined(DISABLED_GPIO_PROTECTION)
		data->pins_poweron = pinctrl_lookup_state(data->p, "pins_poweron");
#else
		data->pins_poweron = pinctrl_lookup_state(data->p, "pins_poweron_tz");
#endif
		if (IS_ERR(data->pins_poweron)) {
			pr_err("%s : could not get pins idle_state (%li)\n",
				__func__, PTR_ERR(data->pins_poweron));
		}
	}
	else {
		pr_err("%s: failed pinctrl_get\n", __func__);
	}
	etspi_pin_control(data, false);
	pr_info("%s is successful\n", __func__);
	return errorno;

dt_exit:
	pr_err("%s is failed\n", __func__);
	return errorno;
}

static const struct file_operations etspi_fops = {
	.owner = THIS_MODULE,
	.write = etspi_write,
	.read = etspi_read,
	.unlocked_ioctl = etspi_ioctl,
	.compat_ioctl = etspi_compat_ioctl,
	.open = etspi_open,
	.release = etspi_release,
	.llseek = no_llseek,
	.poll = etspi_fps_interrupt_poll
};

#ifndef ENABLE_SENSORS_FPRINT_SECURE
static int etspi_type_check(struct etspi_data *etspi)
{
	u8 buf1, buf2, buf3;

	etspi_power_control(g_data, 1);

	msleep(20);

	etspi_read_register(etspi, 0x00, &buf1);
	etspi_read_register(etspi, 0x01, &buf2);
	etspi_read_register(etspi, 0x02, &buf3);

	etspi_power_control(g_data, 0);

	pr_info("%s buf1-3: %x, %x, %x\n",
		__func__, buf1, buf2, buf3);

	/*
	 * type check return value
	 * ET711A : 0x07 / 0x1D or 0x07 / 0x0B
	 * ET713A : 0x07 / 0x0D
	 * ET715  : 0x07 / 0x0F
	 */
	if ((buf1 == 0x07) && ((buf2 == 0x1D) || (buf2 == 0x0B))) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET711A sensor\n", __func__);
	} else if ((buf1 == 0x07) && (buf2 == 0x0D)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET713A sensor\n", __func__);
	} else if ((buf1 == 0x07) && (buf2 == 0x0F)) {
		etspi->sensortype = SENSOR_EGIS;
		pr_info("%s sensor type is EGIS ET715 sensor\n", __func__);
	} else {
		etspi->sensortype = SENSOR_FAILED;
		pr_info("%s sensor type is FAILED\n", __func__);
		return -ENODEV;
	}
	return 0;
}
#endif

static ssize_t etspi_bfs_values_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "\"FP_SPICLK\":\"%d\"\n",
			data->spi->max_speed_hz);
}

static ssize_t etspi_type_check_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct etspi_data *data = dev_get_drvdata(dev);
#if !defined(ENABLE_SENSORS_FPRINT_SECURE)
	int retry = 0;
	int status = 0;

	do {
		status = etspi_type_check(data);
		pr_info("%s type (%u), retry (%d)\n"
			, __func__, data->sensortype, retry);
	} while (!data->sensortype && ++retry < 3);

	if (status == -ENODEV)
		pr_info("%s type check fail\n", __func__);
#endif
	return snprintf(buf, PAGE_SIZE, "%d\n", data->sensortype);
}

static ssize_t etspi_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t etspi_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", g_data->chipid);
}

static ssize_t etspi_adm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", DETECT_ADM);
}

static ssize_t etspi_position_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", g_data->sensor_position);
}

static DEVICE_ATTR(bfs_values, 0444, etspi_bfs_values_show, NULL);
static DEVICE_ATTR(type_check, 0444, etspi_type_check_show, NULL);
static DEVICE_ATTR(vendor, 0444, etspi_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, etspi_name_show, NULL);
static DEVICE_ATTR(adm, 0444, etspi_adm_show, NULL);
static DEVICE_ATTR(position, 0444, etspi_position_show, NULL);

static struct device_attribute *fp_attrs[] = {
	&dev_attr_bfs_values,
	&dev_attr_type_check,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_adm,
	&dev_attr_position,
	NULL,
};

static void etspi_work_func_debug(struct work_struct *work)
{
	pr_info("%s ldo: %d, sleep: %d, tz: %d, spi_value: 0x%x, type: %s\n",
		__func__,
		g_data->ldo_enabled, gpio_get_value(g_data->sleepPin),
		g_data->tz_mode, g_data->spi_value,
		sensor_status[g_data->sensortype + 2]);
}

static void etspi_enable_debug_timer(void)
{
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static void etspi_disable_debug_timer(void)
{
	del_timer_sync(&g_data->dbg_timer);
	cancel_work_sync(&g_data->work_debug);
}

static void etspi_timer_func(unsigned long ptr)
{
	queue_work(g_data->wq_dbg, &g_data->work_debug);
	mod_timer(&g_data->dbg_timer,
		round_jiffies_up(jiffies + FPSENSOR_DEBUG_TIMER_SEC));
}

static int etspi_set_timer(struct etspi_data *etspi)
{
	int status = 0;

	setup_timer(&etspi->dbg_timer,
		etspi_timer_func, (unsigned long)etspi);
	etspi->wq_dbg =
		create_singlethread_workqueue("etspi_debug_wq");
	if (!etspi->wq_dbg) {
		status = -ENOMEM;
		pr_err("%s could not create workqueue\n", __func__);
		return status;
	}
	INIT_WORK(&etspi->work_debug, etspi_work_func_debug);
	return status;
}

/*-------------------------------------------------------------------------*/

static struct class *etspi_class;

/*-------------------------------------------------------------------------*/

static int etspi_probe(struct spi_device *spi)
{
	struct etspi_data *etspi;
	int status;
	unsigned long minor;
#ifndef ENABLE_SENSORS_FPRINT_SECURE
	int retry = 0;
#endif

	pr_info("%s\n", __func__);
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	fpsensor_goto_suspend = 0;
#endif
	/* Allocate driver data */
	etspi = kzalloc(sizeof(*etspi), GFP_KERNEL);
	if (!etspi)
		return -ENOMEM;

	/* device tree call */
	if (spi->dev.of_node) {
		status = etspi_parse_dt(&spi->dev, etspi);
		if (status) {
			pr_err("%s - Failed to parse DT\n", __func__);
			goto etspi_probe_parse_dt_failed;
		}
	}

	/* Initialize the driver data */
	etspi->spi = spi;
	g_data = etspi;

	spin_lock_init(&etspi->spi_lock);
	mutex_init(&etspi->buf_lock);
	mutex_init(&device_list_lock);

	INIT_LIST_HEAD(&etspi->device_entry);

	/* platform init */
	status = etspi_platformInit(etspi);
	if (status != 0) {
		pr_err("%s platforminit failed\n", __func__);
		goto etspi_probe_platformInit_failed;
	}

	spi->bits_per_word = 8;
	spi->max_speed_hz = SLOW_BAUD_RATE;
	spi->mode = SPI_MODE_0;
	spi->chip_select = 0;
#if !defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
	status = spi_setup(spi);
	if (status != 0) {
		pr_err("%s spi_setup() is failed. status : %d\n",
			__func__, status);
		return status;
	}
#endif
	etspi->spi_value = 0;
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->sensortype = SENSOR_UNKNOWN;
#else
	/* sensor hw type check */
	do {
		status = etspi_type_check(etspi);
		pr_info("%s type (%u), retry (%d)\n"
			, __func__, etspi->sensortype, retry);
	} while (!etspi->sensortype && ++retry < 3);

	if (status == -ENODEV)
		pr_info("%s type check fail\n", __func__);
#endif

#if defined(DISABLED_GPIO_PROTECTION)
	etspi_pin_control(etspi, 0);
#endif

#ifdef ENABLE_SENSORS_FPRINT_SECURE
	etspi->tz_mode = true;
#endif
	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		etspi->devt = MKDEV(ET7XX_MAJOR, minor);
		dev = device_create(etspi_class, &spi->dev,
				etspi->devt, etspi, "esfp0");
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}
	if (status == 0) {
		set_bit(minor, minors);
		list_add(&etspi->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	if (status == 0)
		spi_set_drvdata(spi, etspi);
	else
		goto etspi_create_failed;

	status = fingerprint_register(etspi->fp_device,
		etspi, fp_attrs, "fingerprint");
	if (status) {
		pr_err("%s sysfs register failed\n", __func__);
		goto etspi_register_failed;
	}

	status = etspi_set_timer(etspi);
	if (status)
		goto etspi_sysfs_failed;
	etspi_enable_debug_timer();
	pr_info("%s is successful\n", __func__);

	return status;

etspi_sysfs_failed:
	fingerprint_unregister(etspi->fp_device, fp_attrs);

etspi_register_failed:
	device_destroy(etspi_class, etspi->devt);
	class_destroy(etspi_class);
etspi_create_failed:
	etspi_platformUninit(etspi);
etspi_probe_platformInit_failed:
etspi_probe_parse_dt_failed:
	kfree(etspi);
	pr_err("%s is failed\n", __func__);

	return status;
}

static int etspi_remove(struct spi_device *spi)
{
	struct etspi_data *etspi = spi_get_drvdata(spi);

	pr_info("%s\n", __func__);

	if (etspi != NULL) {
		etspi_disable_debug_timer();
		etspi_platformUninit(etspi);

		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&etspi->spi_lock);
		etspi->spi = NULL;
		spi_set_drvdata(spi, NULL);
		spin_unlock_irq(&etspi->spi_lock);

		/* prevent new opens */
		mutex_lock(&device_list_lock);
		fingerprint_unregister(etspi->fp_device, fp_attrs);

		list_del(&etspi->device_entry);
		device_destroy(etspi_class, etspi->devt);
		clear_bit(MINOR(etspi->devt), minors);
		if (etspi->users == 0)
			kfree(etspi);
		mutex_unlock(&device_list_lock);
	}
	return 0;
}

static int etspi_pm_suspend(struct device *dev)
{
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
#if !defined(CONFIG_TZDEV)
	int ret = 0;
#endif
#endif

	pr_info("%s\n", __func__);

	if (g_data != NULL) {
#ifdef ENABLE_SENSORS_FPRINT_SECURE
		fpsensor_goto_suspend = 1; /* used by pinctrl_samsung.c */
#endif
		etspi_disable_debug_timer();

		if (!g_data->ldo_enabled) {
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
#if !defined(CONFIG_TZDEV)
			ret = exynos_smc(MC_FC_FP_PM_SUSPEND, 0, 0, 0);
			pr_info("%s: suspend smc ret = %d\n", __func__, ret);
#endif
#endif
		} else {
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
#if !defined(CONFIG_TZDEV)
			ret = exynos_smc(MC_FC_FP_PM_SUSPEND_CS_HIGH, 0, 0, 0);
			pr_info("%s: suspend_cs_high smc ret = %d\n",
				__func__, ret);
#endif
#endif
		}
	}
	return 0;
}

static int etspi_pm_resume(struct device *dev)
{
	pr_info("%s\n", __func__);
	if (g_data != NULL) {
		etspi_enable_debug_timer();
#if defined(ENABLE_SENSORS_FPRINT_SECURE) && !defined(DISABLED_GPIO_PROTECTION)
		if (fpsensor_goto_suspend)
			fps_resume_set();
#endif
	}
	return 0;
}

static const struct dev_pm_ops etspi_pm_ops = {
	.suspend = etspi_pm_suspend,
	.resume = etspi_pm_resume
};

static const struct of_device_id etspi_match_table[] = {
	{ .compatible = "etspi,et7xx",},
	{},
};

static struct spi_driver etspi_spi_driver = {
	.driver = {
		.name =	"egis_fingerprint",
		.owner = THIS_MODULE,
		.pm = &etspi_pm_ops,
		.of_match_table = etspi_match_table
	},
	.probe = etspi_probe,
	.remove = etspi_remove,
};

/*-------------------------------------------------------------------------*/

static int __init etspi_init(void)
{
	int status;

	pr_info("%s\n", __func__);

	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(ET7XX_MAJOR, "egis_fingerprint", &etspi_fops);
	if (status < 0) {
		pr_err("%s register_chrdev error.status:%d\n", __func__,
				status);
		return status;
	}

	etspi_class = class_create(THIS_MODULE, "egis_fingerprint");
	if (IS_ERR(etspi_class)) {
		pr_err("%s class_create error.\n", __func__);
		unregister_chrdev(ET7XX_MAJOR, etspi_spi_driver.driver.name);
		return PTR_ERR(etspi_class);
	}

	status = spi_register_driver(&etspi_spi_driver);
	if (status < 0) {
		pr_err("%s spi_register_driver error.\n", __func__);
		class_destroy(etspi_class);
		unregister_chrdev(ET7XX_MAJOR, etspi_spi_driver.driver.name);
		return status;
	}

	pr_info("%s is successful\n", __func__);

	return status;
}

static void __exit etspi_exit(void)
{
	pr_info("%s\n", __func__);

	spi_unregister_driver(&etspi_spi_driver);
	class_destroy(etspi_class);
	unregister_chrdev(ET7XX_MAJOR, etspi_spi_driver.driver.name);
}

module_init(etspi_init);
module_exit(etspi_exit);

MODULE_AUTHOR("Wang YuWei, <robert.wang@egistec.com>");
MODULE_DESCRIPTION("SPI Interface for ET7XX");
MODULE_LICENSE("GPL");
