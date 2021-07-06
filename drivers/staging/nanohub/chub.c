/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *
 * Boojin Kim <boojin.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/iio/iio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/random.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/timekeeping.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_EXYNOS_ITMON
#include <soc/samsung/exynos-itmon.h>
#endif

#include <soc/samsung/exynos-pmu.h>

#ifdef CONFIG_CHRE_SENSORHUB_HAL
#include "main.h"
#endif
#include "bl.h"
#include "comms.h"
#include "chub.h"
#include "chub_ipc.h"
#include "chub_dbg.h"
#include "../../soc/samsung/cal-if/pmucal_shub.h"
#ifdef CONFIG_SENSORS_SSP
#include <linux/gpio.h>
#include "../../sensorhub/ssp_platform.h"
#endif

#define WAIT_TIMEOUT_MS (1000)
#define HW_RESET_WAIT_TIMEOUT_MS (1)

enum { CHUB_ON, CHUB_OFF };
enum { C2A_ON, C2A_OFF };

static DEFINE_MUTEX(reset_mutex);
static DEFINE_MUTEX(pmu_shutdown_mutex);

#ifdef CONFIG_SENSORS_SSP
int contexthub_get_token(struct contexthub_ipc_info *ipc)
#else
static int contexthub_get_token(struct contexthub_ipc_info *ipc)
#endif
{
	if (atomic_read(&ipc->in_reset))
		return -EINVAL;

	atomic_inc(&ipc->in_use_ipc);
	return 0;
}

#ifdef CONFIG_SENSORS_SSP
void contexthub_put_token(struct contexthub_ipc_info *ipc)
#else
static void contexthub_put_token(struct contexthub_ipc_info *ipc)
#endif
{
	atomic_dec(&ipc->in_use_ipc);
}

/* host interface functions */
int contexthub_is_run(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return 1;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return nanohub_irq1_fired(ipc->data);
#else
	return 1;
#endif
}

/* request contexthub to host driver */
int contexthub_request(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return 0;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return request_wakeup_timeout(ipc->data, WAIT_TIMEOUT_MS);
#else
	return 0;
#endif
}

/* rlease contexthub to host driver */
void contexthub_release(struct contexthub_ipc_info *ipc)
{
	if (!ipc->powermode)
		return;

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	release_wakeup(ipc->data);
#endif
}

static inline void contexthub_notify_host(struct contexthub_ipc_info *ipc)
{
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	nanohub_handle_irq1(ipc->data);
#else
	/* TODO */
#endif
}

#ifdef CONFIG_CHRE_SENSORHUB_HAL
/* by nanohub kernel RxBufStruct. packet header is 10 + 2 bytes to align */
struct rxbuf {
	u8 pad;
	u8 pre_preamble;
	u8 buf[PACKET_SIZE_MAX];
	u8 post_preamble;
};

static int nanohub_mailbox_open(void *data)
{
	return 0;
}

static void nanohub_mailbox_close(void *data)
{
	(void)data;
}

static int nanohub_mailbox_write(void *data, uint8_t *tx, int length,
				 int timeout)
{
	struct nanohub_data *ipc = data;

	return contexthub_ipc_write(ipc->pdata->mailbox_client, tx, length, timeout);
}

static int nanohub_mailbox_read(void *data, uint8_t *rx, int max_length,
				int timeout)
{
	struct nanohub_data *ipc = data;

	return contexthub_ipc_read(ipc->pdata->mailbox_client, rx, max_length, timeout);
}

void nanohub_mailbox_comms_init(struct nanohub_comms *comms)
{
	comms->seq = 1;
	comms->timeout_write = 544;
	comms->timeout_ack = 272;
	comms->timeout_reply = 512;
	comms->open = nanohub_mailbox_open;
	comms->close = nanohub_mailbox_close;
	comms->write = nanohub_mailbox_write;
	comms->read = nanohub_mailbox_read;
}
#endif

static int contexthub_read_process(uint8_t *rx, u8 *raw_rx, u32 size)
{
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct rxbuf *rxstruct;
	struct nanohub_packet *packet;

	rxstruct = (struct rxbuf *)raw_rx;
	packet = (struct nanohub_packet *)&rxstruct->pre_preamble;
	memcpy_fromio(rx, (void *)packet, size);

	return NANOHUB_PACKET_SIZE(packet->len);
#else
	memcpy_fromio(rx, (void *)raw_rx, size);
	return size;
#endif
}

static int contexthub_ipc_drv_init(struct contexthub_ipc_info *chub)
{
	struct device *chub_dev = chub->dev;
	int ret = 0;

	chub->ipc_map = ipc_get_chub_map();
	if (!chub->ipc_map)
		return -EINVAL;

	/* init debug-log */
	/* HACK for clang */
	chub->ipc_map->logbuf.eq = 0;
	chub->ipc_map->logbuf.dq = 0;
	chub->fw_log = log_register_buffer(chub_dev, 0,
					   (void *)&chub->ipc_map->logbuf,
					   "fw", 1);
	if (!chub->fw_log)
		return -EINVAL;

	if (chub->irq_pin_len) {
		int i;

		for (i = 0; i < chub->irq_pin_len; i++) {
			u32 irq = gpio_to_irq(chub->irq_pins[i]);

			disable_irq_nosync(irq);
			dev_info(chub_dev, "%s: %d irq (pin:%d) is for chub. disable it\n",
				__func__, irq, chub->irq_pins[i]);
		}
	}

#ifdef LOWLEVEL_DEBUG
	chub->dd_log_buffer = vmalloc(SZ_256K + sizeof(struct LOG_BUFFER *));
	chub->dd_log_buffer->index_reader = 0;
	chub->dd_log_buffer->index_writer = 0;
	chub->dd_log_buffer->size = SZ_256K;
	chub->dd_log =
	    log_register_buffer(chub_dev, 1, chub->dd_log_buffer, "dd", 0);
#endif
	ret = chub_dbg_init(chub);
	if (ret)
		dev_err(chub_dev, "%s: fails. ret:%d\n", __func__, ret);

	return ret;
}

#ifdef PACKET_LOW_DEBUG
static void debug_dumpbuf(unsigned char *buf, int len)
{
	print_hex_dump(KERN_CONT, "", DUMP_PREFIX_OFFSET, 16, 1, buf, len,
		       false);
}
#endif

static inline int get_recv_channel(struct recv_ctrl *recv)
{
	int i;
	unsigned long min_order = 0;
	int min_order_evt = INVAL_CHANNEL;

	for (i = 0; i < IPC_BUF_NUM; i++) {
		if (recv->container[i]) {
			if (!min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			} else if (recv->container[i] < min_order) {
				min_order = recv->container[i];
				min_order_evt = i;
			}
		}
	}

	if (min_order_evt != INVAL_CHANNEL)
		recv->container[min_order_evt] = 0;

	return min_order_evt;
}

static inline bool read_is_locked(struct contexthub_ipc_info *ipc)
{
	return atomic_read(&ipc->read_lock.cnt) != 0;
}

static inline void read_get_locked(struct contexthub_ipc_info *ipc)
{
	atomic_inc(&ipc->read_lock.cnt);
}

static inline void read_put_unlocked(struct contexthub_ipc_info *ipc)
{
	atomic_dec(&ipc->read_lock.cnt);
}

/* simple alive check function : don't use ipc map */
static bool contexthub_lowlevel_alive(struct contexthub_ipc_info *ipc)
{
	int val;

	ipc->chub_alive_lock.flag = 0;
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);
	val = wait_event_timeout(ipc->chub_alive_lock.event,
				 ipc->chub_alive_lock.flag,
				 msecs_to_jiffies(200));

	return ipc->chub_alive_lock.flag;
}

#define CHUB_RESET_THOLD (5)
/* handle errors of chub driver and fw  */
static void handle_debug_work(struct contexthub_ipc_info *ipc, enum chub_err_type err)
{
	int need_reset;
	int alive = contexthub_lowlevel_alive(ipc);

	dev_info(ipc->dev, "%s: err:%d, alive:%d, status:%d, in-reset:%d\n",
		__func__, err, alive, __raw_readl(&ipc->chub_status),
		__raw_readl(&ipc->in_reset));
	if ((atomic_read(&ipc->chub_status) == CHUB_ST_ERR) || !alive)
		need_reset = 1;

	/* reset */
	if (need_reset) {
#if defined(CHUB_RESET_ENABLE)
		int ret;

		dev_info(ipc->dev, "%s: request silent reset. err:%d, alive:%d, status:%d, in-reset:%d\n",
			__func__, err, alive, __raw_readl(&ipc->chub_status),
			__raw_readl(&ipc->in_reset));
		ret = contexthub_reset(ipc, 0, err);
		if (ret)
			dev_warn(ipc->dev, "%s: fails to reset:%d. status:%d\n",
				__func__, ret, __raw_readl(&ipc->chub_status));
		else
			dev_info(ipc->dev, "%s: chub reset! should be recovery\n",
				__func__);
#else
		dev_info(ipc->dev, "%s: chub hang. wait for sensor driver reset\n",
			__func__, err, alive, __raw_readl(&ipc->chub_status),
			__raw_readl(&ipc->in_reset));

		atomic_set(&ipc->chub_status, CHUB_ST_HANG);
#endif
	}
}

static void contexthub_handle_debug(struct contexthub_ipc_info *ipc,
	enum chub_err_type err,	bool enable_wq)
{
	dev_info(ipc->dev, "%s: err:%d(cnt:%d), enable_wq:%d\n",
		__func__, err, ipc->err_cnt[err], enable_wq);

	if (err < CHUB_ERR_NEED_RESET) {
		if (err < CHUB_ERR_CRITICAL || ipc->err_cnt[err] > CHUB_RESET_THOLD) {
			atomic_set(&ipc->chub_status, CHUB_ST_ERR);
			ipc->err_cnt[err] = 0;
			dev_info(ipc->dev, "%s: err:%d(cnt:%d), enter error status\n",
				__func__, err, ipc->err_cnt[err]);
		} else {
			ipc->err_cnt[err]++;
			return;
		}
	}

	/* get chub-fw err */
	if (err == CHUB_ERR_NANOHUB) {
		enum ipc_debug_event fw_evt;

		if (contexthub_get_token(ipc)) {
			dev_warn(ipc->dev, "%s: get token\n", __func__);
			return;
		}
		fw_evt = ipc_read_debug_event(AP);
		if (fw_evt == IPC_DEBUG_CHUB_FAULT)
			err = CHUB_ERR_FW_FAULT;
		else if ((fw_evt == IPC_DEBUG_CHUB_ASSERT) || (fw_evt == IPC_DEBUG_CHUB_ERROR))
			err = CHUB_ERR_FW_ERROR;
		else
			dev_warn(ipc->dev, "%s: unsupported fw_evt: %d\n", fw_evt);

		ipc_write_debug_event(AP, 0);
		contexthub_put_token(ipc);
	}

	/* set status in CHUB_ST_ERR */
	if ((err == CHUB_ERR_ITMON) || (err == CHUB_ERR_FW_WDT) || (err == CHUB_ERR_FW_FAULT))
		atomic_set(&ipc->chub_status, CHUB_ST_ERR);

	/* handle err */
	if (mutex_is_locked(&reset_mutex) || enable_wq) {
		ipc->cur_err |= (1 << err);
		schedule_work(&ipc->debug_work);
	} else {
		handle_debug_work(ipc, err);
	}
}

static DEFINE_MUTEX(dbg_mutex);
static void handle_debug_work_func(struct work_struct *work)
{
	struct contexthub_ipc_info *ipc =
	    container_of(work, struct contexthub_ipc_info, debug_work);
	int i;

	dev_info(ipc->dev, "%s: cur_err:0x%x\n", __func__, ipc->cur_err);
	for (i = 0; i < CHUB_ERR_MAX; i++) {
		if (ipc->cur_err & (1 << i)) {
			dev_info(ipc->dev, "%s: loop: err:%d, cur_err:0x%x\n", __func__, i, ipc->cur_err);
			handle_debug_work(ipc, i);
			ipc->cur_err &= ~(1 << i);
		}
	}
}

static inline void clear_err_cnt(struct contexthub_ipc_info *ipc, enum chub_err_type err)
{
	if (ipc->err_cnt[err])
		ipc->err_cnt[err] = 0;
}

int contexthub_ipc_read(struct contexthub_ipc_info *ipc, uint8_t *rx, int max_length,
				int timeout)
{
	unsigned long flag;
	int size = 0;
	int ret = 0;
	void *rxbuf;

	if (!ipc->read_lock.flag) {
		spin_lock_irqsave(&ipc->read_lock.event.lock, flag);
		read_get_locked(ipc);
		ret =
			wait_event_interruptible_timeout_locked(ipc->read_lock.event,
								ipc->read_lock.flag,
								msecs_to_jiffies(timeout));
		read_put_unlocked(ipc);
		spin_unlock_irqrestore(&ipc->read_lock.event.lock, flag);
		if (ret < 0)
			dev_warn(ipc->dev,
				 "fails to get read ret:%d timeout:%d, flag:0x%x",
				 ret, timeout, ipc->read_lock.flag);

		if (!ipc->read_lock.flag)
			goto fail_get_channel;
	}

	ipc->read_lock.flag--;

	if (contexthub_get_token(ipc)) {
		dev_warn(ipc->dev, "no-active: read fails\n");
		return 0;
	}

	rxbuf = ipc_read_data(IPC_DATA_C2A, &size);

	if (size > 0) {
		clear_err_cnt(ipc, CHUB_ERR_READ_FAIL);
		ret = contexthub_read_process(rx, rxbuf, size);
	}
	contexthub_put_token(ipc);
	return ret;

fail_get_channel:
	contexthub_handle_debug(ipc, CHUB_ERR_READ_FAIL, 0);
	return -EINVAL;
}

int contexthub_ipc_write(struct contexthub_ipc_info *ipc,
				uint8_t *tx, int length, int timeout)
{
	int ret;

	if (contexthub_get_token(ipc)) {
		dev_warn(ipc->dev, "no-active: write fails\n");
		return 0;
	}

	ret = ipc_write_data(IPC_DATA_A2C, tx, (u16)length);
	contexthub_put_token(ipc);
	if (ret) {
		pr_err("%s: fails to write data: ret:%d, len:%d errcnt:%d\n",
			__func__, ret, length, ipc->err_cnt[CHUB_ERR_WRITE_FAIL]);
		contexthub_handle_debug(ipc, CHUB_ERR_WRITE_FAIL, 0);
		length = 0;
	} else {
		clear_err_cnt(ipc, CHUB_ERR_WRITE_FAIL);
	}
	return length;
}

static void check_rtc_time(void)
{
	struct rtc_device *chub_rtc = rtc_class_open(CONFIG_RTC_SYSTOHC_DEVICE);
	struct rtc_device *ap_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	struct rtc_time chub_tm, ap_tm;
	time64_t chub_t, ap_t;

	rtc_read_time(ap_rtc, &chub_tm);
	rtc_read_time(chub_rtc, &ap_tm);

	chub_t = rtc_tm_sub(&chub_tm, &ap_tm);

	if (chub_t) {
		pr_info("nanohub %s: diff_time: %llu\n", __func__, chub_t);
		rtc_set_time(chub_rtc, &ap_tm);
	};

	chub_t = rtc_tm_to_time64(&chub_tm);
	ap_t = rtc_tm_to_time64(&ap_tm);
}

static int contexthub_hw_reset(struct contexthub_ipc_info *ipc,
				 enum mailbox_event event)
{
	u32 val;
	int trycnt = 0;
	int ret = 0;
	int i;

	dev_info(ipc->dev, "%s. status:%d\n",
		__func__, __raw_readl(&ipc->chub_status));

	/* clear ipc value */
	atomic_set(&ipc->wakeup_chub, CHUB_OFF);
	atomic_set(&ipc->irq1_apInt, C2A_OFF);
	atomic_set(&ipc->read_lock.cnt, 0x0);

	/* chub err init */
	for (i = 0; i < CHUB_ERR_MAX; i++) {
		if (i == CHUB_ERR_RESET_CNT)
			continue;
		ipc->err_cnt[i] = 0;
	}

	ipc->read_lock.flag = 0;
	ipc_hw_write_shared_reg(AP, ipc->os_load, SR_BOOT_MODE);
	ipc_set_chub_clk((u32)ipc->clkrate);
	ipc_set_chub_bootmode(BOOTMODE_COLD);
	ipc_set_chub_kernel_log(KERNEL_LOG_ON);

	switch (event) {
	case MAILBOX_EVT_POWER_ON:
#ifdef NEED_TO_RTC_SYNC
		check_rtc_time();
#endif
		if (atomic_read(&ipc->chub_status) == CHUB_ST_NO_POWER) {
			atomic_set(&ipc->chub_status, CHUB_ST_POWER_ON);

			/* enable Dump gpr */
			IPC_HW_WRITE_DUMPGPR_CTRL(ipc->chub_dumpgpr, 0x1);

#if defined(CONFIG_SOC_EXYNOS9610)
			/* cmu cm4 clock - gating */
			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | IGNORE_FORCE_PM_EN),
				     ipc->cmu_chub_qch +
				     REG_QCH_CON_CM4_SHUB_QCH);
#endif
			/* pmu reset-release on CHUB */
			val = __raw_readl(ipc->pmu_chub_reset +
					REG_CHUB_RESET_CHUB_OPTION);
			__raw_writel((val | CHUB_RESET_RELEASE_VALUE),
				     ipc->pmu_chub_reset +
				     REG_CHUB_RESET_CHUB_OPTION);

#if defined(CONFIG_SOC_EXYNOS9610)
			/* check chub cpu status */
			do {
				val = __raw_readl(ipc->pmu_chub_reset +
						REG_CHUB_RESET_CHUB_CONFIGURATION);
				msleep(HW_RESET_WAIT_TIMEOUT_MS);
				if (++trycnt > RESET_WAIT_TRY_CNT) {
					dev_warn(ipc->dev, "chub cpu status is not set correctly\n");
					break;
				}
			} while ((val & 0x1) == 0x0);

			/* cmu cm4 clock - release */
			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | IGNORE_FORCE_PM_EN | CLOCK_REQ),
				     ipc->cmu_chub_qch +
				    REG_QCH_CON_CM4_SHUB_QCH);

			val = __raw_readl(ipc->cmu_chub_qch +
					REG_QCH_CON_CM4_SHUB_QCH);
			val &= ~(IGNORE_FORCE_PM_EN | CLOCK_REQ | ENABLE);
			__raw_writel((val | CLOCK_REQ),
				     ipc->cmu_chub_qch +
				    REG_QCH_CON_CM4_SHUB_QCH);
#endif
		} else {
			ret = -EINVAL;
			dev_warn(ipc->dev,
				 "fails to contexthub power on. Status is %d\n",
				 atomic_read(&ipc->chub_status));
		}
		break;
	case MAILBOX_EVT_RESET:
		ret = pmucal_shub_reset_release();
		break;
	default:
		break;
	}

	if (ret)
		return ret;
	else {
		/* wait active */
		trycnt = 0;
		do {
			msleep(50);
			contexthub_ipc_write_event(ipc, MAILBOX_EVT_CHUB_ALIVE);
			if (++trycnt > WAIT_TRY_CNT)
				break;
		} while ((atomic_read(&ipc->chub_status) != CHUB_ST_RUN));

		if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN) {
			dev_info(ipc->dev, "%s done. contexthub status is %d\n",
				 __func__, atomic_read(&ipc->chub_status));
			return 0;
		} else {
			dev_warn(ipc->dev, "%s fails. contexthub status is %d\n",
				 __func__, atomic_read(&ipc->chub_status));
			atomic_set(&ipc->chub_status, CHUB_ST_NO_RESPONSE);
			contexthub_handle_debug(ipc, CHUB_ERR_CHUB_NO_RESPONSE, 0);
			return -ETIMEDOUT;
		}
	}
}

static void contexthub_config_init(struct contexthub_ipc_info *chub)
{
	/* BAAW-P-APM-CHUB for CHUB to access APM_CMGP. 1 window is used */
	if (chub->chub_baaw) {
		IPC_HW_WRITE_BAAW_CHUB0(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_start);
		IPC_HW_WRITE_BAAW_CHUB1(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_end);
		IPC_HW_WRITE_BAAW_CHUB2(chub->chub_baaw,
					chub->baaw_info.baaw_p_apm_chub_remap);
		IPC_HW_WRITE_BAAW_CHUB3(chub->chub_baaw, BAAW_RW_ACCESS_ENABLE);
	}

	/* enable mailbox ipc */
	ipc_set_base(chub->sram);
	ipc_set_owner(AP, chub->mailbox, IPC_SRC);
}

int contexthub_ipc_write_event(struct contexthub_ipc_info *ipc,
				enum mailbox_event event)
{
	u32 val;
	int ret = 0;
	int need_ipc = 0;

	switch (event) {
	case MAILBOX_EVT_INIT_IPC:
		ret = contexthub_ipc_drv_init(ipc);
		break;
	case MAILBOX_EVT_POWER_ON:
		ret = contexthub_hw_reset(ipc, event);
#if 0
		if (!ret)
			log_schedule_flush_all();
#endif
		break;
	case MAILBOX_EVT_RESET:
		if (atomic_read(&ipc->chub_shutdown)) {
			ret = contexthub_hw_reset(ipc, event);
		} else {
			dev_err(ipc->dev,
				"contexthub status isn't shutdown. fails to reset: %d, %d\n",
					atomic_read(&ipc->chub_shutdown),
					atomic_read(&ipc->chub_status));
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_SHUTDOWN:
		/* assert */
		if (ipc->block_reset) {
			/* pmu call assert */
			ret = pmucal_shub_reset_assert();
			if (ret) {
				pr_err("%s: reset assert fail\n", __func__);
				ipc->pmu_shub_status.reason = 2;
				return ret;
			}

			/* pmu call reset-release_config */
			ret = pmucal_shub_reset_release_config();
			if (ret) {
				pr_err("%s: reset release cfg fail\n", __func__);
				ipc->pmu_shub_status.reason = 3;
				return ret;
			}

			/* tzpc setting */
			ret = exynos_smc(SMC_CMD_CONN_IF,
				(EXYNOS_SHUB << 32) |
				EXYNOS_SET_CONN_TZPC, 0, 0);
			if (ret) {
				pr_err("%s: TZPC setting fail\n",
					__func__);
				ipc->pmu_shub_status.reason = 4;
				return -EINVAL;
			}
			dev_info(ipc->dev, "%s: tzpc setted\n", __func__);
				/* baaw config */
			contexthub_config_init(ipc);
		} else {
			val = __raw_readl(ipc->pmu_chub_reset +
					  REG_CHUB_CPU_STATUS);
			if (val & (1 << REG_CHUB_CPU_STATUS_BIT_STANDBYWFI)) {
				val = __raw_readl(ipc->pmu_chub_reset +
						  REG_CHUB_RESET_CHUB_CONFIGURATION);
				__raw_writel(val & ~(1 << 0),
						 ipc->pmu_chub_reset +
						 REG_CHUB_RESET_CHUB_CONFIGURATION);
			} else {
				dev_err(ipc->dev,
					"fails to shutdown contexthub. cpu_status: 0x%x\n",
					val);
				return -EINVAL;
			}
		}
		atomic_set(&ipc->chub_shutdown, 1);
		atomic_set(&ipc->chub_status, CHUB_ST_SHUTDOWN);
		break;
	case MAILBOX_EVT_CHUB_ALIVE:
		val = contexthub_lowlevel_alive(ipc);
		if (val) {
			atomic_set(&ipc->chub_status, CHUB_ST_RUN);
			dev_info(ipc->dev, "chub is alive");
			clear_err_cnt(ipc, CHUB_ERR_CHUB_NO_RESPONSE);
		} else {
			dev_err(ipc->dev,
				"chub isn't alive, should be reset. status:%d\n",
				atomic_read(&ipc->chub_status));
			ipc_dump_mailbox_sfr(&ipc->mailbox_sfr_dump);
			ret = -EINVAL;
		}
		break;
	case MAILBOX_EVT_ENABLE_IRQ:
		/* if enable, mask from CHUB IRQ, else, unmask from CHUB IRQ */
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_unmask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
	case MAILBOX_EVT_DISABLE_IRQ:
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INT);
		ipc_hw_mask_irq(AP, IRQ_EVT_C2A_INTCLR);
		break;
	default:
		need_ipc = 1;
		break;
	}

	if (need_ipc) {
		if (contexthub_get_token(ipc)) {
			dev_warn(ipc->dev, "%s event:%d/%d fails chub isn't active, status:%d, inreset:%d\n",
				__func__, event, MAILBOX_EVT_MAX, atomic_read(&ipc->chub_status), atomic_read(&ipc->in_reset));
			return -EINVAL;
		}

		/* handle ipc */
		switch (event) {
		case MAILBOX_EVT_ERASE_SHARED:
			memset(ipc_get_base(IPC_REG_SHARED), 0, ipc_get_offset(IPC_REG_SHARED));
			break;
		case MAILBOX_EVT_DUMP_STATUS:
			/* dump nanohub kernel status */
			dev_info(ipc->dev, "Request to dump chub fw status\n");
			ipc_write_debug_event(AP, (u32)MAILBOX_EVT_DUMP_STATUS);
			ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
			break;
		case MAILBOX_EVT_WAKEUP_CLR:
			if (atomic_read(&ipc->wakeup_chub) == CHUB_ON) {
				atomic_set(&ipc->wakeup_chub, CHUB_OFF);
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_WAKEUP_CLR);
			}
			break;
		case MAILBOX_EVT_WAKEUP:
			if (atomic_read(&ipc->wakeup_chub) == CHUB_OFF) {
				atomic_set(&ipc->wakeup_chub, CHUB_ON);
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_WAKEUP);
			}
			break;
		default:
			/* handle ipc utc */
			if ((int)event < IPC_DEBUG_UTC_MAX) {
				ipc->utc_run = event;
				if ((int)event == IPC_DEBUG_UTC_TIME_SYNC)
					check_rtc_time();
				ipc_write_debug_event(AP, (u32)event);
				ret = ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_DEBUG);
			}
			break;
		}
		contexthub_put_token(ipc);

		if (ret)
			ipc->err_cnt[CHUB_ERR_EVTQ_ADD]++;
	}
	return ret;
}

int contexthub_poweron(struct contexthub_ipc_info *ipc)
{
	int ret = 0;
	struct device *dev = ipc->dev;

	if (!atomic_read(&ipc->chub_status)) {
		ret = contexthub_download_image(ipc, IPC_REG_BL);
		if (ret) {
			dev_warn(dev, "fails to download bootloader\n");
			return ret;
		}

		ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_INIT_IPC);
		if (ret) {
			dev_warn(dev, "fails to init ipc\n");
			return ret;
		}

		ret = contexthub_download_image(ipc, IPC_REG_OS);
		if (ret) {
			dev_warn(dev, "fails to download kernel\n");
			return ret;
		}
		ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_POWER_ON);
		if (ret) {
			dev_warn(dev, "fails to poweron\n");
			return ret;
		}

		if (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)
			dev_info(dev, "contexthub power-on");
		else
			dev_warn(dev, "contexthub fails to power-on");
	} else {
		ret = -EINVAL;
	}
	return ret;
}

static int contexthub_download_and_check_image(struct contexthub_ipc_info *ipc, enum ipc_region reg)
{
	u32 *fw = vmalloc(ipc_get_offset(reg));
	int ret = 0;

	if (!fw)
		return contexthub_download_image(ipc, reg);

	memcpy_fromio(fw, ipc_get_base(reg), ipc_get_offset(reg));
	ret = contexthub_download_image(ipc, reg);
	if (ret) {
		dev_err(ipc->dev, "%s: download bl(%d) fails\n", __func__, reg == IPC_REG_BL);
		goto out;
	}

	ret = memcmp(fw, ipc_get_base(reg), ipc_get_offset(reg));
	if (ret) {
		int i;
		u32 *fw_image = (u32 *)ipc_get_base(reg);

		dev_err(ipc->dev, "%s: fw doens't match with size %d\n",
			__func__, ipc_get_offset(reg));
		for (i = 0; i < ipc_get_offset(reg) / 4; i++)
			if (fw[i] != fw_image[i]) {
				dev_err(ipc->dev, "fw[%d] %x -> wrong %x\n", i, fw_image[i], fw[i]);
				print_hex_dump(KERN_CONT, "before:", DUMP_PREFIX_OFFSET, 16, 1, &fw[i], 64, false);
				print_hex_dump(KERN_CONT, "after:", DUMP_PREFIX_OFFSET, 16, 1, &fw_image[i], 64, false);
				ret = -EINVAL;
				break;
			}
	}
out:
	dev_info(ipc->dev, "%s: download and checked bl(%d) ret:%d \n", __func__, reg == IPC_REG_BL, ret);
	vfree(fw);
	return ret;
}

void contexthub_dump_pmu_sfr(struct contexthub_ipc_info *ipc)
{
	exynos_pmu_read(TOP_BUS_SHUB_STATUS, &ipc->pmu_shub_status.top_bus_shub);
	exynos_pmu_read(TOP_PWR_SHUB_STATUS, &ipc->pmu_shub_status.top_pwr_shub);
	exynos_pmu_read(LOGIC_RESET_SHUB_STATUS, &ipc->pmu_shub_status.logic_reset_shub);
	exynos_pmu_read(RESET_OTP_SHUB_STATUS, &ipc->pmu_shub_status.reset_otp_shub);
	exynos_pmu_read(RESET_CMU_SHUB_STATUS, &ipc->pmu_shub_status.reset_cmu_shub);
	exynos_pmu_read(RESET_SUBCPU_SHUB_STATUS, &ipc->pmu_shub_status.reset_subcpu_shub);
	dev_err(ipc->dev, "================%s: reset fail at %d: ================", __func__,  ipc->pmu_shub_status.reason);
	dev_err(ipc->dev, "TOP_BUS_SHUB_STATUS     : 0x%08x, TOP_PWR_SHUB_STATUS      : 0x%08x",
			ipc->pmu_shub_status.top_bus_shub, ipc->pmu_shub_status.top_pwr_shub);
	dev_err(ipc->dev, "LOGIC_RESET_SHUB_STATUS : 0x%08x, RESET_OTP_SHUB_STATUS    : 0x%08x",
			ipc->pmu_shub_status.logic_reset_shub, ipc->pmu_shub_status.reset_otp_shub);
	dev_err(ipc->dev, "RESET_CMU_SHUB_STATUS   : 0x%08x, RESET_SUBCPU_SHUB_STATUS : 0x%08x",
			ipc->pmu_shub_status.reset_cmu_shub, ipc->pmu_shub_status.reset_subcpu_shub);
	dev_err(ipc->dev, "===========================================================================");
}

#define RESET_RETRY_THD (5)
int contexthub_reset(struct contexthub_ipc_info *ipc, bool force_load, int dump)
{
	int ret;
	int trycnt = 0;
	bool irq_disabled;
	int fail_cnt = 0;

	dev_info(ipc->dev, "%s: force:%d, status:%d, in-reset:%d, dump:%d, user:%d\n",
		__func__, force_load, atomic_read(&ipc->chub_status), atomic_read(&ipc->in_reset), dump, atomic_read(&ipc->in_use_ipc));

	mutex_lock(&reset_mutex);
	if (!force_load && (atomic_read(&ipc->chub_status) == CHUB_ST_RUN)) {
		mutex_unlock(&reset_mutex);
		dev_info(ipc->dev, "%s: out status:%d\n", __func__, atomic_read(&ipc->chub_status));
		return 0;
	}

	atomic_inc(&ipc->in_reset);
	__pm_stay_awake(&ipc->ws_reset);

reset_fail_retry:
	/* disable mailbox interrupt to prevent sram access during chub reset */
	disable_irq(ipc->irq_mailbox);
	irq_disabled = true;

	while (atomic_read(&ipc->in_use_ipc)) {
		msleep(WAIT_CHUB_MS);
		if (++trycnt > RESET_WAIT_TRY_CNT) {
			dev_info(ipc->dev, "%s: can't get lock. in_use_ipc: %d\n", __func__, atomic_read(&ipc->in_use_ipc));
			ipc->pmu_shub_status.reason = 1;
			ret = -EINVAL;
			goto out;
		}
		dev_info(ipc->dev, "%s: wait for ipc user free: %d\n", __func__, atomic_read(&ipc->in_use_ipc));
	}

	if (dump) {
		ipc->err_cnt[CHUB_ERR_NONE] = dump;
		chub_dbg_dump_hw(ipc, ipc->cur_err);
	}

	dev_info(ipc->dev, "%s: start reset status:%d\n", __func__, atomic_read(&ipc->chub_status));
	if (!ipc->block_reset) {
		/* core reset */
		ipc_add_evt(IPC_EVT_A2C, IRQ_EVT_A2C_SHUTDOWN);
		msleep(100);	/* wait for shut down time */
	}

	mutex_lock(&pmu_shutdown_mutex);
	atomic_set(&ipc->chub_shutdown, 0);
	dev_info(ipc->dev, "%s: enter shutdown\n", __func__);
	ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_SHUTDOWN);
	if (ret) {
		dev_err(ipc->dev, "%s: shutdown fails, ret:%d\n", __func__, ret);
		mutex_unlock(&pmu_shutdown_mutex);
		goto out;
	}
	dev_info(ipc->dev, "%s: out shutdown\n", __func__);
	mutex_unlock(&pmu_shutdown_mutex);

	if (ipc->block_reset || force_load) {
		ret = contexthub_download_image(ipc, IPC_REG_BL);
		if (!ret) {
			if (force_load) /* can use new binary */
				ret = contexthub_download_image(ipc, IPC_REG_OS);
			else /* use previous binary */
				ret = contexthub_download_and_check_image(ipc, IPC_REG_OS);

			if (ret) {
				dev_err(ipc->dev, "%s: download os fails\n", __func__);
				ipc->pmu_shub_status.reason = 5;
				goto out;
			}
		} else {
				dev_err(ipc->dev, "%s: download bl fails\n", __func__);
				ipc->pmu_shub_status.reason = 6;
				goto out;
		}
	}

	/* enable mailbox interrupt to get 'alive' event */
	enable_irq(ipc->irq_mailbox);
	irq_disabled = false;

	ret = contexthub_ipc_write_event(ipc, MAILBOX_EVT_RESET);
	if (ret) {
		dev_err(ipc->dev, "%s: reset fails, ret:%d\n", __func__, ret);
		ipc->pmu_shub_status.reason = 7;
	} else {
		dev_info(ipc->dev, "%s: chub reseted! (cnt:%d)\n",
			__func__, ipc->err_cnt[CHUB_ERR_RESET_CNT]);
		ipc->err_cnt[CHUB_ERR_RESET_CNT]++;
		atomic_set(&ipc->in_use_ipc, 0);
	}
out:
	if (ret) {
		atomic_set(&ipc->chub_status, CHUB_ST_NO_RESPONSE);
		contexthub_dump_pmu_sfr(ipc);
		ipc_dump_mailbox_sfr(&ipc->mailbox_sfr_dump);
		if (irq_disabled)
			enable_irq(ipc->irq_mailbox);
		dev_err(ipc->dev, "%s: chub reset fail! should retry to reset (ret:%d), irq_disabled:%d\n",
			__func__, ret, irq_disabled);
		dump = 1;
		if (fail_cnt++ < RESET_RETRY_THD) {
			msleep(2000);
			dev_err(ipc->dev, "%s: chub reset fail! retry:%d\n", __func__, fail_cnt);
			goto reset_fail_retry;
		}
		dev_err(ipc->dev, "%s: chub reset failed finally\n", __func__);

	}

	__pm_relax(&ipc->ws_reset);
	atomic_dec(&ipc->in_reset);
	mutex_unlock(&reset_mutex);
#ifdef CONFIG_SENSORS_SSP
	if (!ret) {
		ssp_platform_start_refrsh_task(ipc->ssp_data);
	}
#endif
	return ret;
}

int contexthub_download_image(struct contexthub_ipc_info *ipc, enum ipc_region reg)
{
	const struct firmware *entry;
	int ret;

	dev_info(ipc->dev, "%s: enter for bl:%d\n", __func__, reg == IPC_REG_BL);
	if (reg == IPC_REG_BL)
#ifdef CONFIG_SENSORS_SSP
		ret = request_firmware(&entry, SSP_BOOTLOADER_FILE, ipc->dev);
#else
		ret = request_firmware(&entry, "bl.unchecked.bin", ipc->dev);
#endif
	else if (reg == IPC_REG_OS)
	{
#ifdef CONFIG_SENSORS_SSP
		ret = ssp_download_firmware(ipc->ssp_data, ipc->dev, ipc_get_base(reg));
		return ret;
#else
		dev_info(ipc->dev, "%s: download %s\n", __func__, ipc->os_name);
		ret = request_firmware(&entry, ipc->os_name, ipc->dev);
#endif
	}
	else
		ret = -EINVAL;

	if (ret) {
		dev_err(ipc->dev, "%s, bl(%d) request_firmware failed\n",
			__func__, reg == IPC_REG_BL);
		return ret;
	}
	memcpy(ipc_get_base(reg), entry->data, entry->size);
	dev_info(ipc->dev, "%s: bl:%d, bin(size:%d) on %lx\n",
		 __func__, reg == IPC_REG_BL, (int)entry->size, (unsigned long)ipc_get_base(reg));
	release_firmware(entry);

	return 0;
}

static void handle_irq(struct contexthub_ipc_info *ipc, enum irq_evt_chub evt)
{
	switch (evt) {
	case IRQ_EVT_C2A_DEBUG:
		contexthub_handle_debug(ipc, CHUB_ERR_NANOHUB, 1);
		break;
	case IRQ_EVT_C2A_INT:
		if (atomic_read(&ipc->irq1_apInt) == C2A_OFF) {
			atomic_set(&ipc->irq1_apInt, C2A_ON);
			contexthub_notify_host(ipc);
		}
		break;
	case IRQ_EVT_C2A_INTCLR:
		atomic_set(&ipc->irq1_apInt, C2A_OFF);
		break;
	default:
		if (evt < IRQ_EVT_CH_MAX) {
#ifdef CONFIG_SENSORS_SSP
			int size = 0;
			char rx_buf[PACKET_SIZE_MAX] = {0,};
			void *raw_rx_buf = 0;
 			raw_rx_buf = ipc_read_data(IPC_DATA_C2A, &size);
 			if (size > 0 && raw_rx_buf) {
				memcpy_fromio(rx_buf, (void *)raw_rx_buf, size);
				ssp_handle_recv_packet(ipc->ssp_data, rx_buf, size);
			}
			else {
				dev_err(ipc->dev, "%s: invalid comm (%d %x)\n", __func__, size, raw_rx_buf);
			}
#else // CONFIG_SENSORS_SSP
			int lock;
			ipc->read_lock.flag++;
			/* TODO: requered.. ? */
			spin_lock(&ipc->read_lock.event.lock);
			lock = read_is_locked(ipc);
			spin_unlock(&ipc->read_lock.event.lock);
			if (lock)
				wake_up_interruptible_sync(&ipc->read_lock.event);
#endif
		} else {
			dev_warn(ipc->dev, "%s: invalid %d event",
				 __func__, evt);
		}
		break;
	};
}

static irqreturn_t contexthub_irq_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;
	int start_index = ipc_hw_read_int_start_index(AP);
	unsigned int status = ipc_hw_read_int_status_reg(AP);
	struct ipc_evt_buf *cur_evt;
	enum chub_err_type err = 0;
	enum irq_chub evt = 0;
	int irq_num = IRQ_EVT_CHUB_ALIVE + start_index;

	if (atomic_read(&ipc->chub_status) != CHUB_ST_POWER_ON &&
		atomic_read(&ipc->chub_status) != CHUB_ST_RUN &&
		atomic_read(&ipc->chub_status) != CHUB_ST_SHUTDOWN) {
		pr_err("%s: illegal interrupt from mailbox!! %d", __func__, atomic_read(&ipc->chub_status));
		ipc_hw_mask_all(AP, 1);
		contexthub_dump_pmu_sfr(data);
		ipc_dump_mailbox_sfr(&ipc->mailbox_sfr_dump);
		return IRQ_HANDLED;
	}

	/* chub alive interrupt handle */
	if (status & (1 << irq_num)) {
		status &= ~(1 << irq_num);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		/* set wakeup flag for chub_alive_lock */
		ipc->chub_alive_lock.flag = 1;
		wake_up(&ipc->chub_alive_lock.event);
	}

	if (contexthub_get_token(ipc)) {
	    return IRQ_HANDLED;
	}

	/* chub ipc interrupt handle */
	while (status) {
		cur_evt = ipc_get_evt(IPC_EVT_C2A);

		if (cur_evt) {
			evt = cur_evt->evt;
			irq_num = cur_evt->irq + start_index;

			/* check match evtq and hw interrupt pending */
			if (!(status & (1 << irq_num))) {
				err = CHUB_ERR_EVTQ_NO_HW_TRIGGER;
				break;
			}
		} else {
			err = CHUB_ERR_EVTQ_EMTPY;
			break;
		}

		handle_irq(ipc, (u32)evt);
		ipc_hw_clear_int_pend_reg(AP, irq_num);
		status &= ~(1 << irq_num);
	}

	contexthub_put_token(ipc);

	if (err) {
		pr_err("inval irq err(%d):start_irqnum:%d,evt(%p):%d,irq_hw:%d,status_reg:0x%x(0x%x,0x%x)\n",
		       err, start_index, cur_evt, evt, irq_num,
		       status, ipc_hw_read_int_status_reg(AP),
		       ipc_hw_read_int_gen_reg(AP));
		ipc_hw_clear_all_int_pend_reg(AP);
		contexthub_handle_debug(ipc, err, 1);
	} else {
		clear_err_cnt(ipc, CHUB_ERR_EVTQ_EMTPY);
		clear_err_cnt(ipc, CHUB_ERR_EVTQ_NO_HW_TRIGGER);
	}
	return IRQ_HANDLED;
}

#if defined(CHUB_RESET_ENABLE)
static irqreturn_t contexthub_irq_wdt_handler(int irq, void *data)
{
	struct contexthub_ipc_info *ipc = data;

	dev_info(ipc->dev, "%s called\n", __func__);
	disable_irq_nosync(ipc->irq_wdt);
	ipc->irq_wdt_disabled = 1;
	contexthub_handle_debug(ipc, CHUB_ERR_FW_WDT, 1);

	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_SENSORS_SSP)
static int contexthub_cmgp_gpio_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
 	int sensor_ldo_en, sensor3p3_ldo_en;
	int ret;
	enum of_gpio_flags flags;

	/* sensor_ldo_en */
	sensor_ldo_en = of_get_named_gpio_flags(node, "sensor-ldo-en", 0, &flags);
	dev_info(dev, "[nanohub] sensor ldo en = %d", sensor_ldo_en);

	if(sensor_ldo_en >= 0) {
		ret = gpio_request(sensor_ldo_en, "sensor_ldo_en");
		if (ret) {
			dev_err(dev, "[nanohub] failed to request sensor_ldo_en, ret:%d\n", ret);
			return ret;
		}

		ret = gpio_direction_output(sensor_ldo_en, 1);
		if (ret) {
			dev_err(dev, "[nanohub] failed set sensor_ldo_en as output mode, ret:%d", ret);
			return ret;
		}

		gpio_set_value_cansleep(sensor_ldo_en, 1);
	}

	/* sensor3p3_ldo_en */
	sensor3p3_ldo_en = of_get_named_gpio_flags(node, "sensor3p3-ldo-en", 0, &flags);
	dev_info(dev, "[nanohub] sensor 3p3 ldo en = %d", sensor3p3_ldo_en);

	if(sensor3p3_ldo_en >= 0) {
		ret = gpio_request(sensor3p3_ldo_en, "sensor3p3 _ldo_en");
		if (ret) {
			dev_err(dev, "[nanohub] failed to request sensor_ldo_en, ret:%d\n", ret);
			return ret;
		}

		ret = gpio_direction_output(sensor3p3_ldo_en, 1);
		if (ret) {
			dev_err(dev, "[nanohub] failed set sensor3p3_ldo_en as output mode, ret:%d", ret);
			return ret;
		}

		gpio_set_value_cansleep(sensor3p3_ldo_en, 1);
	}

	return 0;
}
#endif

static struct clk *devm_clk_get_and_prepare(struct device *dev,
	const char *name)
{
	struct clk *clk = NULL;
	int ret;

	clk = devm_clk_get(dev, name);
	if (IS_ERR(clk)) {
		dev_err(dev, "Failed to get clock %s\n", name);
		goto error;
	}

	ret = clk_prepare(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare clock %s\n", name);
		goto error;
	}

	ret = clk_enable(clk);
	if (ret < 0) {
		dev_err(dev, "Failed to enable clock %s\n", name);
		goto error;
	}

error:
	return clk;
}

#if defined(CONFIG_SOC_EXYNOS9610)
extern int cal_dll_apm_enable(void);
#endif

static __init int contexthub_ipc_hw_init(struct platform_device *pdev,
					 struct contexthub_ipc_info *chub)
{
	int ret;
	int irq;
	struct resource *res;
	const char *os;
	const char *resetmode;
	struct device *dev = &pdev->dev;
	struct device_node *node = dev->of_node;
	const char *string_array[10];
	int chub_clk_len;
	struct clk *clk;
	int i;

	if (!node) {
		dev_err(dev, "driver doesn't support non-dt\n");
		return -ENODEV;
	}

	/* get os type from dt */
	os = of_get_property(node, "os-type", NULL);
	if (!os || !strcmp(os, "none") || !strcmp(os, "pass")) {
		dev_err(dev, "no use contexthub\n");
		chub->os_load = 0;
		return -ENODEV;
	} else {
		chub->os_load = 1;
		strcpy(chub->os_name, os);
	}

	/* get resetmode from dt */
	resetmode = of_get_property(node, "reset-mode", NULL);
	if (!resetmode || !strcmp(resetmode, "block"))
		chub->block_reset = 1;
	else
		chub->block_reset = 0;

	/* get mailbox interrupt */
	chub->irq_mailbox = irq_of_parse_and_map(node, 0);
	if (chub->irq_mailbox < 0) {
		dev_err(dev, "failed to get irq:%d\n", irq);
		return -EINVAL;
	}

	/* request irq handler */
#if defined(CONFIG_SENSORS_SSP)
	ret = devm_request_threaded_irq(dev, chub->irq_mailbox, NULL, contexthub_irq_handler,
			       IRQF_ONESHOT, dev_name(dev), chub);
#else
	ret = devm_request_irq(dev, chub->irq_mailbox, contexthub_irq_handler,
			       0, dev_name(dev), chub);
#endif

	if (ret) {
		dev_err(dev, "failed to request irq:%d, ret:%d\n",
			chub->irq_mailbox, ret);
		return ret;
	}

#if defined(CHUB_RESET_ENABLE)
	/* get wdt interrupt optionally */
	chub->irq_wdt = irq_of_parse_and_map(node, 1);
	if (chub->irq_wdt > 0) {
		/* request irq handler */
		ret = devm_request_irq(dev, chub->irq_wdt,
				       contexthub_irq_wdt_handler, 0,
				       dev_name(dev), chub);
		if (ret) {
			dev_err(dev, "failed to request wdt irq:%d, ret:%d\n",
				chub->irq_wdt, ret);
			return ret;
		}
		chub->irq_wdt_disabled = 0;
	} else {
		dev_info(dev, "don't use wdt irq:%d\n", irq);
	}
#endif

	/* get MAILBOX SFR */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mailbox");
	chub->mailbox = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->mailbox)) {
		dev_err(dev, "fails to get mailbox sfr\n");
		return PTR_ERR(chub->mailbox);
	}

	/* get SRAM base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sram");
	chub->sram = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->sram)) {
		dev_err(dev, "fails to get sram\n");
		return PTR_ERR(chub->sram);
	}

	/* get chub gpr base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "dumpgpr");
	chub->chub_dumpgpr = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->chub_dumpgpr)) {
		dev_err(dev, "fails to get dumpgpr\n");
		return PTR_ERR(chub->chub_dumpgpr);
	}

	/* get pmu reset base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "chub_reset");
	chub->pmu_chub_reset = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->pmu_chub_reset)) {
		dev_err(dev, "fails to get dumpgpr\n");
		return PTR_ERR(chub->pmu_chub_reset);
	}

	/* get chub baaw base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "chub_baaw");
	chub->chub_baaw = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->chub_baaw)) {
		pr_err("driver failed to get chub_baaw\n");
		chub->chub_baaw = 0;	/* it can be set on other-side (vts) */
	}

#if defined(CONFIG_SOC_EXYNOS9610)
	/* get cmu qch base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cmu_chub_qch");
	chub->cmu_chub_qch = devm_ioremap_resource(dev, res);
	if (IS_ERR(chub->cmu_chub_qch)) {
		pr_err("driver failed to get cmu_chub_qch\n");
		return PTR_ERR(chub->cmu_chub_qch);
	}
#endif

	/* get addresses information to set BAAW */
	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 0,
		 &chub->baaw_info.baaw_p_apm_chub_start)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, start\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 1,
		 &chub->baaw_info.baaw_p_apm_chub_end)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, end\n");
		return -ENODEV;
	}

	if (of_property_read_u32_index
		(node, "baaw,baaw-p-apm-chub", 2,
		 &chub->baaw_info.baaw_p_apm_chub_remap)) {
		dev_err(&pdev->dev,
			"driver failed to get baaw-p-apm-chub, remap\n");
		return -ENODEV;
	}

	/* disable chub irq list (for sensor irq) */
	of_property_read_u32(node, "chub-irq-pin-len", &chub->irq_pin_len);
	if (chub->irq_pin_len) {
		if (chub->irq_pin_len > sizeof(chub->irq_pins)) {
			dev_err(&pdev->dev,
			"failed to get irq pin length %d, %d\n",
			chub->irq_pin_len, sizeof(chub->irq_pins));
			chub->irq_pin_len = 0;
			return -ENODEV;
		}

		dev_info(&pdev->dev, "get chub irq_pin len:%d\n", chub->irq_pin_len);
		for (i = 0; i < chub->irq_pin_len; i++) {
			chub->irq_pins[i] = of_get_named_gpio(node, "chub-irq-pin", i);
			if (!gpio_is_valid(chub->irq_pins[i])) {
				dev_err(&pdev->dev, "get invalid chub irq_pin:%d\n", chub->irq_pins[i]);
				return -EINVAL;
			}
			dev_info(&pdev->dev, "get chub irq_pin:%d\n", chub->irq_pins[i]);
		}
	}
#if defined(CONFIG_SOC_EXYNOS9610)
	cal_dll_apm_enable();
#endif

	clk = devm_clk_get_and_prepare(dev, "chub_bus");
	if (!clk)
		return -ENODEV;
	chub->clkrate = clk_get_rate(clk);

	chub_clk_len = of_property_count_strings(node, "clock-names");
	of_property_read_string_array(node, "clock-names", string_array, chub_clk_len);
	for (i = 0; i < chub_clk_len; i++) {
		clk = devm_clk_get_and_prepare(dev, string_array[i]);
		if (!clk)
			return -ENODEV;
		dev_info(&pdev->dev, "clk_name: %s enable\n", __clk_get_name(clk));
	}
#if defined(CONFIG_SENSORS_SSP)
	ret = contexthub_cmgp_gpio_init(&pdev->dev);
	if(ret) {
		dev_err(&pdev->dev, "[nanohub] contexthub_cmgp_gpio_init failed\n");
		return ret;
	}
#endif

	return 0;
}

static ssize_t chub_poweron(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
	int ret = contexthub_poweron(ipc);

#ifdef CONFIG_SENSORS_SSP
	if (ret < 0) {
		dev_err(dev, "poweron failed %d\n", ret);
	} else {
		ssp_platform_start_refrsh_task(ipc->ssp_data);
	}
#endif
	return ret < 0 ? ret : count;
}

static ssize_t chub_reset(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
	int ret = contexthub_reset(ipc, 1, 1);

	return ret < 0 ? ret : count;
}

static struct device_attribute attributes[] = {
	__ATTR(poweron, 0220, NULL, chub_poweron),
	__ATTR(reset, 0220, NULL, chub_reset),
};

#ifdef CONFIG_EXYNOS_ITMON
static int chub_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct contexthub_ipc_info *data = container_of(nb, struct contexthub_ipc_info, itmon_nb);
	struct itmon_notifier *itmon_data = nb_data;

	if (itmon_data && itmon_data->master &&
		((!strncmp("CM4_SHUB_CD", itmon_data->master, sizeof("CM4_SHUB_CD") - 1)) ||
		(!strncmp("CM4_SHUB_P", itmon_data->master, sizeof("CM4_SHUB_P") - 1)) ||
		(!strncmp("PDMA_SHUB", itmon_data->master, sizeof("PDMA_SHUB") - 1)))) {
		dev_info(data->dev, "%s: chub(%s) itmon detected: action:%d!!\n",
			__func__, itmon_data->master, action);
		contexthub_handle_debug(data, CHUB_ERR_ITMON, 1);
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

static int contexthub_ipc_probe(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub;
	int need_to_free = 0;
	int ret = 0;
	int i;
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct iio_dev *iio_dev;
#endif
	chub = chub_dbg_get_memory(DBG_NANOHUB_DD_AREA);
	if (!chub) {
		chub =
		    devm_kzalloc(&pdev->dev, sizeof(struct contexthub_ipc_info),
				 GFP_KERNEL);
		need_to_free = 1;
	}
	if (IS_ERR(chub)) {
		dev_err(&pdev->dev, "%s failed to get ipc memory\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	/* parse dt and hw init */
	ret = contexthub_ipc_hw_init(pdev, chub);
	if (ret) {
		dev_err(&pdev->dev, "%s failed to get init hw with ret %d\n",
			__func__, ret);
		goto err;
	}

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	/* nanohub probe */
	iio_dev = nanohub_probe(&pdev->dev, NULL);
	if (IS_ERR(iio_dev))
		goto err;

	/* set wakeup irq number on nanohub driver */
	chub->data = iio_priv(iio_dev);
	nanohub_mailbox_comms_init(&chub->data->comms);
	chub->pdata = chub->data->pdata;
	chub->pdata->mailbox_client = chub;
	chub->data->irq1 = IRQ_EVT_A2C_WAKEUP;
	chub->data->irq2 = 0;
#elif defined(CONFIG_SENSORS_SSP)
	chub->ssp_data = ssp_device_probe(&pdev->dev);
	if(IS_ERR(chub->ssp_data)) {
		dev_err(chub->dev, "[nanohub] ssp_probe failed \n");
		return PTR_ERR(chub->ssp_data);
	}

	ssp_platform_init(chub->ssp_data, chub);
	ssp_set_firmware_name(chub->ssp_data, chub->os_name);
#endif
	atomic_set(&chub->in_use_ipc, 0);
	atomic_set(&chub->chub_status, CHUB_ST_NO_POWER);
	atomic_set(&chub->in_reset, 0);
	chub->powermode = 0; /* updated by fw bl */
	chub->cur_err = 0;
	for (i = 0; i < CHUB_ERR_MAX; i++)
		chub->err_cnt[i] = 0;
	chub->dev = &pdev->dev;
	platform_set_drvdata(pdev, chub);
	contexthub_config_init(chub);

	for (i = 0, ret = 0; i < ARRAY_SIZE(attributes); i++) {
		ret = device_create_file(chub->dev, &attributes[i]);
		if (ret)
			dev_warn(chub->dev, "Failed to create file: %s\n",
				 attributes[i].attr.name);
	}

	init_waitqueue_head(&chub->read_lock.event);
	init_waitqueue_head(&chub->chub_alive_lock.event);
	INIT_WORK(&chub->debug_work, handle_debug_work_func);
#ifdef CONFIG_EXYNOS_ITMON
	chub->itmon_nb.notifier_call = chub_itmon_notifier;
	itmon_notifier_chain_register(&chub->itmon_nb);
#endif

	wakeup_source_init(&chub->ws_reset, "chub_reboot");
	dev_info(chub->dev, "%s with %s FW and %lu clk is done\n",
					__func__, chub->os_name, chub->clkrate);
	return 0;
err:
	if (chub)
		if (need_to_free)
			devm_kfree(&pdev->dev, chub);

	dev_err(&pdev->dev, "%s is fail with ret %d\n", __func__, ret);
	return ret;
}

static int contexthub_ipc_remove(struct platform_device *pdev)
{
	struct contexthub_ipc_info *chub = platform_get_drvdata(pdev);

	wakeup_source_trash(&chub->ws_reset);

#ifdef CONFIG_SENSORS_SSP
	ssp_device_remove(chub->ssp_data);
#endif
	return 0;
}

static int contexthub_alive_noirq(struct contexthub_ipc_info *ipc, int ap_state)
{
    int cnt = 100;
    int start_index = ipc_hw_read_int_start_index(AP);
    unsigned int status;
    int irq_num = IRQ_EVT_CHUB_ALIVE + start_index;
    pr_info("%s start\n", __func__);
    ipc_hw_write_shared_reg(AP, ap_state, SR_3);
    ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);

    ipc->chub_alive_lock.flag = 0;
    while(cnt--) {
        mdelay(1);
        status = ipc_hw_read_int_status_reg(AP);
        if (status & (1 << irq_num)) {
            ipc_hw_clear_int_pend_reg(AP, irq_num);
            ipc->chub_alive_lock.flag = 1;
            pr_info("%s end\n", __func__);
            return 0;
        }
    }
    pr_err("%s pm alive fail!!\n", __func__);
    return -1;
}

static int contexthub_suspend_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct nanohub_data *data = ipc->data;
#endif

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	pr_info("%s\n", __func__);
	ipc_hw_write_shared_reg(AP, MAILBOX_REQUEST_KLOG_OFF, SR_3);
	ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);

#ifdef CONFIG_CHRE_SENSORHUB_HAL
	return nanohub_suspend(data->iio_dev);
#else
	return 0;
#endif
}

static int contexthub_resume_noirq(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);
#ifdef CONFIG_CHRE_SENSORHUB_HAL
	struct nanohub_data *data = ipc->data;
#endif

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

	pr_info("%s\n", __func__);
	contexthub_alive_noirq(ipc, MAILBOX_REQUEST_KLOG_ON);

#if defined(CONFIG_CHRE_SENSORHUB_HAL)
	return nanohub_resume(data->iio_dev);
#endif
	return 0;
}

static int contexthub_prepare(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return 0;

    pr_info("%s\n", __func__);
    ipc_hw_write_shared_reg(AP, MAILBOX_REQUEST_AP_PREPARE, SR_3);
    ipc_hw_gen_interrupt(AP, IRQ_EVT_CHUB_ALIVE);
#ifdef CONFIG_SENSORS_SSP
	ssp_device_suspend(ipc->ssp_data);
#endif

	return 0;
}

static void contexthub_complete(struct device *dev)
{
	struct contexthub_ipc_info *ipc = dev_get_drvdata(dev);

	if (atomic_read(&ipc->chub_status) != CHUB_ST_RUN)
		return;

	pr_info("%s irq disabled\n", __func__);
	disable_irq(ipc->irq_mailbox);
    contexthub_alive_noirq(ipc, MAILBOX_REQUEST_AP_COMPLETE);
	enable_irq(ipc->irq_mailbox);
#ifdef CONFIG_SENSORS_SSP
	ssp_device_resume(ipc->ssp_data);
#endif

	return;
}

//static SIMPLE_DEV_PM_OPS(contexthub_pm_ops, contexthub_suspend, contexthub_resume);
static const struct dev_pm_ops contexthub_pm = {
		.prepare = contexthub_prepare,
		.complete = contexthub_complete,
		.suspend_noirq = contexthub_suspend_noirq,
		.resume_noirq = contexthub_resume_noirq,
};


static const struct of_device_id contexthub_ipc_match[] = {
	{.compatible = "samsung,exynos-nanohub"},
	{},
};

static struct platform_driver samsung_contexthub_ipc_driver = {
	.probe = contexthub_ipc_probe,
	.remove = contexthub_ipc_remove,
	.driver = {
		   .name = "nanohub-ipc",
		   .owner = THIS_MODULE,
		   .of_match_table = contexthub_ipc_match,
		   .pm = &contexthub_pm,
	},
};

int nanohub_mailbox_init(void)
{
	return platform_driver_register(&samsung_contexthub_ipc_driver);
}

static void __exit nanohub_mailbox_cleanup(void)
{
	platform_driver_unregister(&samsung_contexthub_ipc_driver);
}

module_init(nanohub_mailbox_init);
module_exit(nanohub_mailbox_cleanup);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Exynos contexthub mailbox Driver");
MODULE_AUTHOR("Boojin Kim <boojin.kim@samsung.com>");
