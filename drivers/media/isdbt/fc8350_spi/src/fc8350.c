/*****************************************************************************
 *	Copyright(c) 2017 FCI Inc. All Rights Reserved
 *
 *	File name : fc8350.c
 *
 *	Description : API source of ISDB-T baseband module
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *	GNU General Public License for more details.
 *
 *	History :
 *	----------------------------------------------------------------------
 *******************************************************************************/
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/io.h>
#ifdef SEC_ENABLE_13SEG_BOOST
#include <linux/pm_qos.h>
#endif

#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>

#include "fc8350.h"
#include "fc8350_i2c.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fc8350_regs.h"
#include "fc8350_isr.h"
#include "fci_hal.h"
#include "isdbt_tuner_pdata.h"
#ifdef CONFIG_SEC_ISDBT_FORCE_OFF
#include <linux/qcom/sec_debug.h>
#endif

struct ISDBT_INIT_INFO_T *hInit;
struct ISDBT_OPEN_INFO_T hOpen_Val;
static struct wake_lock isdbt_wlock;

unsigned int bbm_xtal_freq;
u16 g_pkt_length;

#ifndef BBM_I2C_TSIF
#define RING_BUFFER_SIZE	(TS0_32PKT_LENGTH * 17)
#endif

/* GPIO(RESET & INTRRUPT) Setting */
#define FC8350_NAME		"isdbt"
static struct isdbt_platform_data *isdbt_pdata;


#define ISDBT_LDO_ON      1
#define ISDBT_LDO_OFF     0

/* u8 static_ringbuffer[RING_BUFFER_SIZE]; */
u8 *static_ringbuffer;

#ifdef SEC_ENABLE_13SEG_BOOST
#define FULLSEG_MIN_FREQ	1296000
static struct pm_qos_request fc8350_cpu_handle;
#endif

#ifdef TS_DROP_DEBUG
#define FEATURE_TS_CHECK
#ifdef FEATURE_TS_CHECK
u32 check_cnt_size;

#define MAX_DEMUX           2

/*
 * Sync Byte 0xb8
 */
#define SYNC_BYTE_INVERSION

struct pid_info {
	unsigned long count;
	unsigned long discontinuity;
	unsigned long continuity;
};

struct demux_info {
	struct pid_info  pids[8192];

	unsigned long    ts_packet_c;
	unsigned long    malformed_packet_c;
	unsigned long    tot_scraped_sz;
	unsigned long    packet_no;
	unsigned long    sync_err;
	unsigned long    sync_err_set;
};

static int is_sync(unsigned char *p)
{
	int syncword = p[0];
#ifdef SYNC_BYTE_INVERSION
	if (0x47 == syncword || 0xb8 == syncword)
		return 1;
#else
	if (0x47 == syncword)
		return 1;
#endif
	return 0;
}
static struct demux_info demux[MAX_DEMUX];

int print_pkt_log(void)
{
	unsigned long i = 0;

	print_log(NULL
		, "\nPKTOT : %d, SYNERR : %d, SYNERR_BIT : %d, ERR_PKT : %d\n"
		, demux[0].ts_packet_c, demux[0].sync_err
		, demux[0].sync_err_set, demux[0].malformed_packet_c);

	for (i = 0; i < 8192; i++) {
		if (demux[0].pids[i].count > 0)
			print_log(NULL, "PID : %d, TOT_PKT : %d, DISCON : %d\n"
			, i, demux[0].pids[i].count
			, demux[0].pids[i].discontinuity);
	}
	return 0;
}

int put_ts_packet(int no, unsigned char *packet, int sz)
{
	unsigned char *p;
	int transport_error_indicator, pid, payload_unit_start_indicator;
	int continuity_counter, last_continuity_counter;
	int i;
	if ((sz % 188)) {
		print_log(NULL, "L : %d", sz);
	} else {
		for (i = 0; i < sz; i += 188) {
			p = packet + i;

			pid = ((p[1] & 0x1f) << 8) + p[2];

			demux[no].ts_packet_c++;
			if (!is_sync(packet + i)) {
				/* print_log(NULL, "S     "); */
				demux[no].sync_err++;
				if (0x80 == (p[1] & 0x80))
					demux[no].sync_err_set++;
				/*
				print_log(NULL, "0x%x, 0x%x, 0x%x, 0x%x\n"
					, *p, *(p+1),  *(p+2), *(p+3)); */
				continue;
			}

			transport_error_indicator = (p[1] & 0x80) >> 7;
			if (1 == transport_error_indicator) {
				demux[no].malformed_packet_c++;
				continue;
			}

			payload_unit_start_indicator = (p[1] & 0x40) >> 6;

			demux[no].pids[pid].count++;

			continuity_counter = p[3] & 0x0f;

			if (demux[no].pids[pid].continuity == -1) {
				demux[no].pids[pid].continuity
					= continuity_counter;
			} else {
				last_continuity_counter
					= demux[no].pids[pid].continuity;

				demux[no].pids[pid].continuity
					= continuity_counter;

				if (((last_continuity_counter + 1) & 0x0f)
					!= continuity_counter) {
					demux[no].pids[pid].discontinuity++;
				}
			}
		}
	}
	return 0;
}


void create_tspacket_anal(void)
{
	int n, i;

	for (n = 0; n < MAX_DEMUX; n++) {
		memset((void *)&demux[n], 0, sizeof(demux[n]));

		for (i = 0; i < 8192; i++)
			demux[n].pids[i].continuity = -1;
	}

}
#endif
#endif

enum ISDBT_MODE driver_mode = ISDBT_POWEROFF;
static DEFINE_MUTEX(ringbuffer_lock);
static DEFINE_MUTEX(power_onoff_lock);

static DECLARE_WAIT_QUEUE_HEAD(isdbt_isr_wait);

static long	open_cnt;        /* OPEN counter	*/
static long	moni_cnt;		 /* Monitor counter	*/

static int tuner_ioctl_set_monitor_mode(struct file *FIle,
			unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;

	pr_err("tuner_ioctl_set_monitor_mode << Start >> ");

	if (1 == arg) {
		/* Monitor Mode Start */
		moni_cnt++;
	} else {
		/* Monitor Mode Stop */
		moni_cnt--;
		if (0 > moni_cnt) {
			pr_err(" tuner_ioctl_set_monitor_mode under counter = %ld => 0"
				, moni_cnt);
			moni_cnt = 0;
		}
	}
	pr_err("tuner_ioctl_set_monitor_mode << End >> : moni_cnt = %ld",
				moni_cnt);

	return ret;
}

static int tuner_ioctl_get_open_count(struct file *FIle,
			unsigned int cmd,
			unsigned long arg)
{
	struct tuner_sts_data *arg_data;
	int	ret = 0;
	unsigned long	temp_open = 0;

	pr_err("tuner_ioctl_get_open_count << Start >> : open = %ld",
				 (open_cnt - moni_cnt));

	/* Parameter check */
	arg_data = (struct tuner_sts_data *)arg;

	if (NULL == arg_data)	{
		pr_err("Parameter Error : arg = NULL");
		return -EINVAL;
	}
	/* state check */
	if (open_cnt < moni_cnt)	{
		pr_err("tuner_ioctl_get_open_count Error : open = %ld, moni = %ld",
					 open_cnt, moni_cnt);
		return -EINVAL;
	}
	temp_open = (open_cnt - moni_cnt);

	/* Copy to User Area */
	ret = put_user(temp_open
			, (unsigned long __user *)&(arg_data->open_cnt));
	if (0 != ret)	{
		pr_err("tuner_ioctl_get_open_count put_user(arg_data->open_cnt) Error : ret = %d"
			, ret);
		return -EINVAL;
	}

	/* Copy to User Area */
	ret = put_user(moni_cnt, (unsigned long __user *)&(arg_data->moni_cnt));
	if (0 != ret)	{
		pr_err("tuner_ioctl_get_open_count put_user(arg_data->moni_cnt) Error : ret = %d"
			, ret);
		return -EINVAL;
	}

	pr_err("tuner_ioctl_get_open_count << End >>");
	pr_err(" Open Count Result    : %ld", open_cnt);
	pr_err(" Monitor Count Result : %ld", moni_cnt);

	return 0;
}

#ifndef BBM_I2C_TSIF
int data_callback(void *hDevice, u8 bufid, u8 *data, int len)
{
	struct ISDBT_INIT_INFO_T *hInit;
	struct list_head *temp;

	hInit = (struct ISDBT_INIT_INFO_T *)hDevice;

	list_for_each(temp, &(hInit->hHead))
	{
		struct ISDBT_OPEN_INFO_T *hOpen;

		hOpen = list_entry(temp, struct ISDBT_OPEN_INFO_T, hList);

		if (hOpen->isdbttype == TS_TYPE) {
#ifdef TS_DROP_DEBUG

			if (!(len%188)) {
				put_ts_packet(0, data, len);
				check_cnt_size += len;

				if (check_cnt_size > 188*32*200) {
					print_pkt_log();
					check_cnt_size = 0;
				}
			}

#endif

			if (fci_ringbuffer_free(&hOpen->RingBuffer) < len) {
#ifdef TS_DROP_DEBUG
				print_log(NULL, "[FC8350] RingBuffer full\n");
#endif
				/* return 0 */;
				FCI_RINGBUFFER_SKIP(&hOpen->RingBuffer, len);
			}
#ifdef TS_DROP_DEBUG
			print_log(NULL, "[FC8350] RingBuffer Write %d\n", len);
#endif
			mutex_lock(&ringbuffer_lock);
			fci_ringbuffer_write(&hOpen->RingBuffer, data, len);
			mutex_unlock(&ringbuffer_lock);
		}
#ifdef TS_DROP_DEBUG
		else
			print_log(NULL, "[FC8350] Data callback : TS Stop\n");
#endif
	}

	return 0;
}

static irqreturn_t isdbt_threaded_irq(int irq, void *dev_id)
{
	struct ISDBT_INIT_INFO_T *hInit = (struct ISDBT_INIT_INFO_T *)dev_id;

	mutex_lock(&power_onoff_lock);
	if (driver_mode == ISDBT_POWERON) {
		driver_mode = ISDBT_DATAREAD;
		bbm_com_isr(hInit);
		driver_mode = ISDBT_POWERON;
	}
	mutex_unlock(&power_onoff_lock);

	return IRQ_HANDLED;
}

irqreturn_t isdbt_irq(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}
#endif

int isdbt_hw_setting(HANDLE hDevice)
{
	int err;
	struct ISDBT_INIT_INFO_T *hInit = (struct ISDBT_INIT_INFO_T *)hDevice;

	print_log(0, "isdbt_hw_setting\n");
	pr_err("%s\n", __func__);

#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
	err = gpio_request(isdbt_pdata->gpio_tmm_sw, "isdbt_tmm_sw");
	if (err) {
		pr_err("isdbt_hw_setting: Couldn't request isdbt_tmm_sw\n");
		goto ISDBT_TMM_SW_ERR;
	}
	gpio_direction_output(isdbt_pdata->gpio_tmm_sw, 0);
#endif
	if (gpio_is_valid(isdbt_pdata->gpio_power_en)) {
		err = gpio_request(isdbt_pdata->gpio_power_en, "isdbt_power_en");
		if (err) {
			pr_err("isdbt_hw_setting: Couldn't request isdbt_power_en err=%d\n"
				, err);
		}
		gpio_direction_output(isdbt_pdata->gpio_power_en, 0);
	}
	if (gpio_is_valid(isdbt_pdata->gpio_en)) {
		err = gpio_request(isdbt_pdata->gpio_en, "isdbt_en");
		if (err) {
			pr_err("isdbt_hw_setting: Couldn't request isdbt_en err=%d\n"
					, err);
		} else {
			gpio_direction_output(isdbt_pdata->gpio_en, 0);
		}
	}

	if (isdbt_pdata->gpio_cp_dt > 0)
		gpio_direction_input(isdbt_pdata->gpio_cp_dt);

	err = gpio_request(isdbt_pdata->gpio_rst, "isdbt_rst");
	if (err) {
		pr_err("isdbt_hw_setting: Couldn't request isdbt_rst err=%d\n"
			, err);
		goto ISDBT_RST_ERR;
	}
	gpio_direction_output(isdbt_pdata->gpio_rst, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_lna_en)) {
		err = gpio_request(isdbt_pdata->gpio_lna_en, "isdbt_lna_en");
		if (err)
			pr_err("isdbt_hw_setting: Couldn't request isdbt_lna_en err=%d\n"
				, err);

		gpio_direction_output(isdbt_pdata->gpio_lna_en, 0);
	}

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl)) {
		err = gpio_request(isdbt_pdata->gpio_ant_ctrl, "isdbt_ant_ctrl");
		if (err)
			pr_err("isdbt_hw_setting: Couldn't request isdbt_ant_ctrl err=%d\n"
				, err);

		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl, 0);
	}

#ifndef BBM_I2C_TSIF
	err =	gpio_request(isdbt_pdata->gpio_int, "isdbt_irq");
	if (err) {
		pr_err("isdbt_hw_setting: Couldn't request isdbt_irq\n");
		goto ISDBT_INT_ERR;
	}

	err = request_threaded_irq(gpio_to_irq(isdbt_pdata->gpio_int), isdbt_irq
		, isdbt_threaded_irq, /*IRQF_DISABLED |*/ IRQF_TRIGGER_FALLING
		, FC8350_NAME, hInit);
	if (err < 0) {
		print_log(0, "isdbt_hw_setting: couldn't request gpio interrupt %d reason(%d)\n"
			, gpio_to_irq(isdbt_pdata->gpio_int), err);
		goto request_isdbt_irq;
	}
	gpio_direction_input(isdbt_pdata->gpio_int);
#endif

	return 0;

#ifndef BBM_I2C_TSIF
request_isdbt_irq:
	gpio_free(isdbt_pdata->gpio_int);
ISDBT_INT_ERR:
	gpio_free(isdbt_pdata->gpio_rst);
#endif

ISDBT_RST_ERR:
	gpio_free(isdbt_pdata->gpio_power_en);

#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
ISDBT_TMM_SW_ERR:
	gpio_free(isdbt_pdata->gpio_tmm_sw);
#endif
	return err;
}


static void isdbt_gpio_init(HANDLE hDevice)
{
#if defined(CONFIG_SEC_GPIO_SETTINGS)
	struct pinctrl *isdbt_pinctrl;

	isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
						, "isdbt_gpio_suspend");
	if (IS_ERR(isdbt_pinctrl))
		pr_err("Target does not use pinctrl(isdbt_gpio_suspend)\n");

	isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
						, "isdbt_b28_rel");
	if (IS_ERR(isdbt_pinctrl))
		pr_err("Target does not use pinctrl(isdbt_b28_rel)\n");	
#endif
	pr_err("%s\n", __func__);

	isdbt_hw_setting(hDevice);
}

static void isdbt_regulator_onoff(int onoff)
{
	int rc = 0;
	struct regulator *regulator_vdd_1p8;

	if (isdbt_pdata->ldo_vdd_1p8 == NULL) {
		pr_err("%s - ldo_vdd_1p8 regulator is not present. Hence ignoring the setting\n"
			, __func__);
		return;
	}

	if (isdbt_pdata->regulator_is_enable == onoff) {
		pr_err("ISDBT duplicate regulator setting. Already in %s state"
			, onoff ? "ON" : "OFF");
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
		. Default voltage is 1.8v.*/
		/*
		rc = regulator_set_voltage(regulator_vdd_1p8
			, 1800000, 1800000);
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

#if defined(CONFIG_SEC_GPIO_SETTINGS)
	struct pinctrl *isdbt_pinctrl;

	isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
						, "isdbt_gpio_active");
	if (IS_ERR(isdbt_pinctrl))
		pr_err("Target does not use pinctrl\n");
#endif

	isdbt_regulator_onoff(ISDBT_LDO_ON);
	if (gpio_is_valid(isdbt_pdata->gpio_power_en))
		gpio_direction_output(isdbt_pdata->gpio_power_en, 1);

	if (isdbt_pdata->isdbt_clk != NULL) {
		clk_prepare_enable(isdbt_pdata->isdbt_clk);
		pr_err("%s, Enabling ISDBT_CLK\n", __func__);
	}
	while (driver_mode == ISDBT_DATAREAD) {
		msWait(100);
		if (i++ > 5)
			break;
	}

	pr_err("%s\n", __func__);

	if (gpio_is_valid(isdbt_pdata->gpio_lna_en))
		gpio_direction_output(isdbt_pdata->gpio_lna_en, 1);

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl, 1);

	pr_err("%s, lna = %d ant ctrl %d\n", __func__,
			gpio_get_value(isdbt_pdata->gpio_lna_en), gpio_get_value(isdbt_pdata->gpio_ant_ctrl));

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl1))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl1, isdbt_pdata->gpio_ant_ctrl_sel[0]);
	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl2))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl2, isdbt_pdata->gpio_ant_ctrl_sel[1]);

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl1) && gpio_is_valid(isdbt_pdata->gpio_ant_ctrl2))
		pr_err("%s, gpio_ant_ctrl1 = %d gpio_ant_ctrl2 = %d \n", __func__,
			gpio_get_value(isdbt_pdata->gpio_ant_ctrl1), gpio_get_value(isdbt_pdata->gpio_ant_ctrl2));

	gpio_direction_output(isdbt_pdata->gpio_rst, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_en))
		gpio_direction_output(isdbt_pdata->gpio_en, 1);
	usleep_range(9000, 10000); /* fc8350 chipspec is 1ms */

	if (gpio_is_valid(isdbt_pdata->gpio_en)) {
		pr_err("%s, gpio_en = %d\n", __func__,
			gpio_get_value(isdbt_pdata->gpio_en));
	}
	gpio_direction_output(isdbt_pdata->gpio_rst, 1);
	usleep_range(9000, 10000);
	if (gpio_is_valid(isdbt_pdata->gpio_rst)) {
		pr_err("%s, gpio_rst = %d\n", __func__,
			gpio_get_value(isdbt_pdata->gpio_en));
	}	

	mutex_lock(&power_onoff_lock);
	driver_mode = ISDBT_POWERON;
	mutex_unlock(&power_onoff_lock);
#ifdef BBM_SPI_IF
	bbm_com_byte_write(hInit, DIV_BROADCAST, BBM_DM_DATA, 0x00);
#endif
}

/*POWER_OFF */
void isdbt_hw_deinit(void)
{
#if defined(CONFIG_SEC_GPIO_SETTINGS)
	struct pinctrl *isdbt_pinctrl;
#endif
	if (isdbt_pdata->isdbt_clk != NULL) {
		clk_disable_unprepare(isdbt_pdata->isdbt_clk);
		pr_err("%s, Turning ISDBT_CLK off\n", __func__);
	}
	mutex_lock(&power_onoff_lock);
	driver_mode = ISDBT_POWEROFF;
	mutex_unlock(&power_onoff_lock);

	if (gpio_is_valid(isdbt_pdata->gpio_en))
		gpio_direction_output(isdbt_pdata->gpio_en, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_rst))
		gpio_direction_output(isdbt_pdata->gpio_rst, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_lna_en))
		gpio_direction_output(isdbt_pdata->gpio_lna_en, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl, 0);

	if (gpio_is_valid(isdbt_pdata->gpio_power_en))
		gpio_direction_output(isdbt_pdata->gpio_power_en, 0);
	isdbt_regulator_onoff(ISDBT_LDO_OFF);

	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl1))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl1, isdbt_pdata->gpio_ant_ctrl_sel[2]);
	if (gpio_is_valid(isdbt_pdata->gpio_ant_ctrl2))
		gpio_direction_output(isdbt_pdata->gpio_ant_ctrl2, isdbt_pdata->gpio_ant_ctrl_sel[3]);

#if defined(CONFIG_SEC_GPIO_SETTINGS)
	isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
							, "isdbt_gpio_suspend");
	if (IS_ERR(isdbt_pinctrl))
		pr_err("Target does not use pinctrl\n");
#endif
}

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

static struct miscdevice fc8350_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = FC8350_NAME,
	.fops = &isdbt_fops,
};

int isdbt_open(struct inode *inode, struct file *filp)
{
	struct ISDBT_OPEN_INFO_T *hOpen;

	pr_err("isdbt open\n");

	if (!static_ringbuffer) {
		static_ringbuffer = kzalloc(RING_BUFFER_SIZE,
				GFP_KERNEL);

		if (!static_ringbuffer) {
			pr_err("%s : failed to allocate memory!\n", __func__);
			return -EINVAL;
		}
	}

	open_cnt++;
	hOpen = &hOpen_Val;
	hOpen->buf = &static_ringbuffer[0];
	hOpen->isdbttype = 0;

	if (list_empty(&(hInit->hHead)))
		list_add(&(hOpen->hList), &(hInit->hHead));

	hOpen->hInit = (HANDLE *)hInit;
	fci_ringbuffer_init(&hOpen->RingBuffer, hOpen->buf, RING_BUFFER_SIZE);

	filp->private_data = hOpen;

	if (open_cnt == 1)
		wake_lock(&isdbt_wlock);

	return 0;
}

ssize_t isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	s32 avail;
	struct ISDBT_OPEN_INFO_T *hOpen
		= (struct ISDBT_OPEN_INFO_T *)filp->private_data;
	struct fci_ringbuffer *cibuf = &hOpen->RingBuffer;
	ssize_t len, read_len = 0;

	if (!cibuf->data || !count)	{
		/*pr_err(" return 0\n"); */
		return 0;
	}
	if (fci_ringbuffer_empty(cibuf))	{
		/*print_log(hInit, "return EWOULDBLOCK\n"); */
		return -EWOULDBLOCK;
	}

	mutex_lock(&ringbuffer_lock);
	avail = fci_ringbuffer_avail(cibuf);
	mutex_unlock(&ringbuffer_lock);

	if (count >= avail)
		len = avail;
	else
		len = count - (count % 188);

	read_len = fci_ringbuffer_read_user(cibuf, buf, len);

#ifdef TS_DROP_DEBUG
#ifdef FEATURE_TS_CHECK
	if (!(read_len%188)) {
		put_ts_packet(0, buf, read_len);
		check_cnt_size += read_len;

		if (check_cnt_size > 188*32*200) {
			print_pkt_log();
			check_cnt_size = 0;
		}
	} else
		print_log(NULL, "[FC8350] Read Len Error %d\n", read_len);
#endif
#endif

#ifdef TS_DROP_DEBUG
	print_log(hInit, "[FC8350] RingBuffer Read %d  Buffer : %d\n", read_len, count);
#endif
	return read_len;
}

int isdbt_release(struct inode *inode, struct file *filp)
{
	pr_err("isdbt_release\n");

#ifdef SEC_ENABLE_13SEG_BOOST
	if (pm_qos_request_active(&fc8350_cpu_handle)) {
		pr_info("[FC8350] disable boost!!!\n");
		pm_qos_remove_request(&fc8350_cpu_handle);
	}
#endif

	if (open_cnt <= 0) {
		pr_err("tuner_module_entry_close: close error\n");
		open_cnt = 0;
		return -EINVAL;
	} else {
		open_cnt--;
	}

	/* close all open */
	if (open_cnt == 0) {
		struct ISDBT_OPEN_INFO_T *hOpen;
		hOpen = filp->private_data;

		if (hOpen != NULL) {
			hOpen->isdbttype = 0;
			if (!list_empty(&(hInit->hHead)))
				list_del(&(hOpen->hList));
			pr_err("isdbt_release hList\n");
			wake_unlock(&isdbt_wlock);
		}

		if (driver_mode != ISDBT_POWEROFF)
			isdbt_hw_deinit();
	}

	return 0;
}

#ifdef CONFIG_SEC_ISDBT_FORCE_OFF
struct workqueue_struct *g_isdbt_wq;
struct work_struct g_isdbt_force_off_work;
int g_flag;

static void isdbt_force_off_func(struct work_struct *work)
{
	pr_err("%s\n", __func__);
	if (gpio_is_valid(isdbt_pdata->gpio_en))
		gpio_direction_output(isdbt_pdata->gpio_en, 0);
	gpio_direction_output(isdbt_pdata->gpio_rst, 0);
	gpio_direction_output(isdbt_pdata->gpio_power_en, 0);
	g_flag = 1;
}

void isdbt_force_off(void)
{
	if (driver_mode != ISDBT_POWEROFF) {
		int i;
		pr_err("%s start\n", __func__);
		g_flag = 0;
		queue_work(g_isdbt_wq, &g_isdbt_force_off_work);
		for (i = 0; i < 10; i++) {
			mdelay(2);
			if (g_flag)
				break;
		}
		pr_err("%s end\n", __func__);
	}
}
#endif

#ifndef BBM_I2C_TSIF
void isdbt_isr_check(HANDLE hDevice)
{
	u8 isr_time = 0;

	bbm_com_write(hDevice, DIV_BROADCAST, BBM_BUF_ENABLE, 0x00);
	while (isr_time < 10) {
		if (driver_mode != ISDBT_DATAREAD)
			break;

		msWait(10);
		isr_time++;
	}

}
#endif

static ssize_t ber_show(struct class *dev,
	struct class_attribute *attr, char *buf)
{
	int type, BER;

	switch (isdbt_pdata->type) {
	case ISDBTMM_13SEG:
		type = 2;
		BER = isdbt_pdata->BER[1];
		break;
	case ISDBT_13SEG:
		type = 1;
		BER = isdbt_pdata->BER[1];
		break;
	default:	/* 1-seg */
		type = 0;
		BER = isdbt_pdata->BER[0];
		break;
	}

	sprintf(buf, "%d,%d", type, BER);
	pr_info("%s, type:%d, BER_A:%d, BER_B:%d\n", __func__, type,
		isdbt_pdata->BER[0], isdbt_pdata->BER[1]);

	return strlen(buf);
}

static CLASS_ATTR_RO(ber);

long isdbt_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	s32 res = BBM_NOK;
	s32 err = 0;
	u32 size = 0;
	struct ISDBT_OPEN_INFO_T *hOpen;

	struct ioctl_info info;
#if defined(CONFIG_SEC_GPIO_SETTINGS)
	struct pinctrl *isdbt_pinctrl;
#endif

	if (_IOC_TYPE(cmd) != IOCTL_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) >= IOCTL_MAXNR)
		return -EINVAL;

	hOpen = filp->private_data;

	size = _IOC_SIZE(cmd);
	if (size > sizeof(struct ioctl_info))
		size = sizeof(struct ioctl_info);
	switch (cmd) {
	case IOCTL_ISDBT_RESET:
		res = bbm_com_reset(hInit, DIV_BROADCAST);
		pr_err("[FC8350] IOCTL_ISDBT_RESET\n");
		break;
	case IOCTL_ISDBT_INIT:
		pr_err("[FC8350] IOCTL_ISDBT_INIT\n");
#ifndef BBM_I2C_TSIF
		isdbt_isr_check(hInit);
#endif
		res = bbm_com_i2c_init(hInit, FCI_HPI_TYPE);
		pr_err("[FC8350] IOCTL_ISDBT_INIT bbm_com_i2c_init res = %d\n"
			, res);
		res |= bbm_com_probe(hInit, DIV_BROADCAST);
		pr_err("[FC8350] ISDBT_INIT BBM : %s Xtal : %d, DEV : %s\n"
			, DRIVER_VER, BBM_XTAL_FREQ, DRV_VER);

		if (res) {
			pr_err("[FC8350] Initialize Fail\n");
			break;
		}
		pr_err("[FC8350] IOCTL_ISDBT_INIT bbm_com_probe success\n");
		res |= bbm_com_init(hInit, DIV_BROADCAST);
		pr_err("[FC8350] IOCTL_ISDBT_INITbbm_com_init %d\n", res);
		break;
	case IOCTL_ISDBT_BYTE_READ:
		pr_debug("[FC8350] IOCTL_ISDBT_BYTE_READ\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_read(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u8 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_WORD_READ:
		pr_debug("[FC8350] IOCTL_ISDBT_WORD_READ\n");

		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_read(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u16 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_LONG_READ:
		pr_debug("[FC8350] IOCTL_ISDBT_LONG_READ\n");

		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_read(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u32 *)(&info.buff[1]));
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_BULK_READ:
		pr_debug("[FC8350] IOCTL_ISDBT_BULK_READ\n");

		err = copy_from_user((void *)&info, (void *)arg, size);
		if (info.buff[1] >
			(sizeof(info.buff) - sizeof(info.buff[0]) * 2)) {
			print_log(hInit, "[FC8350] BULK_READ sizeErr %d\n"
				, info.buff[1]);
			res = BBM_NOK;
			break;
		}
		res = bbm_com_bulk_read(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_BYTE_WRITE:
		pr_debug("[FC8350] IOCTL_ISDBT_BYTE_WRITE\n");

		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_byte_write(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u8)info.buff[1]);
		break;
	case IOCTL_ISDBT_WORD_WRITE:
		pr_debug("[FC8350] IOCTL_ISDBT_WORD_WRITE\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_word_write(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u16)info.buff[1]);
		break;
	case IOCTL_ISDBT_LONG_WRITE:
		pr_debug("[FC8350] IOCTL_ISDBT_LONG_WRITE\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_long_write(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u32)info.buff[1]);
		break;
	case IOCTL_ISDBT_BULK_WRITE:
		pr_debug("[FC8350] IOCTL_ISDBT_BULK_WRITE\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		if (info.buff[1] >
			(sizeof(info.buff) - sizeof(info.buff[0]) * 2)) {
			print_log(hInit, "[FC8350] BULK_WRITE sizeErr %d\n"
				, info.buff[1]);
			res = BBM_NOK;
			break;
		}
		res = bbm_com_bulk_write(hInit, DIV_BROADCAST, (u16)info.buff[0]
			, (u8 *)(&info.buff[2]), info.buff[1]);
		break;
	case IOCTL_ISDBT_TUNER_READ:
		pr_debug("[FC8350] IOCTL_ISDBT_TUNER_READ\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		if ((info.buff[1] > 1) || (info.buff[2] >
			(sizeof(info.buff) - sizeof(info.buff[0]) * 3))) {
			print_log(hInit
				, "[FC8350] TUNER_R sizeErr A[%d] D[%d]\n"
				, info.buff[1], info.buff[2]);
			res = BBM_NOK;
			break;
		}
		res = bbm_com_tuner_read(hInit, DIV_BROADCAST, (u8)info.buff[0]
			, (u8)info.buff[1],  (u8 *)(&info.buff[3])
			, (u8)info.buff[2]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case IOCTL_ISDBT_TUNER_WRITE:
		pr_debug("[FC8350] IOCTL_ISDBT_TUNER_WRITE\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		if ((info.buff[1] > 1) || (info.buff[2] >
			(sizeof(info.buff) - sizeof(info.buff[0]) * 3))) {
			print_log(hInit
				, "[FC8350] TUNER_R sizeErr A[%d] D[%d]\n"
				, info.buff[1], info.buff[2]);
			res = BBM_NOK;
			break;
		}
		res = bbm_com_tuner_write(hInit
					, DIV_BROADCAST, (u8)info.buff[0]
					, (u8)info.buff[1], (u8 *)(&info.buff[3])
					, (u8)info.buff[2]);
		break;
	case IOCTL_ISDBT_TUNER_SET_FREQ:
		{
			u32 f_rf;
			u8 subch;
			err = copy_from_user((void *)&info, (void *)arg, size);

			f_rf = (u32)info.buff[0];
			subch = (u8)info.buff[1];
			pr_err("[FC8350] IOCTL_ISDBT_TUNER_SET_FREQ freq=%d subch=%d\n"
				, f_rf, subch);
#ifndef BBM_I2C_TSIF
			isdbt_isr_check(hInit);
#endif
			res = bbm_com_tuner_set_freq(hInit
				, DIV_BROADCAST, f_rf, subch);
			pr_err("[FC8350] IOCTL_ISDBT_TUNER_SET_FREQ %d\n", res);
#ifndef BBM_I2C_TSIF
			mutex_lock(&ringbuffer_lock);
			fci_ringbuffer_flush(&hOpen->RingBuffer);
			mutex_unlock(&ringbuffer_lock);
			bbm_com_write(hInit
				, DIV_BROADCAST, BBM_BUF_ENABLE, 0x01);
#endif
		}
		break;

	case IOCTL_ISDBT_TUNER_SELECT_2:
		pr_err("[FC8350] IOCTL_ISDBT_TUNER_SELECT_2\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		bbm_com_byte_write(hInit, DIV_BROADCAST
			, BBM_BUF_ENABLE, 0x00);
		if (((u32)info.buff[1] == ISDBTMM_13SEG)
			|| ((u32)info.buff[1] == ISDBT_13SEG)) {
#ifdef SEC_ENABLE_13SEG_BOOST
			pr_info("[FC8350] enable boost!\n");
			pm_qos_add_request(&fc8350_cpu_handle
				, PM_QOS_CLUSTER0_FREQ_MIN, FULLSEG_MIN_FREQ);
#endif
			g_pkt_length = TS0_32PKT_LENGTH;
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_END, TS0_32PKT_LENGTH - 1);
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_THR, TS0_32PKT_LENGTH / 2 - 1);
			bbm_com_byte_write(hInit, DIV_BROADCAST
				, BBM_LAYER_FILTER0, 0x00);
			bbm_com_byte_write(hInit, DIV_BROADCAST
				, BBM_BID_FILTER_MODE, 0x00);
			print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
				, TS0_32PKT_LENGTH / 2 - 1);
		}	else	{
#ifdef SEC_ENABLE_13SEG_BOOST
			if (pm_qos_request_active(&fc8350_cpu_handle)) {
				pr_info("[FC8350] 1seg don't need boost. remove!!\n");
				pm_qos_remove_request(&fc8350_cpu_handle);
			}
#endif
			g_pkt_length = TS0_5PKT_LENGTH;
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_END, TS0_5PKT_LENGTH - 1);
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_THR, TS0_5PKT_LENGTH / 2 - 1);
			bbm_com_byte_write(hInit, DIV_BROADCAST
				, BBM_LAYER_FILTER0, 0x01);
			bbm_com_byte_write(hInit, DIV_BROADCAST
				, BBM_BID_FILTER_MODE, 0x02);
			print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
				, TS0_5PKT_LENGTH / 2 - 1);
		}
		isdbt_pdata->type = (u32)info.buff[1];
		bbm_com_byte_write(hInit, DIV_BROADCAST, BBM_BUF_ENABLE, 0x01);
		bbm_com_reset(hInit, DIV_BROADCAST);
		print_log(hInit, "[FC8350] IOCTL_ISDBT_TUNER_SELECT_2 %d\n"
			, (u32)info.buff[1]);
		break;

	case IOCTL_ISDBT_TUNER_SELECT:
		pr_err("[FC8350] IOCTL_ISDBT_TUNER_SELECT\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_select(hInit
					, DIV_BROADCAST, FC8350_TUNER/*(u32)info.buff[0]*/, (u32)info.buff[1]);
		pr_err("[FC8350] bbm_com_tuner_select %d\n", res);

		if (((u32)info.buff[1] == ISDBTMM_13SEG)
			|| ((u32)info.buff[1] == ISDBT_13SEG)) {
#ifdef SEC_ENABLE_13SEG_BOOST
			pr_info("[FC8350] enable boost!\n");
			pm_qos_add_request(&fc8350_cpu_handle
				, PM_QOS_CLUSTER0_FREQ_MIN, FULLSEG_MIN_FREQ);
#endif
			g_pkt_length = TS0_32PKT_LENGTH;
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_START, 0);
			bbm_com_word_write(hInit, DIV_BROADCAST, BBM_BUF_TS0_END
					, TS0_32PKT_LENGTH - 1);
			bbm_com_word_write(hInit, DIV_BROADCAST, BBM_BUF_TS0_THR
					, TS0_32PKT_LENGTH / 2 - 1);
			print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
			   , TS0_32PKT_LENGTH / 2 - 1);
		}	else	{
#ifdef SEC_ENABLE_13SEG_BOOST
			if (pm_qos_request_active(&fc8350_cpu_handle)) {
				pr_info("[FC8350] 1seg don't need boost. remove!!\n");
				pm_qos_remove_request(&fc8350_cpu_handle);
			}
#endif
			g_pkt_length = TS0_5PKT_LENGTH;
			bbm_com_word_write(hInit, DIV_BROADCAST
				, BBM_BUF_TS0_START, 0);
			bbm_com_word_write(hInit, DIV_BROADCAST, BBM_BUF_TS0_END
				, TS0_5PKT_LENGTH - 1);
			bbm_com_word_write(hInit, DIV_BROADCAST, BBM_BUF_TS0_THR
				, TS0_5PKT_LENGTH / 2 - 1);
			print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
				, TS0_5PKT_LENGTH / 2 - 1);
		}

		isdbt_pdata->type = (u32)info.buff[1];

		print_log(hInit, "[FC8350] IOCTL_ISDBT_TUNER_SELECT %d\n"
			, (u32)info.buff[1]);
		break;
	case IOCTL_ISDBT_RF_BER:
		err = copy_from_user((void *)&info, (void *)arg, size);
		pr_err("[FC8350] IOCTL_ISDBT_RF_BER, CN(%d), BER_A(%d), BER_B(%d)\n",
			(u8)info.buff[0], (u32)info.buff[1], (u32)info.buff[2]);
		isdbt_pdata->BER[0] = (int)info.buff[1];
		isdbt_pdata->BER[1] = (int)info.buff[2];
		res = 0;
		break;
	case IOCTL_ISDBT_TS_START:
		pr_err("[FC8350] IOCTL_ISDBT_TS_START\n");
#ifdef TS_DROP_DEBUG
#ifdef FEATURE_TS_CHECK
		create_tspacket_anal();
		check_cnt_size = 0;
#endif
#endif
		hOpen->isdbttype = TS_TYPE;
		pr_err("[FC8350] IOCTL_ISDBT_TS_START - end\n");

		break;
	case IOCTL_ISDBT_TS_STOP:
		pr_err("[FC8350] IOCTL_ISDBT_TS_STOP\n");
		hOpen->isdbttype = 0;

#ifdef SEC_ENABLE_13SEG_BOOST
		if (pm_qos_request_active(&fc8350_cpu_handle)) {
			pr_info("[FC8350] disable boost!\n");
			pm_qos_remove_request(&fc8350_cpu_handle);
		}
#endif
		break;
	case IOCTL_ISDBT_POWER_ON:
		pr_err("[FC8350] IOCTL_ISDBT_POWER_ON\n");
		isdbt_hw_init();
		res = bbm_com_probe(hInit, DIV_BROADCAST);

		if (res) {
			pr_err("[FC8350] IOCTL_ISDBT_POWER_ON FAIL\n");
			isdbt_hw_deinit();
		} else {
			pr_err("[FC8350] IOCTL_ISDBT_POWER_ON SUCCESS\n");
		}
		break;
	case IOCTL_ISDBT_POWER_OFF:
		pr_err("[FC8350] IOCTL_ISDBT_POWER_OFF\n");
#ifdef SEC_ENABLE_13SEG_BOOST
		if (pm_qos_request_active(&fc8350_cpu_handle)) {
			pr_info("[FC8350] disable boost!!\n");
			pm_qos_remove_request(&fc8350_cpu_handle);
		}
#endif
		isdbt_hw_deinit();

		break;
	case IOCTL_ISDBT_SCAN_STATUS:
		pr_err("[FC8350] IOCTL_ISDBT_SCAN_STATUS\n");
		res = bbm_com_scan_status(hInit, DIV_BROADCAST);
		pr_err("[FC8350] IOCTL_ISDBT_SCAN_STATUS %d\n", res);
		break;
	case IOCTL_ISDBT_TUNER_GET_RSSI:

		err = copy_from_user((void *)&info, (void *)arg, size);
		res = bbm_com_tuner_get_rssi(hInit
			, DIV_BROADCAST, (s32 *)&info.buff[0]);
		err |= copy_to_user((void *)arg, (void *)&info, size);
		break;
	case TUNER_IOCTL_VALGET_OPENCNT:
		res = tuner_ioctl_get_open_count(filp->private_data,
				cmd,
				arg);
		break;

	case TUNER_IOCTL_VALSET_MONICNT:
		res = tuner_ioctl_set_monitor_mode(filp->private_data,
				cmd,
				arg);
		break;

	case IOCTL_ISDBT_TUNER_PKT_MODE:

		pr_err("[FC8350] IOCTL_ISDBT_TUNER_PKT_MODE\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		if (!err) {
			bbm_byte_write(hInit, DIV_MASTER
				, BBM_BUF_ENABLE, 0x00); /* buffer disable */

			if ((u32)info.buff[0] == ISDBT_INTERRUPT_32_PKT) {
				pr_err("[FC8350] IOCTL_ISDBT_TUNER_PKT_MODE ISDBT_INTERRUPT_32_PKT\n");
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_START, 0);
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_END
					, TS0_32PKT_LENGTH - 1);
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_THR
					, TS0_32PKT_LENGTH / 2 - 1);
				print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
				   , TS0_32PKT_LENGTH / 2 - 1);
			} else {
				pr_err("[FC8350] IOCTL_ISDBT_TUNER_PKT_MODE ISDBT_INTERRUPT_5_PKT\n");
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_START, 0);
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_END
					, TS0_5PKT_LENGTH - 1);
				bbm_com_word_write(hInit, DIV_BROADCAST
					, BBM_BUF_TS0_THR
					, TS0_5PKT_LENGTH / 2 - 1);
				print_log(hInit, "[FC8350] TUNER THRESHOLD: %d\n"
					, TS0_5PKT_LENGTH / 2 - 1);
			}

			bbm_byte_write(hInit, DIV_MASTER
				, BBM_BUF_ENABLE, 0x01); /* buffer enable */

			print_log(hInit, "[FC8350] IOCTL_ISDBT_TUNER_PKT_MODE %lu\n"
				, info.buff[0]);
		}
		res = err;
		break;
	case IOCTL_ISDBT_LTE_B28_STATUS:
		pr_err("[FC8350] IOCTL_ISDBT_LTE_B28_STATUS\n");
		err = copy_from_user((void *)&info, (void *)arg, size);
		if (!err && gpio_is_valid(isdbt_pdata->gpio_b28_ctrl)) {
			if ((u32)info.buff[0] == 1) { 
#if defined(CONFIG_SEC_GPIO_SETTINGS)
				isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
						, "isdbt_b28_set");
				if (IS_ERR(isdbt_pinctrl))
					pr_err("Target does not use pinctrl(isdbt_b28_set)\n");
#endif
				gpio_direction_output(isdbt_pdata->gpio_ant_ctrl1, isdbt_pdata->gpio_ant_ctrl_sel[4]);
				gpio_direction_output(isdbt_pdata->gpio_ant_ctrl2, isdbt_pdata->gpio_ant_ctrl_sel[5]);
			} else {
#if defined(CONFIG_SEC_GPIO_SETTINGS)
				isdbt_pinctrl = devm_pinctrl_get_select(isdbt_pdata->isdbt_device
						, "isdbt_b28_rel");
				if (IS_ERR(isdbt_pinctrl))
					pr_err("Target does not use pinctrl(isdbt_b28_rel)\n");
#endif

				gpio_direction_output(isdbt_pdata->gpio_ant_ctrl1, isdbt_pdata->gpio_ant_ctrl_sel[0]);
				gpio_direction_output(isdbt_pdata->gpio_ant_ctrl2, isdbt_pdata->gpio_ant_ctrl_sel[1]);
			}
			pr_err("[FC8350] B28 filter select %d\n", (u32)info.buff[0]);
			pr_err("%s, gpio_ant_ctrl1 = %d gpio_ant_ctrl2 = %d \n", __func__,
				gpio_get_value(isdbt_pdata->gpio_ant_ctrl1),
				gpio_get_value(isdbt_pdata->gpio_ant_ctrl2));
		} else
			pr_err("[FC8350] IOCTL_ISDBT_LTE_B28_STATUS - Not support\n");
		
		res = err;
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

#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA

static struct device isdbt_sysfs_dev = {
	.init_name = "isdbt",
};

static ssize_t isdbt_signal_source_store(struct device *dev
			     , struct device_attribute *attr
			     , const char *buf, size_t size)
{
	int state;

	if (sysfs_streq(buf, "1"))
			state = 1;
	else if (sysfs_streq(buf, "0"))
			state = 0;
	else {
			pr_err("%s - invalid value %d\n", __func__, *buf);
			return -EINVAL;
	}

	pr_err("%s: state:%u system_rev:%u\n"
		, __func__, state, system_rev);


	/*Rev 0.3 gpio: F-type cable:1, Antenna:0 */

	if (state == 0) {
		pr_err("%s: state:%u Enabling F type cable by setting TMM_SW to HIGH\n"
			, __func__, state);
		gpio_set_value_cansleep(isdbt_pdata->gpio_tmm_sw, 1);
	} else if (state == 1) {
		pr_err("%s: state:%u Enabling antenna by setting TMM_SW to LOW\n"
			, __func__, state);
		gpio_set_value_cansleep(isdbt_pdata->gpio_tmm_sw, 0);
	} else {
		return -EINVAL;
	}

	return size;
}
/*
static ssize_t isdbt_signal_source_show(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	int ret;
	ret = gpio_get_value_cansleep(isdbt_pdata->gpio_tmm_sw);
	pr_err("%s: gpio_tmm_sw state:%d\n", __func__, ret);
	return snprintf(buf, PAGE_SIZE, "%d\n", ret);
}
*/
static DEVICE_ATTR(isdbt_signal_source
			, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP)
			, NULL, isdbt_signal_source_store);
#endif

static struct isdbt_platform_data *isdbt_populate_dt_pdata(struct device *dev)
{
	struct isdbt_platform_data *pdata;
	const char *temp_string = NULL;
	int ret = 0;
	int len;

	pr_err("%s\n", __func__);
	pdata =  devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s : could not allocate memory for platform data\n"
			, __func__);
		goto err;
	}

	of_property_read_u32(dev->of_node
		, "isdbt,isdb-bbm-xtal-freq", &bbm_xtal_freq);
	if (bbm_xtal_freq < 0)	{
		pr_err("%s : can not find the isdbt-bbmxtal-freq in the dt, set to : 32000\n"
			, __func__);
		bbm_xtal_freq = 32000;
	}

	pdata->gpio_rst = of_get_named_gpio(dev->of_node, "isdbt,gpio-tunner-rst", 0);
	if (pdata->gpio_rst < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-tunner-rst", &pdata->gpio_rst);
	if (pdata->gpio_rst < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-tunner-rst in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio-tunner-rst =%d\n"
			, __func__, pdata->gpio_rst);
	}

	pdata->gpio_en = of_get_named_gpio(dev->of_node, "isdbt,gpio-tunner-en", 0);
	if (pdata->gpio_en < 0)
		of_property_read_u32(dev->of_node, "isdbt,gpio-tunner-en"
			, &pdata->gpio_en);
	if (pdata->gpio_en < 0)	{
		pr_err("%s : can not find the isdbt-detect-gpio-tunner-en in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio-tunner-en =%d\n"
			, __func__, pdata->gpio_en);
	}

	pdata->gpio_lna_en = of_get_named_gpio(dev->of_node, "isdbt,gpio-lna-en", 0);
	if (pdata->gpio_lna_en < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-lna-en", &pdata->gpio_lna_en);
	if (pdata->gpio_lna_en < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-lna-en in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_lna_en =%d\n"
			, __func__, pdata->gpio_lna_en);
	}

	/*B28 filter for latin DTV*/
	pdata->gpio_b28_ctrl = of_get_named_gpio(dev->of_node, "isdbt,gpio-b28-ctrl", 0);
	if (pdata->gpio_b28_ctrl < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-b28-ctrl", &pdata->gpio_b28_ctrl);
	if (pdata->gpio_b28_ctrl < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-b28-ctrl in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio-b28-ctrl =%d\n"
			, __func__, pdata->gpio_b28_ctrl);
	}

	/* ant switch for FM/DTV/DTV with B28 filter*/
	pdata->gpio_ant_ctrl1 = of_get_named_gpio(dev->of_node, "isdbt,gpio-ant-ctrl1", 0);
	if (pdata->gpio_ant_ctrl1 < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-ant-ctrl1", &pdata->gpio_ant_ctrl1);
	if (pdata->gpio_ant_ctrl1 < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-ant-ctrl1 in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio-ant-ctrl1 =%d\n"
			, __func__, pdata->gpio_ant_ctrl1);
	}

	pdata->gpio_ant_ctrl2 = of_get_named_gpio(dev->of_node, "isdbt,gpio-ant-ctrl2", 0);
	if (pdata->gpio_ant_ctrl2 < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-ant-ctrl2", &pdata->gpio_ant_ctrl2);
	if (pdata->gpio_ant_ctrl2 < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-ant-ctrl2 in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio-ant-ctrl2 =%d\n"
			, __func__, pdata->gpio_ant_ctrl2);
	}
	if (gpio_is_valid(pdata->gpio_ant_ctrl1) && gpio_is_valid(pdata->gpio_ant_ctrl1)) {
		if (!of_get_property(dev->of_node, "isdbt,gpio-ant-ctrl-sel", &len)) {
			pr_err("%s : can not find size of the gpio-ant-ctrl-sel in the dt\n"
				, __func__);
			goto alloc_err;
		}
		len = len / sizeof(u32);
		ret = of_property_read_u32_array(dev->of_node
			, "isdbt,gpio-ant-ctrl-sel", pdata->gpio_ant_ctrl_sel, len);
		if (ret) {
			pr_err("%s : can not find the gpio-ant-ctrl-sel in the dt\n"
				, __func__);
			goto alloc_err;
		}
	}

	pdata->gpio_power_en = of_get_named_gpio(dev->of_node
		, "isdbt,isdb-gpio-pwr-en", 0);
	if (pdata->gpio_power_en < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-pwr-en"
			, &pdata->gpio_power_en);
	if (pdata->gpio_power_en < 0)	{
		pr_err("%s : can not find the isdbt-detect-gpio gpio_power_en in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_power_en =%d\n"
			, __func__, pdata->gpio_power_en);
	}

	pdata->gpio_cp_dt = of_get_named_gpio(dev->of_node
		, "isdbt,isdb-cp-detect", 0);
	if (pdata->gpio_cp_dt < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,isdb-cp-detect", &pdata->gpio_cp_dt);
	if (pdata->gpio_cp_dt < 0)
		pr_err("%s : can not find the isdb-cp-detect gpio_cp_dt in the dt\n"
			, __func__);
	else
		pr_err("%s : isdbt-detect-gpio gpio_cp_dt =%d\n"
			, __func__, pdata->gpio_cp_dt);

	pdata->gpio_ant_ctrl = of_get_named_gpio(dev->of_node, "isdbt,gpio-fm-dtv-ctrl", 0);
	if (pdata->gpio_ant_ctrl < 0)
		of_property_read_u32(dev->of_node, "isdbt,gpio-fm-dtv-ctrl"
			, &pdata->gpio_ant_ctrl);
	if (pdata->gpio_ant_ctrl < 0)	{
		pr_err("%s : can not find the isdbt-detect-gpio gpio-fm-dtv-ctrl in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio gpio-fm-dtv-ctrl =%d\n"
			, __func__, pdata->gpio_ant_ctrl);
	}

	pdata->gpio_dtv_check = of_get_named_gpio(dev->of_node, "isdbt,gpio-dtv-check", 0);
	if (pdata->gpio_dtv_check < 0)
		of_property_read_u32(dev->of_node, "isdbt,gpio-dtv-check"
			, &pdata->gpio_dtv_check);
	if (pdata->gpio_dtv_check < 0)	{
		pr_err("%s : can not find the isdbt-detect-gpio gpio-dtv-check in the dt\n"
			, __func__);
	} else {
		pr_err("%s : isdbt-detect-gpio gpio-dtv-check =%d\n"
			, __func__, pdata->gpio_dtv_check);
	}

/*
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
	pdata->gpio_init = pinctrl_lookup_state(pdata->isdb_pinctrl
							, "isdb_gpio_init");
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
	pdata->gpio_int = of_get_named_gpio(dev->of_node
							, "isdbt,gpio-tunner-irq", 0);
	if (pdata->gpio_int < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,gpio-tunner-irq", &pdata->gpio_int);
	if (pdata->gpio_int < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio-tunner-irq in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio-tunner-irq =%d\n"
			, __func__, pdata->gpio_int);
	}
#endif
#if defined(BBM_I2C_SPI) || defined(BBM_I2C_TSIF)
	pdata->gpio_i2c_sda = of_get_named_gpio(dev->of_node
								, "isdbt,isdb-gpio-i2c_sda", 0);
	if (pdata->gpio_i2c_sda < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,isdb-gpio-i2c_sda", &pdata->gpio_i2c_sda);
	if (pdata->gpio_i2c_sda < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_i2c_sda in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_i2c_sda=%d\n"
			, __func__, pdata->gpio_i2c_sda);
	}

	pdata->gpio_i2c_scl = of_get_named_gpio(dev->of_node
		, "isdbt,isdb-gpio-i2c_scl", 0);
	if (pdata->gpio_i2c_scl < 0)
		of_property_read_u32(dev->of_node
			, "isdbt,isdb-gpio-i2c_scl", &pdata->gpio_i2c_scl);
	if (pdata->gpio_i2c_scl < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_i2c_scl in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_i2c_scl=%d\n"
			, __func__, pdata->gpio_i2c_scl);
	}
#endif
	pdata->gpio_spi_do = of_get_named_gpio(dev->of_node
		, "isdbt,isdb-gpio-spi_do", 0);
	if (pdata->gpio_spi_do < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio in the gpio_spi_do dt\n"
			, __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_do =%d\n"
			, __func__, pdata->gpio_spi_do);

	pdata->gpio_spi_di = of_get_named_gpio(dev->of_node
								, "isdbt,isdb-gpio-spi_di", 0);
	if (pdata->gpio_spi_di < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio in the gpio_spi_di dt\n"
			, __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_di =%d\n"
			, __func__, pdata->gpio_spi_di);

	pdata->gpio_spi_cs = of_get_named_gpio(dev->of_node
								, "isdbt,isdb-gpio-spi_cs", 0);
	if (pdata->gpio_spi_cs < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_spi_cs in the dt\n"
			, __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_cs=%d\n"
			, __func__, pdata->gpio_spi_cs);

	pdata->gpio_spi_clk = of_get_named_gpio(dev->of_node
								, "isdbt,isdb-gpio-spi_clk", 0);
	if (pdata->gpio_spi_clk < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_spi_clk in the dt\n"
			, __func__);
		goto alloc_err;
	} else
		pr_err("%s : isdbt-detect-gpio gpio_spi_clk=%d\n"
			, __func__, pdata->gpio_spi_clk);

#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
	pdata->gpio_tmm_sw = of_get_named_gpio(dev->of_node
								, "isdbt,isdb-gpio-tmm_sw", 0);
	if (pdata->gpio_tmm_sw < 0)
		of_property_read_u32(dev->of_node, "isdbt,isdb-gpio-tmm_sw"
			, &pdata->gpio_tmm_sw);
	if (pdata->gpio_tmm_sw < 0) {
		pr_err("%s : can not find the isdbt-detect-gpio gpio_tmm_sw in the dt\n"
			, __func__);
		goto alloc_err;
	} else {
		pr_err("%s : isdbt-detect-gpio gpio_tmm_sw=%d\n"
			, __func__, pdata->gpio_tmm_sw);
	}
#endif

	ret = of_property_read_string(dev->of_node
				, "clock-names", &temp_string);
	if (ret) {
		pr_err("%s: cannot get clock name(%d)\n", __func__, ret);
		pdata->isdbt_clk = NULL;
	} else {
		pdata->isdbt_clk = clk_get(dev, temp_string);
		if (pdata->isdbt_clk < 0) {
			pr_err("%s: cannot get clock\n", __func__);
		}
	}
	if (of_property_read_string(dev->of_node, "isdbt,ldo_vdd_1p8",
		&pdata->ldo_vdd_1p8) < 0) {
		pr_err("%s: get ldo_vdd_1p8 error\n", __func__);
		pdata->ldo_vdd_1p8 = NULL;
	}

	return pdata;
alloc_err:
	devm_kfree(dev, pdata);
err:
	return NULL;
}

static int isdbt_probe(struct platform_device *pdev)
{
	int res = 0;
	static struct class *isdbt_class;

	pr_err("%s\n", __func__);

	open_cnt         = 0;

	isdbt_pdata = isdbt_populate_dt_pdata(&pdev->dev);
	if (!isdbt_pdata) {
		pr_err("%s : isdbt_pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	/*support dtv model check GPIO*/
	if (gpio_is_valid(isdbt_pdata->gpio_dtv_check)) {
		if (gpio_get_value(isdbt_pdata->gpio_dtv_check)) {
			pr_info("%s : dtv support model\n", __func__);
		}else{
			pr_info("%s : dtv not support model\n", __func__);
			return -ENXIO;
		}
	}

#ifdef CONFIG_SEC_ISDBT_FORCE_OFF
	g_isdbt_wq = create_singlethread_workqueue
		("isdbt_force_power_off");
	INIT_WORK(&g_isdbt_force_off_work, isdbt_force_off_func);
	isdbt_force_off_callback = isdbt_force_off;
#endif

#if defined(CONFIG_SEC_GPIO_SETTINGS)
	/* Get pinctrl if target uses pinctrl */
	isdbt_pdata->isdbt_device = &pdev->dev;
#endif

	res = misc_register(&fc8350_misc_device);

	if (res < 0) {
		pr_err("isdbt init fail : %d\n", res);
		return res;
	}

	hInit = kmalloc(sizeof(struct ISDBT_INIT_INFO_T), GFP_KERNEL);
	isdbt_gpio_init(hInit);

#if defined(BBM_I2C_TSIF) || defined(BBM_I2C_SPI)
	res = bbm_com_hostif_select(hInit, BBM_I2C);
	pr_err("isdbt host interface select BBM_I2C!\n");
#else
	pr_err("isdbt host interface select BBM_SPI !\n");
	res = bbm_com_hostif_select(hInit, BBM_SPI);
#endif
	if (res)
		pr_err("isdbt host interface select fail!\n");

	bbm_com_ts_callback_register(hInit, data_callback);

#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
	res = device_register(&isdbt_sysfs_dev);
	if (res) {
		pr_err("[W1] error register isdbt_sysfs_dev device\n");
	} else {
		res = sysfs_create_file(&isdbt_sysfs_dev.kobj
					, &dev_attr_isdbt_signal_source.attr);
		if (res < 0)
			pr_err("couldn't create sysfs for F-type cable\n");
		else
			pr_err("created sysfs for F-type cable\n");
	}
#endif

	INIT_LIST_HEAD(&(hInit->hHead));

	isdbt_class = class_create(THIS_MODULE, "isdbt");
	if (IS_ERR(isdbt_class)) {
		pr_err("%s : class_create failed!\n", __func__);
	} else {
		res = class_create_file(isdbt_class, &class_attr_ber);
		if (res)
			pr_err("%s : failed to create device file in sysfs entries!\n"
				, __func__);
	}

	wake_lock_init(&isdbt_wlock, WAKE_LOCK_SUSPEND, "isdbt_wlock");

	static_ringbuffer = kzalloc(RING_BUFFER_SIZE,
					GFP_KERNEL);

	if (static_ringbuffer == NULL)
		pr_err("%s : failed to allocate memory!\n", __func__);

	return 0;
}
static int isdbt_remove(struct platform_device *pdev)
{
	pr_err("ISDBT remove\n");
	return 0;
}

static int isdbt_suspend(struct platform_device *pdev, pm_message_t mesg)
{
#if 0
	int value;
	value = gpio_get_value_cansleep(isdbt_pdata->gpio_power_en);

	pr_err("%s  value = %d\n", __func__, value);
	if (value == 1)
		gpio_direction_output(isdbt_pdata->gpio_power_en, 0);
#endif
	return 0;
}

static int isdbt_resume(struct platform_device *pdev)
{
	return 0;
}

static void isdbt_shutdown(struct platform_device *pdev)
{
	if (open_cnt <= 0) {
		open_cnt = 0;
		return;
	} else {
		pr_err("%s\n", __func__);
		open_cnt--;
	}

	if (open_cnt == 0) {
		wake_unlock(&isdbt_wlock);
		if (driver_mode != ISDBT_POWEROFF)
			isdbt_hw_deinit();
	}

	if (static_ringbuffer != NULL)
		kfree(static_ringbuffer);
}

static const struct of_device_id isdbt_match_table[] = {
	{   .compatible = "isdbt_pdata",
	},
	{}
};

static struct platform_driver isdb_fc8350_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name = "isdbt",
		.of_match_table = isdbt_match_table,
	},
	.probe	= isdbt_probe,
	.remove = isdbt_remove,
	.suspend = isdbt_suspend,
	.resume = isdbt_resume,
	.shutdown = isdbt_shutdown,
};

int __init isdbt_init(void)
{
	s32 res;

	pr_err("isdbt_fc8350_init started\n");
	res = platform_driver_register(&isdb_fc8350_driver);
	if (res < 0) {
		pr_err("isdbt init fail : %d\n", res);
		return res;
	}

	return 0;
}

void __exit isdbt_exit(void)
{
	pr_err("isdb_fc8350_exit\n");


#ifndef BBM_I2C_TSIF
	free_irq(gpio_to_irq(isdbt_pdata->gpio_int), NULL);
	gpio_free(isdbt_pdata->gpio_int);
#endif
	if (gpio_is_valid(isdbt_pdata->gpio_rst))
		gpio_free(isdbt_pdata->gpio_rst);
	if (gpio_is_valid(isdbt_pdata->gpio_power_en))
		gpio_free(isdbt_pdata->gpio_power_en);
	if (gpio_is_valid(isdbt_pdata->gpio_en))
		gpio_free(isdbt_pdata->gpio_en);
	if (gpio_is_valid(isdbt_pdata->gpio_lna_en))
		gpio_free(isdbt_pdata->gpio_lna_en);
	bbm_com_ts_callback_deregister();
#ifdef CONFIG_ISDBT_F_TYPE_ANTENNA
	gpio_free(isdbt_pdata->gpio_tmm_sw);
	sysfs_remove_file(&isdbt_sysfs_dev.kobj
		, &dev_attr_isdbt_signal_source.attr);
	device_unregister(&isdbt_sysfs_dev);
#endif
	bbm_com_hostif_deselect(hInit);
	isdbt_hw_deinit();
	platform_driver_unregister(&isdb_fc8350_driver);
	misc_deregister(&fc8350_misc_device);

	if (hInit != NULL)
		kfree(hInit);

}

module_init(isdbt_init);
module_exit(isdbt_exit);

MODULE_LICENSE("Dual BSD/GPL");
