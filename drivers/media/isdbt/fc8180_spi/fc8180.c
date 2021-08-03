/*****************************************************************************
	Copyright(c) 2013 FCI Inc. All Rights Reserved

	File name : fc8180.c

	Description : Driver source file

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	History :
	----------------------------------------------------------------------
*******************************************************************************/
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/io.h>

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include "fc8180.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fc8180_regs.h"
#include "fc8180_isr.h"
#include "fci_hal.h"
#include "isdbt_tuner_pdata.h"

struct ISDBT_INIT_INFO_T *hInit;
static struct wake_lock isdbt_wlock;

int bbm_xtal_freq;
unsigned int fc8300_xtal_freq;
#define RING_BUFFER_SIZE	(188 * 32 * 50)

/* GPIO(RESET & INTRRUPT) Setting */
#define FC8180_NAME		"isdbt"
static struct isdbt_platform_data *isdbt_pdata;

#define TS0_32PKT_LENGTH (188 * 37)	/* compatible pkt rate: 5, 37, 97, 177, 277 */

#define ISDBT_LDO_ON      1
#define ISDBT_LDO_OFF     0


u8 static_ringbuffer[RING_BUFFER_SIZE];

enum ISDBT_MODE driver_mode = ISDBT_POWEROFF;
static DEFINE_MUTEX(ringbuffer_lock);

static DECLARE_WAIT_QUEUE_HEAD(isdbt_isr_wait);


#ifndef BBM_I2C_TSIF
static u8 isdbt_isr_sig;
static struct task_struct *isdbt_kthread;

static irqreturn_t isdbt_irq(int irq, void *dev_id)
{
	isdbt_isr_sig = 1;
	wake_up_interruptible(&isdbt_isr_wait);
	return IRQ_HANDLED;
}
#endif

int isdbt_hw_setting(void)
{
	int err;
	pr_err("%s\n", __func__);

	err = gpio_request(isdbt_pdata->gpio_en, "isdbt_en");
	if (err) {
		pr_err("isdbt_hw_setting: Couldn't request isdbt_en err=%d\n", err);
		goto ISBT_EN_ERR;
	}
	gpio_direction_output(isdbt_pdata->gpio_en, 0);

#ifndef BBM_I2C_TSIF
	err =	gpio_request(isdbt_pdata->gpio_int, "isdbt_irq");
	if (err) {
		pr_err("isdbt_hw_setting: Couldn't request isdbt_irq\n");
		goto ISDBT_INT_ERR;
	}

	gpio_direction_input(isdbt_pdata->gpio_int);

	err = request_irq(gpio_to_irq(isdbt_pdata->gpio_int), isdbt_irq
		, IRQF_DISABLED | IRQF_TRIGGER_FALLING, FC8180_NAME, NULL);

	if (err < 0) {
		print_log(0, "isdbt_hw_setting:	couldn't request gpio interrupt %d reason(%d)\n"
			, gpio_to_irq(isdbt_pdata->gpio_int), err);
	goto request_isdbt_irq;
	}
#endif

	return 0;
#ifndef BBM_I2C_TSIF
request_isdbt_irq:
	gpio_free(isdbt_pdata->gpio_int);
ISDBT_INT_ERR:
	gpio_free(isdbt_pdata->gpio_en);
#endif

ISBT_EN_ERR:

	return err;
}


static void isdbt_gpio_init(void)
{
	pr_err("%s\n", __func__);

/*
	if (pinctrl_select_state(isdbt_pdata->isdb_pinctrl, isdbt_pdata->pwr_on)) {
		pr_err("%s: Failed to configure isdb_on\n", __func__);
		gpio_free(isdbt_pdata->gpio_en);
		gpio_free(isdbt_pdata->gpio_rst);
		if (isdbt_pdata->gpio_int)
			gpio_free(isdbt_pdata->gpio_int);
	}
*/

	isdbt_hw_setting();
}

static void isdbt_regulator_onoff(int onoff)
{
	int rc = 0;
	struct regulator *regulator_vdd_1p8;

	if (isdbt_pdata->ldo_vdd_1p8 == NULL) {
		pr_err("%s - ldo_vdd_1p8 regulator is not present. Hence ignoring the setting \n", __func__);
		return;
	}

	if (isdbt_pdata->regulator_is_enable == onoff) {
		pr_err("ISDBT duplicate regulator setting. Already in %s state", onoff ? "ON" : "OFF");
		return;
	}

	regulator_vdd_1p8 = regulator_get(NULL, isdbt_pdata->ldo_vdd_1p8);
	if (IS_ERR(regulator_vdd_1p8) || regulator_vdd_1p8 == NULL) {
		pr_err("%s - ldo_vdd_1p8 regulator_get fail\n", __func__);
		return;
	}

	pr_info("%s - onoff = %d\n", __func__, onoff);

	if (onoff == ISDBT_LDO_ON) {

		/* voltage setting is not allowed for LDO 23
				rc = regulator_set_voltage(regulator_vdd_1p8, 1800000, 1800000);
				if (rc < 0) {
						pr_err("%s - set 1p8v failed, rc=%d\n",
								__func__, rc);
						goto done;
				}
		*/
		rc = regulator_enable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - enable vdd_1p8 failed, rc=%d\n",
					__func__, rc);
			goto done;
		}

	} else {
		rc = regulator_disable(regulator_vdd_1p8);
		if (rc) {
			pr_err("%s - disable vdd_1p8 failed, rc=%d\n",
					__func__, rc);
			goto done;
		}
	}

	isdbt_pdata->regulator_is_enable = (u8)onoff;

done:
	regulator_put(regulator_vdd_1p8);

	return;

}


/*POWER_ON & HW_RESET & INTERRUPT_CLEAR */
void isdbt_hw_init(void)
{
	int i = 0;

	isdbt_regulator_onoff(ISDBT_LDO_ON);

#ifdef CONFIG_ISDBT_GPIO_CLK
	pinctrl_select_state(isdbt_pdata->isdbt_pinctrl, isdbt_pdata->isdbt_on);
#else
	clk_prepare_enable(isdbt_pdata->isdbt_clk);
	pr_err("%s, Enabling ISDBT_CLK\n", __func__);
#endif

	while (driver_mode == ISDBT_DATAREAD) {
		ms_wait(100);
		if (i++ > 5)
			break;
	}

	pr_err("%s\n", __func__);


	gpio_direction_output(isdbt_pdata->gpio_en, 1);
	pr_err("%s, gpio_en =%d\n", __func__, gpio_get_value(isdbt_pdata->gpio_en));

	mdelay(30);

	driver_mode = ISDBT_POWERON;

}

/*POWER_OFF */
void isdbt_hw_deinit(void)
{
#ifdef CONFIG_ISDBT_GPIO_CLK
	pinctrl_select_state(isdbt_pdata->isdbt_pinctrl, isdbt_pdata->isdbt_off);
#else
	clk_disable_unprepare(isdbt_pdata->isdbt_clk);
	pr_err("%s, Turning ISDBT_CLK off\n", __func__);
#endif
	driver_mode = ISDBT_POWEROFF;
	gpio_direction_output(isdbt_pdata->gpio_en, 0);
	mdelay(5);
	isdbt_regulator_onoff(ISDBT_LDO_OFF);
}

int data_callback(ulong hDevice, u8 *data, int len)
{
	struct ISDBT_INIT_INFO_T *hInit;
	struct list_head *temp;
	hInit = (struct ISDBT_INIT_INFO_T *)hDevice;

	list_for_each(temp, &(hInit->hHead))
	{
		struct ISDBT_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, struct ISDBT_OPEN_INFO_T, hList);

		if (hOpen->isdbttype == TS_TYPE) {
			mutex_lock(&ringbuffer_lock);
			if (fci_ringbuffer_free(&hOpen->RingBuffer) < len) {
				/* return 0 */;
				FCI_RINGBUFFER_SKIP(&hOpen->RingBuffer, len);
			}
			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);

			wake_up_interruptible(&(hOpen->RingBuffer.queue));

			mutex_unlock(&ringbuffer_lock);
		}
	}

	return 0;
}


#ifndef BBM_I2C_TSIF
static int isdbt_thread(void *hDevice)
{
	struct ISDBT_INIT_INFO_T *hInit = (struct ISDBT_INIT_INFO_T *)hDevice;

	set_user_nice(current, -20);

	pr_err("isdbt_kthread enter\n");

	bbm_com_ts_callback_register((ulong)hInit, data_callback);

	while (1) {
		wait_event_interruptible(isdbt_isr_wait,
			isdbt_isr_sig || kthread_should_stop());

		if (driver_mode == ISDBT_POWERON) {
			driver_mode = ISDBT_DATAREAD;
			bbm_com_isr(hInit);
			driver_mode = ISDBT_POWERON;
		}

		isdbt_isr_sig = 0;

		if (kthread_should_stop())
			break;
	}

	bbm_com_ts_callback_deregister();

	pr_err("isdbt_kthread exit\n");

	return 0;
}
#endif

const struct file_operations isdbt_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl		= isdbt_ioctl,
	.open		= isdbt_open,
	.read		= isdbt_read,
	.release	= isdbt_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = isdbt_ioctl,
#endif
};

static struct miscdevice fc8180_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = FC8180_NAME,
	.fops = &isdbt_fops,
};

int isdbt_open(struct inode *inode, struct file *filp)
{
	struct ISDBT_OPEN_INFO_T *hOpen;

	pr_err("isdbt open\n");
	hOpen = kmalloc(sizeof(struct ISDBT_OPEN_INFO_T), GFP_KERNEL);
	if (!hOpen)	{
		pr_err("ISDBT hOpen malloc failed ENOMEM\n");
		return -ENOMEM;
	}
	hOpen->buf = &static_ringbuffer[0];
	/*kmalloc(RING_BUFFER_SIZE, GFP_KERNEL);*/
	hOpen->isdbttype = 0;

	list_add(&(hOpen->hList), &(hInit->hHead));

	hOpen->hInit = (HANDLE *)hInit;

	if (hOpen->buf == NULL) {
		pr_err("ISDBT ring buffer malloc error\n");
		return -ENOMEM;
	}

	fci_ringbuffer_init(&hOpen->RingBuffer, hOpen->buf, RING_BUFFER_SIZE);

	filp->private_data = hOpen;

	wake_lock(&isdbt_wlock);

	return 0;
}

ssize_t isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	s32 avail;
	s32 non_blocking = filp->f_flags & O_NONBLOCK;
	struct ISDBT_OPEN_INFO_T *hOpen
		= (struct ISDBT_OPEN_INFO_T *)filp->private_data;
	struct fci_ringbuffer *cibuf = &hOpen->RingBuffer;
	ssize_t len, read_len = 0;

	if (!cibuf->data || !count)	{
		/*pr_err(" return 0\n"); */
		return 0;
	}

	if (non_blocking && (fci_ringbuffer_empty(cibuf)))	{
		/*pr_err("return EWOULDBLOCK\n"); */
		return -EWOULDBLOCK;
	}

	if (wait_event_interruptible(cibuf->queue,
		!fci_ringbuffer_empty(cibuf))) {
		pr_err("return ERESTARTSYS\n");
		return -ERESTARTSYS;
	}

	mutex_lock(&ringbuffer_lock);

	avail = fci_ringbuffer_avail(cibuf);

	if (count >= avail)
		len = avail;
	else
		len = count - (count % 188);

	read_len = fci_ringbuffer_read_user(cibuf, buf, len);

	mutex_unlock(&ringbuffer_lock);

	return read_len;
}

int isdbt_release(struct inode *inode, struct file *filp)
{

	struct ISDBT_OPEN_INFO_T *hOpen;
	pr_err("isdbt_release\n");
	hOpen = filp->private_data;

	if (hOpen != NULL)	{
		hOpen->isdbttype = 0;
		list_del(&(hOpen->hList));
		pr_err("isdbt_release hList\n");
		/*kfree(hOpen->buf);*/
		kfree(hOpen);
	}

	if (isdbt_pdata->regulator_is_enable)
		isdbt_regulator_onoff(ISDBT_LDO_OFF);

	wake_unlock(&isdbt_wlock);

	return 0;
}


#ifndef BBM_I2C_TSIF
void isdbt_isr_check(HANDLE hDevice)
{
	u8 isr_time = 0;

	bbm_com_write(hDevice, BBM_BUF_ENABLE, 0x00);

	while (isr_time < 10) {
		if (!isdbt_isr_sig)
			break;

		ms_wait(10);
		isr_time++;
	}

}
#endif

static ssize_t isdbt_ber_show(struct class *dev,
                struct class_attribute *attr, char *buf)
{
	int type = 0;
	sprintf(buf, "%d,%d", type, isdbt_pdata->BER);
	pr_info("%s, type:%d, ber:%d\n", __func__, type, isdbt_pdata->BER);
	return strlen(buf);
}
static CLASS_ATTR(ber, 0444,
                isdbt_ber_show, NULL);

long isdbt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 res = BBM_NOK;
	s32 err = 0;
	s32 size = 0;
	struct ISDBT_OPEN_INFO_T *hOpen;

	struct ioctl_info info;

	if (_IOC_TYPE(cmd) != IOCTL_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) >= IOCTL_MAXNR)
		return -EINVAL;

	hOpen = filp->private_data;

	size = _IOC_SIZE(cmd);

	switch (cmd) {
	case IOCTL_ISDBT_RESET:
		res = bbm_com_reset(hInit);
		pr_err("[FC8180] IOCTL_ISDBT_RESET\n");
		break;
	case IOCTL_ISDBT_INIT:
		pr_err("[FC8180] IOCTL_ISDBT_INIT\n");
#ifdef BBM_SPI_IF
		bbm_com_byte_write(hInit, BBM_DM_DATA, 0x00);
#endif
		res = bbm_com_i2c_init(hInit, FCI_HPI_TYPE);
		pr_err("[FC8180] IOCTL_ISDBT_INIT bbm_com_i2c_init res =%d\n", res);
		res |= bbm_com_probe(hInit);
		if (res) {
			pr_err("FC8180 Initialize Fail\n");
			break;
		}
		pr_err("[FC8180] IOCTL_ISDBT_INIT bbm_com_probe success\n");
		res = bbm_com_init(hInit);
		pr_err("[FC8180] IOCTL_ISDBT_INITbbm_com_init\n");
		res |= bbm_com_tuner_select(hInit, FC8180_TUNER, 0);
		break;
	case IOCTL_ISDBT_BYTE_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_read(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_WORD_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_read(hInit, (u16)info.buff[0]
			, (u16 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_LONG_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_read(hInit, (u16)info.buff[0]
			, (u32 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_BULK_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_bulk_read(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_BYTE_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_write(hInit, (u16)info.buff[0]
			, (u8)info.buff[1]);
		break;
	case IOCTL_ISDBT_WORD_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_write(hInit, (u16)info.buff[0]
			, (u16)info.buff[1]);
		break;
	case IOCTL_ISDBT_LONG_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_write(hInit, (u16)info.buff[0]
			, (u32)info.buff[1]);
		break;
	case IOCTL_ISDBT_BULK_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_bulk_write(hInit, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		break;
	case IOCTL_ISDBT_TUNER_READ:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_read(hInit, (u8)info.buff[0]
			, (u8)info.buff[1],  (u8 *)(&info.buff[3])
			, (u8)info.buff[2]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_TUNER_WRITE:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_write(hInit, (u8)info.buff[0]
			, (u8)info.buff[1], (u8 *)(&info.buff[3])
			, (u8)info.buff[2]);
		break;
	case IOCTL_ISDBT_TUNER_SET_FREQ:
		{
			u32 f_rf;
			err = copy_from_user((void *)&info, (void *)arg, size);
			f_rf = (u32)info.buff[0];
			pr_err("[FC8180] IOCTL_ISDBT_TUNER_SET_FREQ freq=%d\n", f_rf);
#ifndef BBM_I2C_TSIF
			isdbt_isr_check(hInit);
#endif
			res = bbm_com_tuner_set_freq(hInit, f_rf);
#ifndef BBM_I2C_TSIF
			mutex_lock(&ringbuffer_lock);
			fci_ringbuffer_flush(&hOpen->RingBuffer);
			mutex_unlock(&ringbuffer_lock);
			bbm_com_write(hInit, BBM_BUF_ENABLE, 0x01);
#endif
		}
		break;
	case IOCTL_ISDBT_TUNER_SELECT:
		pr_err("[FC8180] IOCTL_ISDBT_TUNER_SELECT\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_select(hInit
			, (u32)info.buff[0], (u32)info.buff[1]);
		bbm_com_byte_write(hInit, BBM_BUF_ENABLE, 0x00);
		bbm_com_word_write(hInit, BBM_BUF_TS_END, (TS0_32PKT_LENGTH * 2) - 1);
		bbm_com_word_write(hInit, BBM_BUF_TS_THR, (TS0_32PKT_LENGTH - 1));
		bbm_com_byte_write(hInit, BBM_BUF_ENABLE, 0x01);
		print_log(hInit, "[FC8180] IOCTL_ISDBT_TUNER_SELECT %d\n"
		, (u32)info.buff[1]);
		break;
	case IOCTL_ISDBT_RF_BER:
		err = copy_from_user((void *)&info, (void *)arg, size);
		pr_err("[FC8300] IOCTL_ISDBT_RF_BER, CN(%d), BER_A(%d), BER_B(%d)\n",
			(u8)info.buff[0], (u32)info.buff[1], (u32)info.buff[2]);
		isdbt_pdata->BER = (int)info.buff[1];
		res = 0;
		break;
	case IOCTL_ISDBT_TS_START:
		pr_err("[FC8180] IOCTL_ISDBT_TS_START\n");
		hOpen->isdbttype = TS_TYPE;

		break;
	case IOCTL_ISDBT_TS_STOP:
		pr_err("[FC8180] IOCTL_ISDBT_TS_STOP\n");
		hOpen->isdbttype = 0;

		break;
	case IOCTL_ISDBT_POWER_ON:
		pr_err("[FC8180] IOCTL_ISDBT_POWER_ON\n");

		isdbt_hw_init();
#ifdef BBM_SPI_IF
		bbm_com_byte_write(hInit, BBM_DM_DATA, 0x00);
#endif
		res = bbm_com_i2c_init(hInit, FCI_HPI_TYPE);
		pr_err("[FC8180] IOCTL_ISDBT_POWER_ON bbm_com_i2c_init res =%d\n", res);
		res |= bbm_com_probe(hInit);
		if (res) {
			pr_err("FC8180 Initialize Fail\n");
			isdbt_hw_deinit();
		} else {
			pr_err("FC8180 IOCTL_ISDBT_POWER_ON SUCCESS\n");
		}
		break;
	case IOCTL_ISDBT_POWER_OFF:
		pr_err("[FC8180] IOCTL_ISDBT_POWER_OFF\n");
		isdbt_hw_deinit();

		break;
	case IOCTL_ISDBT_SCAN_STATUS:
		res = bbm_com_scan_status(hInit);
		pr_err("[FC8180] IOCTL_ISDBT_SCAN_STATUS : %d\n", res);
		break;
	case IOCTL_ISDBT_TUNER_GET_RSSI:
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_get_rssi(hInit, (s32 *)&info.buff[0]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;

	default:
		pr_err("isdbt ioctl error!\n");
		res = BBM_NOK;
		break;
	}

	if (err < 0) {
		pr_err("copy to/from user fail : %d", err);
		res = BBM_NOK;
	}
	return res;
}


static struct isdbt_platform_data *isdbt_populate_dt_pdata(struct device *dev)
{
	struct isdbt_platform_data *pdata;
#ifndef CONFIG_ISDBT_GPIO_CLK
	const char *temp_string = NULL;
	int ret = 0;
#endif
	pr_err("%s\n", __func__);
	pdata =  devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s : could not allocate memory for platform data\n", __func__);
		goto err;
	}

	of_property_read_u32(dev->of_node, "isdbt,isdb-bbm-xtal-freq", &bbm_xtal_freq);
	if (bbm_xtal_freq < 0)	{
		pr_err("%s : can not find the isdbt-bbmxtal-freq in the dt, set to : 26000\n", __func__);
		bbm_xtal_freq = 26000;
	}

	pdata->gpio_en = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-pwr-en", 0);
	if (pdata->gpio_en < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-pwr-en", &pdata->gpio_en);
	if (pdata->gpio_en < 0)	{
		pr_err("%s : can not find the isdbt-detect-gpio gpio_en in the dt\n", __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_en =%d\n", __func__, pdata->gpio_en);
	}

	pdata->gpio_cp_dt = of_get_named_gpio(dev->of_node, "isdbt,isdb-cp-detect", 0);
	if (pdata->gpio_cp_dt < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-cp-detect", &pdata->gpio_cp_dt);
	if (pdata->gpio_cp_dt < 0)
		pr_err("%s : can not find the isdb-cp-detect gpio_cp_dt in the dt\n", __func__);
	else
		pr_err("%s : isdbt-detect-gpio gpio_cp_dt =%d\n", __func__, pdata->gpio_cp_dt);
/*
	pdata->gpio_rst = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-rst", 0);
	if (pdata->gpio_rst < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-rst", &pdata->gpio_rst);
	if (pdata->gpio_rst < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_rst in the dt\n", __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_rst =%d\n", __func__, pdata->gpio_rst);
	}

	pdata->isdb_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pdata->isdb_pinctrl)) {
		pr_err("devm_pinctrl_get is fail");
		goto alloc_err;
	}
	pdata->pwr_on = pinctrl_lookup_state(pdata->isdb_pinctrl, "isdb_on");
	if(IS_ERR(pdata->pwr_on)) {
		pr_err("%s : could not get pins isdb_on state (%li)\n",
			__func__, PTR_ERR(pdata->pwr_on));
		goto err_pinctrl_lookup_state;
	}
	pdata->gpio_init = pinctrl_lookup_state(pdata->isdb_pinctrl, "isdb_gpio_init");
	if(IS_ERR(pdata->gpio_init)) {
		pr_err("%s : could not get pins isdb_gpio_init state (%li)\n",
			__func__, PTR_ERR(pdata->pwr_off));
		goto err_pinctrl_lookup_state;
	}
	pdata->pwr_off = pinctrl_lookup_state(pdata->isdb_pinctrl, "isdb_off");
	if(IS_ERR(pdata->pwr_off)) {
		pr_err("%s : could not get pins isdb_off state (%li)\n",
			__func__, PTR_ERR(pdata->pwr_off));
		goto err_pinctrl_lookup_state;
	}
*/
#ifndef BBM_I2C_TSIF
	pdata->gpio_int = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-irq", 0);
	if (pdata->gpio_int < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-irq", &pdata->gpio_int);
	if (pdata->gpio_int < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio in the gpio_int dt\n", __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_int =%d\n", __func__, pdata->gpio_int);
	}
#endif
#ifndef CONFIG_ISDBT_FC8180_SPI
	pdata->gpio_i2c_sda = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-i2c_sda", 0);
	if (pdata->gpio_i2c_sda < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-i2c_sda", &pdata->gpio_i2c_sda);
	if (pdata->gpio_i2c_sda < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_i2c_sda in the dt\n", __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_i2c_sda=%d\n", __func__, pdata->gpio_i2c_sda);
	}

	pdata->gpio_i2c_scl = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-i2c_scl", 0);
	if (pdata->gpio_i2c_scl < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-i2c_scl", &pdata->gpio_i2c_scl);
	if (pdata->gpio_i2c_scl < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_i2c_scl in the dt\n", __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_i2c_scl=%d\n", __func__, pdata->gpio_i2c_scl);
	}
#endif
	pdata->gpio_spi_do = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-spi_do", 0);
	if (pdata->gpio_spi_do < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio in the gpio_spi_do dt\n", __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_do =%d\n", __func__, pdata->gpio_spi_do);

	pdata->gpio_spi_di = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-spi_di", 0);
	if (pdata->gpio_spi_di < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio in the gpio_spi_di dt\n", __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_di =%d\n", __func__, pdata->gpio_spi_di);

	pdata->gpio_spi_cs = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-spi_cs", 0);
	if (pdata->gpio_spi_cs < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_spi_cs in the dt\n", __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_cs=%d\n", __func__, pdata->gpio_spi_cs);

	pdata->gpio_spi_clk = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-spi_clk", 0);
	if (pdata->gpio_spi_clk < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_spi_clk in the dt\n", __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_clk=%d\n", __func__, pdata->gpio_spi_clk);

#ifdef CONFIG_ISDBT_GPIO_CLK
	pdata->gpio_clk = of_get_named_gpio(dev->of_node, "isdbt,isdb-gpio-clk", 0);
	if (pdata->gpio_clk < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_clk in the dt\n", __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_clk = %d\n", __func__, pdata->gpio_clk);
	}
#else
	ret = of_property_read_string(dev->of_node, "clock-names", &temp_string);
	if (ret)
		pr_err("isdbt : %s: cannot get clock name(%d)", __func__, ret);

	pdata->isdbt_clk = clk_get(dev, temp_string);
	if (pdata->isdbt_clk < 0)
		pr_err("isdbt : %s: cannot get clock", __func__);
#endif
	if (of_property_read_string(dev->of_node, "isdbt,ldo_vdd_1p8",
		&pdata->ldo_vdd_1p8) < 0)
		pr_err("%s - get ldo_vdd_1p8 error\n", __func__);


	return pdata;
alloc_err:
	devm_kfree(dev, pdata);
err:
	return NULL;
}

#ifdef CONFIG_ISDBT_GPIO_CLK
static int isdbt_pinctrl(struct device *dev,
						struct isdbt_platform_data *pdata)
{
	int ret = 0;

	/* get the pinctrl handler for the device */
	pdata->isdbt_pinctrl = devm_pinctrl_get(dev);

	if (IS_ERR(pdata->isdbt_pinctrl)) {
		pr_err(" fc8180 does not use pinctrl\n");
		pdata->isdbt_pinctrl = NULL;
	} else {

		/* Get the pinctrl for the Active & Suspend states */
		pdata->isdbt_off = pinctrl_lookup_state(pdata->isdbt_pinctrl,
											"isdbt_off");
		if (IS_ERR(pdata->isdbt_off)) {
			pr_info("%s fail due to isdbt_off state not found\n", __func__);
			goto err_exit;
		}

		pdata->isdbt_on = pinctrl_lookup_state(pdata->isdbt_pinctrl,
											"isdbt_on");
		if (IS_ERR(pdata->isdbt_on)) {
			pr_info("%s fail due to isdbt_on state not found\n", __func__);
			goto err_exit;
		}

		/* Need not call the devm_pinctrl_put() as the handler will be
		automatically freed  when the device is removed */
	}

err_exit:
	return ret;
}
#endif

static int isdbt_probe(struct platform_device *pdev)
{
	int res = 0;
	static struct class *isdbt_class;

	pr_err("%s\n", __func__);

	isdbt_pdata = isdbt_populate_dt_pdata(&pdev->dev);
	if (!isdbt_pdata) {
		pr_err("%s : isdbt_pdata is NULL.\n", __func__);
		return -ENODEV;
	}
#ifdef CONFIG_ISDBT_GPIO_CLK
	res = isdbt_pinctrl(&pdev->dev, isdbt_pdata);
	pinctrl_select_state(isdbt_pdata->isdbt_pinctrl, isdbt_pdata->isdbt_off);
#endif
	isdbt_gpio_init();
	fc8300_xtal_freq = bbm_xtal_freq;

	res = misc_register(&fc8180_misc_device);

	if (res < 0) {
		pr_err("isdbt init fail : %d\n", res);
		return res;
	}

	hInit = kmalloc(sizeof(struct ISDBT_INIT_INFO_T), GFP_KERNEL);

#if defined(BBM_I2C_TSIF) || defined(BBM_I2C_SPI)
	res = bbm_com_hostif_select(hInit, BBM_I2C);
	pr_err("isdbt host interface select BBM_I2C!\n");
#else
	pr_err("isdbt host interface select BBM_SPI !\n");
	res = bbm_com_hostif_select(hInit, BBM_SPI);
#endif

	if (res)
		pr_err("isdbt host interface select fail!\n");

#ifndef BBM_I2C_TSIF
	if (!isdbt_kthread)	{
		pr_err("kthread run\n");
		isdbt_kthread = kthread_run(isdbt_thread
			, (void *)hInit, "isdbt_thread");
	}
#endif


	INIT_LIST_HEAD(&(hInit->hHead));

	isdbt_class = class_create(THIS_MODULE, "isdbt");
	if (IS_ERR(isdbt_class)) {
		pr_err("%s : class_create failed!\n", __func__);
	} else {
		res = class_create_file(isdbt_class, &class_attr_ber);
		if (res)
		pr_err("%s : failed to create device file in sysfs entries!\n", __func__);
	}
	wake_lock_init(&isdbt_wlock, WAKE_LOCK_SUSPEND, "isdbt_wlock");

	return 0;
}
static int isdbt_remove(struct platform_device *pdev)
{
	pr_err("ISDBT remove\n");
	wake_lock_destroy(&isdbt_wlock);
	return 0;
}

static int isdbt_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	int value;

	value = gpio_get_value_cansleep(isdbt_pdata->gpio_en);

	pr_err("%s  value = %d\n", __func__, value);
	if (value == 1)
		gpio_direction_output(isdbt_pdata->gpio_en, 0);

	return 0;
}

static int isdbt_resume(struct platform_device *pdev)
{
	return 0;
}


static const struct of_device_id isdbt_match_table[] = {
	{   .compatible = "isdb_fc8300_pdata",
	},
	{}
};

static struct platform_driver isdb_fc8180_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name = "isdbt",
		.of_match_table = isdbt_match_table,
	},
	.probe	= isdbt_probe,
	.remove = isdbt_remove,
	.suspend = isdbt_suspend,
	.resume = isdbt_resume,
};

int isdbt_init(void)
{
	s32 res;

	pr_err("isdbt_fc8180_init started\n");

	res = platform_driver_register(&isdb_fc8180_driver);
	if (res < 0) {
		pr_err("isdbt init fail : %d\n", res);
		return res;
	}

	return 0;
}

void isdbt_exit(void)
{
	pr_err("isdb_fc8300_exit\n");


#ifndef BBM_I2C_TSIF
	free_irq(gpio_to_irq(isdbt_pdata->gpio_int), NULL);
	gpio_free(isdbt_pdata->gpio_int);
#endif
	gpio_free(isdbt_pdata->gpio_en);

#ifndef BBM_I2C_TSIF
	if (isdbt_kthread)
		kthread_stop(isdbt_kthread);

	isdbt_kthread = NULL;
#endif

	bbm_com_hostif_deselect(hInit);

	isdbt_hw_deinit();
	platform_driver_unregister(&isdb_fc8180_driver);
	misc_deregister(&fc8180_misc_device);

	if (hInit != NULL)
		kfree(hInit);

}

module_init(isdbt_init);
module_exit(isdbt_exit);

MODULE_LICENSE("Dual BSD/GPL");

