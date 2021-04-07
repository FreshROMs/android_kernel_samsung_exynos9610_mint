/*
 * Copyright (C) 2012-2014 NXP Semiconductors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/spi/spi.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/wakelock.h>
#include "p61.h"
#include "pn547.h"
#ifdef CONFIG_ESE_SECURE
#include "../misc/tzdev/include/tzdev/tee_client_api.h"
#endif

extern long  pn547_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg);

#define P61_SPI_CLOCK     7000000L

/* size of maximum read/write buffer supported by driver */
#define MAX_BUFFER_SIZE   258U

/* Different driver debug lever */
enum P61_DEBUG_LEVEL {
	P61_DEBUG_OFF,
	P61_FULL_DEBUG
};

/* Variable to store current debug level request by ioctl */
static unsigned char debug_level = P61_FULL_DEBUG;
static unsigned char pwr_req_on;
#define P61_DBG_MSG(msg...) {\
	switch (debug_level) {\
	case P61_DEBUG_OFF:\
		break;\
	case P61_FULL_DEBUG:\
		pr_info("[NXP-P61] " msg);\
		break;\
		/*fallthrough*/\
	default:\
		pr_err("[NXP-P61] Wrong debug level(%d)\n", debug_level);\
		break;\
	} \
}

#define P61_ERR_MSG(msg...) pr_err("[NXP-P61] " msg)
#define P61_INFO_MSG(msg...) pr_info("[NXP-P61] " msg)

#ifdef CONFIG_ESE_SECURE
static TEEC_UUID ese_drv_uuid = {
	0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x65, 0x73, 0x65, 0x44, 0x72, 0x76}
};

enum pm_mode {
	PM_SUSPEND,
	PM_RESUME,
	SECURE_CHECK,
};

enum secure_state {
	NOT_CHECKED,
	ESE_SECURED,
	ESE_NOT_SECURED,
};
#endif

/* Device specific macro and structure */
struct p61_device {
	wait_queue_head_t read_wq; /* wait queue for read interrupt */
	struct mutex read_mutex; /* read mutex */
	struct mutex write_mutex; /* write mutex */
	struct spi_device *spi;  /* spi device structure */
	struct miscdevice miscdev; /* char device as misc driver */
	unsigned int rst_gpio; /* SW Reset gpio */
	unsigned int irq_gpio; /* P61 will interrupt DH for any ntf */
	bool irq_enabled; /* flag to indicate irq is used */
	unsigned char enable_poll_mode; /* enable the poll mode */
	spinlock_t irq_enabled_lock; /*spin lock for read irq */

	bool tz_mode;
	spinlock_t ese_spi_lock;
	bool isGpio_cfgDone;
	struct wake_lock ese_lock;
	bool device_opened;

	struct pinctrl *pinctrl;
	struct pinctrl_state *ese_on_pin;
	struct pinctrl_state *ese_off_pin;

#ifdef CONFIG_ESE_SECURE
	struct clk *ese_spi_pclk;
	struct clk *ese_spi_sclk;
	int ese_secure_check;
#endif
	const char *ap_vendor;
	unsigned char *buf;
};
static struct p61_device *p61_dev;

/* T==1 protocol specific global data */
const unsigned char SOF = 0xA5u;

#ifdef CONFIG_ESE_SECURE
/**
 * p61_spi_clk_max_rate: finds the nearest lower rate for a clk
 * @clk the clock for which to find nearest lower rate
 * @rate clock frequency in Hz
 * @return nearest lower rate or negative error value
 *
 * Public clock API extends clk_round_rate which is a ceiling function. This
 * function is a floor function implemented as a binary search using the
 * ceiling function.
 */
static long p61_spi_clk_max_rate(struct clk *clk, unsigned long rate)
{
	long lowest_available, nearest_low, step_size, cur;
	long step_direction = -1;
	long guess = rate;
	int  max_steps = 10;

	cur =  clk_round_rate(clk, rate);
	if (cur == rate)
		return rate;

	/* if we got here then: cur > rate */
	lowest_available =  clk_round_rate(clk, 0);
	if (lowest_available > rate)
		return -EINVAL;

	step_size = (rate - lowest_available) >> 1;
	nearest_low = lowest_available;

	while (max_steps-- && step_size) {
		guess += step_size * step_direction;

		cur =  clk_round_rate(clk, guess);

		if ((cur < rate) && (cur > nearest_low))
			nearest_low = cur;

		/*
		 * if we stepped too far, then start stepping in the other
		 * direction with half the step size
		 */
		if (((cur > rate) && (step_direction > 0))
		 || ((cur < rate) && (step_direction < 0))) {
			step_direction = -step_direction;
			step_size >>= 1;
		}
	}
	return nearest_low;
}

static void p61_spi_clock_set(struct p61_device *p61_dev, unsigned long speed)
{
	long rate;

	if (!strcmp(p61_dev->ap_vendor, "qualcomm")) {
		/* finds the nearest lower rate for a clk */
		rate = p61_spi_clk_max_rate(p61_dev->ese_spi_sclk, speed);
		if (rate < 0) {
			pr_err("%s: no match found for requested clock: %lu\n",
				__func__, speed);
			return;
		}
		speed = rate;
		/*pr_info("%s speed:%lu\n", __func__, speed);*/
	} else if (!strcmp(p61_dev->ap_vendor, "slsi")) {
		/* There is half-multiplier */
		speed =  speed * 4;
	}

	clk_set_rate(p61_dev->ese_spi_sclk, speed);
}

static int p61_clk_control(struct p61_device *p61_dev, bool onoff)
{
	static bool old_value;

	if (old_value == onoff) {
		pr_info("%s: ALREADY %s\n", __func__,
			onoff ? "enabled" : "disabled");
		return 0;
	}

	if (onoff == true) {
		/* For slsi AP, clk enable should be run before clk set */
		clk_prepare_enable(p61_dev->ese_spi_pclk);
		clk_prepare_enable(p61_dev->ese_spi_sclk);
		p61_spi_clock_set(p61_dev, P61_SPI_CLOCK);
		usleep_range(5000, 5100);
		P61_INFO_MSG("%s: clock: %lu(%lu)\n", __func__, P61_SPI_CLOCK,
			clk_get_rate(p61_dev->ese_spi_sclk));
	} else {
		clk_disable_unprepare(p61_dev->ese_spi_pclk);
		clk_disable_unprepare(p61_dev->ese_spi_sclk);
	}
	old_value = onoff;

	pr_info("%s: clock %s\n", __func__, onoff ? "enabled" : "disabled");
	return 0;
}

static int p61_clk_setup(struct device *dev, struct p61_device *p61_dev)
{
	p61_dev->ese_spi_pclk = clk_get(dev, "pclk");
	if (IS_ERR(p61_dev->ese_spi_pclk)) {
		pr_err("%s: Can't get %s\n", __func__, "pclk");
		p61_dev->ese_spi_pclk = NULL;
		goto err_pclk_get;
	}

	p61_dev->ese_spi_sclk = clk_get(dev, "sclk");
	if (IS_ERR(p61_dev->ese_spi_sclk)) {
		pr_err("%s: Can't get %s\n", __func__, "sclk");
		p61_dev->ese_spi_sclk = NULL;
		goto err_sclk_get;
	}

	return 0;
err_sclk_get:
	clk_put(p61_dev->ese_spi_pclk);
err_pclk_get:
	return -EPERM;
}

static uint32_t tz_tee_ese_drv(enum pm_mode mode)
{
	TEEC_Context context;
	TEEC_Session session;
	TEEC_Result result;
	uint32_t returnOrigin = TEEC_NONE;

	result = TEEC_InitializeContext(NULL, &context);
	if (result != TEEC_SUCCESS)
		goto out;

	result = TEEC_OpenSession(&context, &session, &ese_drv_uuid, TEEC_LOGIN_PUBLIC,
			NULL, NULL, &returnOrigin);
	if (result != TEEC_SUCCESS)
		goto finalize_context;

	/* test with valid cmd id, expected result : TEEC_SUCCESS */
	result = TEEC_InvokeCommand(&session, mode, NULL, &returnOrigin);
	if (result != TEEC_SUCCESS) {
		P61_ERR_MSG("%s with cmd %d : FAIL\n", __func__, mode);
		goto close_session;
	}

	P61_ERR_MSG("eSE tz_tee_dev return origin %d\n", returnOrigin);

close_session:
	TEEC_CloseSession(&session);
finalize_context:
	TEEC_FinalizeContext(&context);
out:
	P61_INFO_MSG("tz_tee_ese_drv, cmd %d result=%#x origin=%#x\n", mode , result, returnOrigin);

	return result;
}

extern int tz_tee_ese_secure_check(void);
int tz_tee_ese_secure_check(void)
{
	return	tz_tee_ese_drv(SECURE_CHECK);
}
#endif

int ese_spi_pinctrl(int enable)
{
	int ret = 0;

	pr_info("[p61] %s (%d)\n", __func__, enable);

	switch (enable) {
	case 0:
#ifdef CONFIG_ESE_SECURE
		p61_clk_control(p61_dev, false);
		tz_tee_ese_drv(PM_SUSPEND);
#else
		if (p61_dev->ese_off_pin) {
			if (pinctrl_select_state(p61_dev->pinctrl, p61_dev->ese_off_pin))
				P61_INFO_MSG("ese off pinctrl set error\n");
			else
				P61_INFO_MSG("ese off pinctrl set\n");
		}
#endif		
		break;
	case 1:
#ifdef CONFIG_ESE_SECURE
		p61_clk_control(p61_dev, true);
		tz_tee_ese_drv(PM_RESUME);
#else
		if (p61_dev->ese_on_pin) {
			if (pinctrl_select_state(p61_dev->pinctrl, p61_dev->ese_on_pin))
				P61_INFO_MSG("ese on pinctrl set error\n");
			else
				P61_INFO_MSG("ese on pinctrl set\n");
		}
#endif
		break;
	default:
		pr_err("%s no matching!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ese_spi_pinctrl);

static int p61_xfer(struct p61_device *p61_dev,
			struct p61_ioctl_transfer *tr)
{
	int status = 0;
	struct spi_message m;
	struct spi_transfer t;
	/*For SDM845 & linux4.9: need to change spi buffer
	 * from stack to dynamic memory
	 */

	if (p61_dev == NULL || tr == NULL)
		return -EFAULT;

	if (tr->len > DEFAULT_BUFFER_SIZE || !tr->len)
		return -EMSGSIZE;

	spi_message_init(&m);
	memset(&t, 0, sizeof(t));

	memset(p61_dev->buf, 0, tr->len); /*memset 0 for read */
	if (tr->tx_buffer != NULL) { /*write */
		pr_info("%s...write\n", __func__);
		if (copy_from_user(p61_dev->buf, tr->tx_buffer, tr->len) != 0)
			return -EFAULT;
	}

	t.rx_buf = p61_dev->buf;
	t.tx_buf = p61_dev->buf;
	t.len = tr->len;

	spi_message_add_tail(&t, &m);

	status = spi_sync(p61_dev->spi, &m);
	if (status == 0) {
		if (tr->rx_buffer != NULL) { /*read */
			unsigned long missing = 0;

			pr_info("%s...read\n", __func__);
			missing = copy_to_user(tr->rx_buffer, p61_dev->buf, tr->len);
			if (missing != 0)
				tr->len = tr->len - (unsigned int)missing;
		}
	}
	pr_info("%s p61_xfer,length=%d\n", __func__, tr->len);
	return status;

} /* vfsspi_xfer */

static int p61_rw_spi_message(struct p61_device *p61_dev,
				 unsigned long arg)
{
	struct p61_ioctl_transfer   *dup = NULL;
	int err = 0;

	dup = kmalloc(sizeof(struct p61_ioctl_transfer), GFP_KERNEL);
	if (dup == NULL)
		return -ENOMEM;

	if (copy_from_user(dup, (void *)arg,
			   sizeof(struct p61_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	}

	err = p61_xfer(p61_dev, dup);
	if (err != 0) {
		kfree(dup);
		pr_err("%s p61_xfer failed!\n", __func__);
		return err;
	}

	if (copy_to_user((void *)arg, dup,
			 sizeof(struct p61_ioctl_transfer)) != 0) {
		kfree(dup);
		return -EFAULT;
	}
	kfree(dup);
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Called from SPI LibEse to initilaize the P61 device
 *
 * \param[in]       struct inode *
 * \param[in]       struct file *
 *
 * \retval 0 if ok.
 */
static int p61_dev_open(struct inode *inode, struct file *filp)
{
	struct p61_device *p61_dev = container_of(filp->private_data,
				struct p61_device, miscdev);
	struct spi_device *spidev = NULL;

	spidev = spi_dev_get(p61_dev->spi);

	filp->private_data = p61_dev;
	if (p61_dev->device_opened) {
		pr_err("%s: already opened!\n", __func__);
		return -EBUSY;
	}
#ifdef CONFIG_ESE_SECURE
	if (p61_dev->ese_secure_check == NOT_CHECKED) {
		int ret = 0;

		ret = tz_tee_ese_secure_check();
		if (ret) {
			p61_dev->ese_secure_check = ESE_NOT_SECURED;
			P61_ERR_MSG("eSE spi is not Secured\n"); 
			return -EBUSY;
		}
		p61_dev->ese_secure_check = ESE_SECURED;
	} else if (p61_dev->ese_secure_check == ESE_NOT_SECURED) {
			P61_ERR_MSG("eSE spi is not Secured\n"); 
			return -EBUSY;
	}
#endif

	pr_info("[ESE]:%s Major No: %d, Minor No: %d\n", __func__,
			imajor(inode), iminor(inode));

	if (!wake_lock_active(&p61_dev->ese_lock)) {
		pr_info("%s: [NFC-ESE] wake lock.\n", __func__);
		wake_lock(&p61_dev->ese_lock);
	}

	p61_dev->device_opened = true;

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To configure the P61_SET_PWR/P61_SET_DBG/P61_SET_POLL
 * \n	P61_SET_PWR - hard reset (arg=2), soft reset (arg=1)
 * \n	P61_SET_DBG - Enable/Disable (based on arg value) the driver logs
 * \n	P61_SET_POLL - Configure the driver in poll (arg = 1),
 *							interrupt (arg = 0) based read operation
 * \param[in]       struct file *
 * \param[in]       unsigned int
 * \param[in]       unsigned long
 *
 * \retval 0 if ok.
 *
 */
static long p61_dev_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int ret = 0;
	struct p61_device *p61_dev = NULL;

	if (_IOC_TYPE(cmd) != P61_MAGIC) {
		pr_err("%s invalid magic. cmd=0x%X Received=0x%X Expected=0x%X\n",
			__func__, cmd, _IOC_TYPE(cmd), P61_MAGIC);
		return -ENOTTY;
	}
	pr_debug("%s entered %x\n", __func__, cmd);
	p61_dev = filp->private_data;

	switch (cmd) {
	case P61_SET_PWR:
		if (arg == 2)
			pr_info("%s P61_SET_PWR. No Action.\n", __func__);
		break;

	case P61_SET_DBG:
		debug_level = (unsigned char)arg;
		P61_DBG_MSG(KERN_INFO"[NXP-P61] -  Debug level %d",
			debug_level);
		break;
	case P61_SET_POLL:
		p61_dev->enable_poll_mode = (unsigned char)arg;
		if (p61_dev->enable_poll_mode == 0) {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - IRQ Mode is set\n");
		} else {
			P61_DBG_MSG(KERN_INFO"[NXP-P61] - Poll Mode is set\n");
			p61_dev->enable_poll_mode = 1;
		}
		break;

#if !defined(CONFIG_NFC_FEATURE_SN100U)
	case P61_SET_SPI_CONFIG:
		pr_info("%s P61_SET_SPI_CONFIG. No Action.\n", __func__);
		break;
	case P61_ENABLE_SPI_CLK:
		pr_info("%s P61_ENABLE_SPI_CLK. No Action.\n", __func__);
		break;
	case P61_DISABLE_SPI_CLK:
		pr_info("%s P61_DISABLE_SPI_CLK. No Action.\n", __func__);
		break;
#endif

	case P61_RW_SPI_DATA:
#ifdef CONFIG_ESE_SECURE
		break;
#endif
		ret = p61_rw_spi_message(p61_dev, arg);
		break;

	case P61_SET_SPM_PWR:
		pr_info("%s P61_SET_SPM_PWR: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_SET_SPI_PWR, arg);
		if (arg == 0 || arg == 1 || arg == 3)
			pwr_req_on = arg;
		pr_info("%s P61_SET_SPM_PWR: exit\n", __func__);
		break;

	case P61_GET_SPM_STATUS:
		pr_info("%s P61_GET_SPM_STATUS: enter\n", __func__);
		ret = pn547_dev_ioctl(filp, P61_GET_PWR_STATUS, arg);
		pr_info("%s P61_GET_SPM_STATUS: exit\n", __func__);
		break;

	case P61_GET_ESE_ACCESS:
		/*P61_DBG_MSG(KERN_ALERT " P61_GET_ESE_ACCESS: enter");*/
		ret = pn547_dev_ioctl(filp, P547_GET_ESE_ACCESS, arg);
		pr_info("%s P61_GET_ESE_ACCESS ret: %d exit\n", __func__, ret);
		break;

	case P61_SET_DWNLD_STATUS:
		P61_DBG_MSG(KERN_ALERT " P61_SET_DWNLD_STATUS: enter\n");
		ret = pn547_dev_ioctl(filp, PN547_SET_DWNLD_STATUS,	arg);
		pr_info("%s P61_SET_DWNLD_STATUS: =%lu exit\n",	__func__, arg);
		break;

	default:
		pr_info("%s no matching ioctl!\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

/*
 * Called when a process closes the device file.
 */
static int p61_dev_release(struct inode *inode, struct file *file)
{
	struct p61_device *p61_dev = file->private_data;

	pr_info("[ESE]: %s\n", __func__);

	if (wake_lock_active(&p61_dev->ese_lock)) {
		pr_info("%s: [NFC-ESE] wake unlock.\n", __func__);
		wake_unlock(&p61_dev->ese_lock);
	}

	if (pwr_req_on && (pwr_req_on != 5)) {
		pr_info("%s: [NFC-ESE] release spi session.\n", __func__);
		pwr_req_on = 0;
		pn547_dev_ioctl(file, P61_SET_SPI_PWR, 0);
		pn547_dev_ioctl(file, P61_SET_SPI_PWR, 5);
	}
	p61_dev->device_opened = false;
	return 0;
}

/**
 * \ingroup spi_driver
 * \brief Write data to P61 on SPI
 *
 * \param[in]       struct file *
 * \param[in]       const char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval data size
 *
 */
static ssize_t p61_dev_write(struct file *filp, const char *buf, size_t count,
	loff_t *offset)
{
	int ret = -1;
	struct p61_device *p61_dev;

	P61_DBG_MSG("p61_dev_write -Enter count %zu\n", count);

#ifdef CONFIG_ESE_SECURE
	return 0;
#endif
	p61_dev = filp->private_data;

	mutex_lock(&p61_dev->write_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	memset(p61_dev->buf, 0, count);
	if (copy_from_user(p61_dev->buf, &buf[0], count)) {
		P61_ERR_MSG("%s : failed to copy from user space\n", __func__);
		mutex_unlock(&p61_dev->write_mutex);
		return -EFAULT;
	}
	/* Write data */
	ret = spi_write(p61_dev->spi, p61_dev->buf, count);
	if (ret < 0)
		ret = -EIO;
	else
		ret = count;

	mutex_unlock(&p61_dev->write_mutex);
	pr_info("%s -count %zu  %d- Exit\n", __func__, count, ret);

	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Used to read data from P61 in Poll/interrupt mode configured using
 *  ioctl call
 *
 * \param[in]       struct file *
 * \param[in]       char *
 * \param[in]       size_t
 * \param[in]       loff_t *
 *
 * \retval read size
 *
 */
/* for p61 only */
static ssize_t p61_dev_read(struct file *filp, char *buf, size_t count,
	loff_t *offset)
{
	int ret = -EIO;
	struct p61_device *p61_dev = filp->private_data;
	unsigned char sof = 0x00;
	int total_count = 0;
	//unsigned char rx_buffer[MAX_BUFFER_SIZE];

	P61_DBG_MSG("p61_dev_read count %zu - Enter\n", count);

#ifdef CONFIG_ESE_SECURE
	return 0;
#endif
	if (count < 1) {
		P61_ERR_MSG("Invalid length (min : 258) [%zu]\n", count);
		return -EINVAL;
	}
	mutex_lock(&p61_dev->read_mutex);
	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	//memset(&rx_buffer[0], 0x00, sizeof(rx_buffer));
	memset(p61_dev->buf, 0x00, MAX_BUFFER_SIZE);

	P61_DBG_MSG(" %s Poll Mode Enabled\n", __func__);
	do {
		sof = 0x00;
		ret = spi_read(p61_dev->spi, (void *)&sof, 1);
		if (ret < 0) {
			P61_ERR_MSG("spi_read failed [SOF]\n");
			goto fail;
		}
		//P61_DBG_MSG(KERN_INFO"SPI_READ returned 0x%x\n", sof);
		/* if SOF not received, give some time to P61 */
		/* RC put the conditional delay only if SOF not received */
		if (sof != SOF)
			usleep_range(5000, 5100);
	} while (sof != SOF);
	P61_DBG_MSG("SPI_READ returned 0x%x...\n", sof);

	total_count = 1;
	//rx_buffer[0] = sof;
	*p61_dev->buf = sof;
	/* Read the HEADR of Two bytes*/
	ret = spi_read(p61_dev->spi, p61_dev->buf + 1, 2);
	if (ret < 0) {
		P61_ERR_MSG("spi_read fails after [PCB]\n");
		ret = -EIO;
		goto fail;
	}

	total_count += 2;
	/* Get the data length */
	//count = rx_buffer[2];
	count = *(p61_dev->buf + 2);
	pr_info("Data Length = %zu", count);
	/* Read the available data along with one byte LRC */
	ret = spi_read(p61_dev->spi, (void *)(p61_dev->buf + 3), (count+1));
	if (ret < 0) {
		pr_err("%s spi_read failed\n", __func__);
		ret = -EIO;
		goto fail;
	}
	total_count = (total_count + (count+1));
	P61_DBG_MSG(KERN_INFO"total_count = %d", total_count);

	if (copy_to_user(buf, p61_dev->buf, total_count)) {
		P61_ERR_MSG("%s : failed to copy to user space\n", __func__);
		ret = -EFAULT;
		goto fail;
	}
	ret = total_count;
	P61_DBG_MSG("p61_dev_read ret %d Exit\n", ret);

	mutex_unlock(&p61_dev->read_mutex);

	return ret;

fail:
	P61_ERR_MSG("Error p61_dev_read ret %d Exit\n", ret);
	pr_info("%s - count %zu  %d- Exit\n", __func__, count, ret);
	mutex_unlock(&p61_dev->read_mutex);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Set the P61 device specific context for future use.
 * \param[in]       struct spi_device *
 * \param[in]       void *
 * \retval void
 */
static inline void p61_set_data(struct spi_device *spi, void *data)
{
	dev_set_drvdata(&spi->dev, data);
}

/**
 * \ingroup spi_driver
 * \brief Get the P61 device specific context.
 * \param[in]       const struct spi_device *
 * \retval Device Parameters
 */
static inline void *p61_get_data(const struct spi_device *spi)
{
	return dev_get_drvdata(&spi->dev);
}

static const struct file_operations p61_dev_fops = {
	.owner = THIS_MODULE,
	.read = p61_dev_read,
	.write = p61_dev_write,
	.open = p61_dev_open,
	.unlocked_ioctl = p61_dev_ioctl,
	.release = p61_dev_release,
};

#if 0
static ssize_t p61_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data;
	int ret = 0;

	//struct spi_device *spi = to_spi_device(dev);
	//ret = spi_read(p61_dev->spi, (void *)&sof, 1);

	pr_info("%s\n", __func__);
	data = 'a';
	snprintf(buf, 4, "%d\n", data);

	return ret;
}

static ssize_t p61_test_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int error;
	//struct spi_device *spi = to_spi_device(dev);

	error = kstrtoul(buf, 10, &data);
	if (error)
		return error;

	pr_info("%s [%lu]\n", __func__, data);

	return count;
}

static DEVICE_ATTR(test, 0644, p61_test_show, p61_test_store);
#endif

static int p61_parse_dt(struct device *dev,
	struct p61_device *p61_dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *spi_device_node;
	struct platform_device *spi_pdev;

	if (!of_property_read_string(np, "p61-ap_vendor",
		&p61_dev->ap_vendor)) {
		pr_info("%s: ap_vendor - %s\n", __func__, p61_dev->ap_vendor);
	}

	spi_device_node = of_parse_phandle(np, "p61-spi_node", 0);
	if (!IS_ERR_OR_NULL(spi_device_node)) {
		spi_pdev = of_find_device_by_node(spi_device_node);
#ifndef CONFIG_ESE_SECURE
		p61_dev->pinctrl = devm_pinctrl_get(&spi_pdev->dev);

		p61_dev->ese_on_pin = pinctrl_lookup_state(p61_dev->pinctrl, "ese_on");
		p61_dev->ese_off_pin = pinctrl_lookup_state(p61_dev->pinctrl, "ese_off");
		if (pinctrl_select_state(p61_dev->pinctrl, p61_dev->ese_off_pin))
			P61_INFO_MSG("ese off pinctrl set error\n");
		else
			P61_INFO_MSG("ese off pinctrl set\n");
#endif
	} else {
		pr_info("target does not use spi pinctrl\n");
	}

	return 0;
}

/**
 * \ingroup spi_driver
 * \brief To probe for P61 SPI interface. If found initialize the SPI clock,
 * bit rate & SPI mode. It will create the dev entry (P61) for user space.
 *
 * \param[in]       struct spi_device *
 *
 * \retval 0 if ok.
 *
 */
static int p61_probe(struct spi_device *spi)
{
	int ret = -1;

	pr_info("%s: chip select(%d), bus number(%d)\n",
		__func__, spi->chip_select, spi->master->bus_num);

	p61_dev = kzalloc(sizeof(*p61_dev), GFP_KERNEL);
	if (p61_dev == NULL) {
		P61_ERR_MSG("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = p61_parse_dt(&spi->dev, p61_dev);
	if (ret) {
		pr_err("%s: Failed to parse DT\n", __func__);
		goto p61_parse_dt_failed;
	}
	pr_info("%s: tz_mode=%d, isGpio_cfgDone:%d\n", __func__,
			p61_dev->tz_mode, p61_dev->isGpio_cfgDone);

	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = P61_SPI_CLOCK;
#ifndef CONFIG_ESE_SECURE
	ret = spi_setup(spi);
	if (ret < 0) {
		P61_ERR_MSG("failed to do spi_setup()\n");
		goto p61_spi_setup_failed;
	}
#else
	p61_dev->ese_secure_check = NOT_CHECKED;
	pr_info("%s: eSE Secured system\n", __func__);
	ret = p61_clk_setup(&spi->dev, p61_dev);
	if (ret)
		pr_err("%s - Failed to do clk_setup\n", __func__);
#endif
	p61_dev->spi = spi;
	p61_dev->miscdev.minor = MISC_DYNAMIC_MINOR;
	p61_dev->miscdev.name = "p61";
	p61_dev->miscdev.fops = &p61_dev_fops;
	p61_dev->miscdev.parent = &spi->dev;

	dev_set_drvdata(&spi->dev, p61_dev);

	/* init mutex and queues */
	init_waitqueue_head(&p61_dev->read_wq);
	mutex_init(&p61_dev->read_mutex);
	mutex_init(&p61_dev->write_mutex);
	spin_lock_init(&p61_dev->ese_spi_lock);

	wake_lock_init(&p61_dev->ese_lock, WAKE_LOCK_SUSPEND, "ese_wake_lock");
	p61_dev->device_opened = false;
	ret = misc_register(&p61_dev->miscdev);
	if (ret < 0) {
		P61_ERR_MSG("misc_register failed! %d\n", ret);
		goto err_exit0;
	}

	p61_dev->enable_poll_mode = 1; /* No USE? */

	p61_dev->buf = kzalloc(sizeof(unsigned char) * MAX_BUFFER_SIZE, GFP_KERNEL);
	if (p61_dev->buf == NULL) {
		P61_ERR_MSG("failed to allocate for spi buffer\n");
		ret = -ENOMEM;
		goto err_exit0;
	}

	pr_info("%s: finished\n", __func__);
	return ret;

err_exit0:
	mutex_destroy(&p61_dev->read_mutex);
	mutex_destroy(&p61_dev->write_mutex);
	wake_lock_destroy(&p61_dev->ese_lock);

#ifndef CONFIG_ESE_SECURE
p61_spi_setup_failed:
#endif
p61_parse_dt_failed:
	if (p61_dev != NULL)
		kfree(p61_dev);
err_exit:
	P61_DBG_MSG("ERROR: Exit : %s ret %d\n", __func__, ret);
	return ret;
}

/**
 * \ingroup spi_driver
 * \brief Will get called when the device is removed to release the resources.
 * \param[in]       struct spi_device
 * \retval 0 if ok.
 */
static int p61_remove(struct spi_device *spi)
{
	struct p61_device *p61_dev = p61_get_data(spi);

	P61_DBG_MSG("Entry : %s\n", __func__);
	mutex_destroy(&p61_dev->read_mutex);
	misc_deregister(&p61_dev->miscdev);
	wake_lock_destroy(&p61_dev->ese_lock);
	kfree(p61_dev->buf);
	kfree(p61_dev);

	P61_DBG_MSG("Exit : %s\n", __func__);
	return 0;
}

static const struct of_device_id p61_match_table[] = {
	{ .compatible = "p61",},
	{},
};

static struct spi_driver p61_driver = {
	.driver = {
		.name = "p61",
		.bus = &spi_bus_type,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = p61_match_table,
#endif
	},
	.probe =  p61_probe,
	.remove = p61_remove,
};

/**
 * \ingroup spi_driver
 * \brief Module init interface
 *
 * \param[in]       void
 *
 * \retval handle
 *
 */
static int __init p61_dev_init(void)
{
	debug_level = P61_FULL_DEBUG;

	P61_DBG_MSG("Entry : %s\n", __func__);

	return spi_register_driver(&p61_driver);

	P61_DBG_MSG("Exit : %s\n", __func__);
}

/**
 * \ingroup spi_driver
 * \brief Module exit interface
 *
 * \param[in]       void
 *
 * \retval void
 *
 */
static void __exit p61_dev_exit(void)
{
	P61_DBG_MSG("Entry : %s\n", __func__);
	spi_unregister_driver(&p61_driver);
	P61_DBG_MSG("Exit : %s\n", __func__);
}

module_init(p61_dev_init);
module_exit(p61_dev_exit);

MODULE_AUTHOR("BHUPENDRA PAWAR");
MODULE_DESCRIPTION("NXP P61 SPI driver");
MODULE_LICENSE("GPL");
