/* drivers/input/touchscreen/sec_ts.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/time.h>
#include <linux/completion.h>
#include <linux/wakelock.h>
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
#include <linux/input/tui_hal_ts.h>
#include <linux/t-base-tui.h>
#endif
#ifdef CONFIG_DRV_SAMSUNG
#include <linux/sec_class.h>
#endif
#include "../../../i2c/busses/i2c-exynos5.h"

struct sec_ts_data *tsp_info;

#include "sec_ts.h"

#ifdef CONFIG_SECURE_TOUCH
#include <soc/qcom/scm.h>
enum subsystem {
	TZ = 1,
	APSS = 3
};

#define TZ_BLSP_MODIFY_OWNERSHIP_ID 3
#endif

struct sec_ts_data *ts_dup;

#ifdef USE_RESET_DURING_POWER_ON
static void sec_ts_reset_work(struct work_struct *work);
#endif
static void sec_ts_read_nv_work(struct work_struct *work);

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev);
static void sec_ts_input_close(struct input_dev *dev);
#endif

static int sec_ts_stop_device(struct sec_ts_data *ts);
static int sec_ts_start_device(struct sec_ts_data *ts);

static int sec_ts_read_information(struct sec_ts_data *ts);
static int sec_ts_set_lowpowermode(struct sec_ts_data *ts, u8 mode);

u8 lv1cmd;
u8 *read_lv1_buff;
static int lv1_readsize;
static int lv1_readremain;
static int lv1_readoffset;
static u32 use_ic_info = 1;

static ssize_t sec_ts_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_regreadsize_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static inline ssize_t sec_ts_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sec_ts_enter_recovery_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_regread_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static ssize_t sec_ts_gesture_status_show(struct device *dev,
		struct device_attribute *attr, char *buf);
static inline ssize_t sec_ts_show_error(struct device *dev,
		struct device_attribute *attr, char *buf);

static DEVICE_ATTR(sec_ts_reg, (S_IWUSR | S_IWGRP), NULL, sec_ts_reg_store);
static DEVICE_ATTR(sec_ts_regreadsize, (S_IWUSR | S_IWGRP), NULL, sec_ts_regreadsize_store);
static DEVICE_ATTR(sec_ts_enter_recovery, (S_IWUSR | S_IWGRP), NULL, sec_ts_enter_recovery_store);
static DEVICE_ATTR(sec_ts_regread, S_IRUGO, sec_ts_regread_show, NULL);
static DEVICE_ATTR(sec_ts_gesture_status, S_IRUGO, sec_ts_gesture_status_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_sec_ts_reg.attr,
	&dev_attr_sec_ts_regreadsize.attr,
	&dev_attr_sec_ts_enter_recovery.attr,
	&dev_attr_sec_ts_regread.attr,
	&dev_attr_sec_ts_gesture_status.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static inline ssize_t sec_ts_show_error(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "sec_ts :%s read only function, %s\n", __func__, attr->attr.name);
	return -EPERM;
}

static inline ssize_t sec_ts_store_error(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	input_err(true, &ts->client->dev, "sec_ts :%s write only function, %s\n", __func__, attr->attr.name);
	return -EPERM;
}

#ifdef CONFIG_SECURE_TOUCH
static int sec_ts_change_pipe_owner(struct sec_ts_data *ts, enum subsystem subsystem)
{
	/* scm call disciptor */
	struct scm_desc desc;
	int ret = 0;

	/* number of arguments */
	desc.arginfo = SCM_ARGS(2);
	/* BLSPID (1 - 12) */
	desc.args[0] = ts->client->adapter->nr - 1;
	/* Owner if TZ or APSS */
	desc.args[1] = subsystem;

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_TZ, TZ_BLSP_MODIFY_OWNERSHIP_ID), &desc);
	input_err(true, &ts->client->dev, "%s: return1: %d\n", __func__, ret);
	if (ret)
		return ret;

	input_err(true, &ts->client->dev, "%s: return2: %llu\n", __func__, desc.ret[0]);

	return desc.ret[0];
}

static irqreturn_t sec_ts_irq_thread(int irq, void *ptr);

/**
 * Sysfs attr group for secure touch & interrupt handler for Secure world.
 * @atomic : syncronization for secure_enabled
 * @pm_runtime : set rpm_resume or rpm_ilde
 */

static void secure_touch_notify(struct sec_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);
	sysfs_notify(&ts->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t secure_filter_interrupt(struct sec_ts_data *ts)
{
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		if (atomic_cmpxchg(&ts->secure_pending_irqs, 0, 1) == 0) {
			input_info(true, &ts->client->dev, "%s: pending irq:%d\n",
						__func__, (int)atomic_read(&ts->secure_pending_irqs));
			secure_touch_notify(ts);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
			complete(&ts->st_irq_received);
#endif
		} else {
			input_info(true, &ts->client->dev, "%s: --\n", __func__);
		}

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int secure_touch_clk_prepare_enable(struct sec_ts_data *ts)
{
	int ret;

	if (!ts->core_clk || !ts->iface_clk) {
		input_err(true, &ts->client->dev, "%s: error clk\n", __func__);
		return -ENODEV;
	}

	ret = clk_prepare_enable(ts->core_clk);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed core clk\n", __func__);
		goto err_core_clk;
	}

	ret = clk_prepare_enable(ts->iface_clk);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed iface clk\n", __func__);
		goto err_iface_clk;
	}

	return 0;

err_iface_clk:
	clk_disable_unprepare(ts->core_clk);
err_core_clk:
	return -ENODEV;
}

static void secure_touch_clk_unprepare_disable(struct sec_ts_data *ts)
{
	if (!ts->core_clk || !ts->iface_clk) {
		input_err(true, &ts->client->dev, "%s: error clk\n", __func__);
		return;
	}

	clk_disable_unprepare(ts->core_clk);
	clk_disable_unprepare(ts->iface_clk);
}

static ssize_t secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d", atomic_read(&ts->secure_enabled));
}

static ssize_t secure_touch_enable_store(struct device *dev,
			struct device_attribute *addr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int data, ret;

	ret = sscanf(buf, "%d", &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read:%d\n",
					__func__, ret);
		return -EINVAL;
	}

	if (data == 1) {
		/* Enable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
			input_err(true, &ts->client->dev, "%s: already enabled\n", __func__);
			return -EBUSY;
		}

		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(ts->client->irq);

		/* Fix normal active mode : idle mode is failed to i2c for 1 time */
		ret = sec_ts_fix_tmode(ts, TOUCH_SYSTEM_MODE_TOUCH, TOUCH_MODE_STATE_TOUCH);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to fix tmode\n",
					__func__);
			return -EIO;
		}

		/* Release All Finger */
		sec_ts_unlocked_release_all_finger(ts);

		if (pm_runtime_get_sync(ts->client->adapter->dev.parent) < 0) {
			input_err(true, &ts->client->dev, "%s: failed to get pm_runtime\n", __func__);
			return -EIO;
		}

		if (secure_touch_clk_prepare_enable(ts) < 0) {
			pm_runtime_put_sync(ts->client->adapter->dev.parent);
			input_err(true, &ts->client->dev, "%s: failed to clk enable\n", __func__);
			return -ENXIO;
		}

		sec_ts_change_pipe_owner(ts, TZ);

		reinit_completion(&ts->secure_powerdown);
		reinit_completion(&ts->secure_interrupt);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
		reinit_completion(&ts->st_irq_received);
#endif
		atomic_set(&ts->secure_enabled, 1);
		atomic_set(&ts->secure_pending_irqs, 0);

		enable_irq(ts->client->irq);

		input_info(true, &ts->client->dev, "%s: secure touch enable\n", __func__);
	} else if (data == 0) {
		/* Disable Secure World */
		if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
			input_err(true, &ts->client->dev, "%s: already disabled\n", __func__);
			return count;
		}

		sec_ts_change_pipe_owner(ts, APSS);

		secure_touch_clk_unprepare_disable(ts);
		pm_runtime_put_sync(ts->client->adapter->dev.parent);
		atomic_set(&ts->secure_enabled, 0);
		secure_touch_notify(ts);

		sec_ts_delay(10);

		sec_ts_irq_thread(ts->client->irq, ts);
		complete(&ts->secure_interrupt);
		complete(&ts->secure_powerdown);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
		complete(&ts->st_irq_received);
#endif

		input_info(true, &ts->client->dev, "%s: secure touch disable\n", __func__);

		ret = sec_ts_release_tmode(ts);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: failed to release tmode\n",
					__func__);
			return -EIO;
		}

	} else {
		input_err(true, &ts->client->dev, "%s: unsupport value:%d\n", __func__, data);
		return -EINVAL;
	}

	return count;
}

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
static int secure_get_irq(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));

	complete(&ts->secure_interrupt);
	
	return val;
}
#endif

static ssize_t secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_DISABLE) {
		input_err(true, &ts->client->dev, "%s: disabled\n", __func__);
		return -EBADF;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, -1, 0) == -1) {
		input_err(true, &ts->client->dev, "%s: pending irq -1\n", __func__);
		return -EINVAL;
	}

	if (atomic_cmpxchg(&ts->secure_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &ts->client->dev, "%s: pending irq is %d\n",
				__func__, atomic_read(&ts->secure_pending_irqs));

	complete(&ts->secure_interrupt);

	return snprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t secure_ownership_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1");
}

static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
				secure_touch_enable_show, secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, secure_touch_show, NULL);

static DEVICE_ATTR(secure_ownership, S_IRUGO, secure_ownership_show, NULL);

static struct attribute *secure_attr[] = {
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_ownership.attr,
	NULL,
};

static struct attribute_group secure_attr_group = {
	.attrs = secure_attr,
};


static int secure_touch_init(struct sec_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	init_completion(&ts->secure_interrupt);
	init_completion(&ts->secure_powerdown);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
	init_completion(&ts->st_irq_received);
#endif

	ts->core_clk = clk_get(&ts->client->adapter->dev, "core_clk");
	if (IS_ERR_OR_NULL(ts->core_clk)) {
		input_err(true, &ts->client->dev, "%s: failed to get core_clk: %ld\n",
					__func__, PTR_ERR(ts->core_clk));
		goto err_core_clk;
	}

	ts->iface_clk = clk_get(&ts->client->adapter->dev, "iface_clk");
	if (IS_ERR_OR_NULL(ts->iface_clk)) {
		input_err(true, &ts->client->dev, "%s: failed to get iface_clk: %ld\n",
					__func__, PTR_ERR(ts->iface_clk));
		goto err_iface_clk;
	}

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
	register_tui_hal_ts(&ts->input_dev->dev, \
						&ts->secure_enabled, \
						&ts->st_irq_received, \
						secure_get_irq, \
						secure_touch_enable_store);
#endif

	return 0;

err_iface_clk:
	clk_put(ts->core_clk);
err_core_clk:
	ts->core_clk = NULL;
	ts->iface_clk = NULL;

	return ENODEV;
}

static void secure_touch_stop(struct sec_ts_data *ts, bool stop)
{
	if (atomic_read(&ts->secure_enabled)) {
		atomic_set(&ts->secure_pending_irqs, -1);
		secure_touch_notify(ts);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
		complete(&ts->st_irq_received);
#endif

		if (stop)
			wait_for_completion_interruptible(&ts->secure_powerdown);

		input_info(true, &ts->client->dev, "%s: %d\n", __func__, stop);
	}
}
#endif

int sec_ts_i2c_write(struct sec_ts_data *ts, u8 reg, u8 *data, int len)
{
	u8 buf[I2C_WRITE_BUFFER_SIZE + 1];
	int ret;
	unsigned char retry;
	struct i2c_msg msg;

#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		dev_err(&ts->client->dev,
			"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &ts->client->dev,
			"%s: TSP no accessible from Linux, TRUSTED_UI is enabled!\n", __func__);
		return -EIO;
		}
#endif

	if (len > I2C_WRITE_BUFFER_SIZE) {
		input_err(true, &ts->client->dev, "sec_ts_i2c_write len is larger than buffer size\n");
		return -1;
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	buf[0] = reg;
	memcpy(buf + 1, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = buf;
	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1)
			break;

		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}

		input_err(true, &ts->client->dev, "%s: I2C retry %d\n", __func__, retry + 1);
		usleep_range(1 * 1000, 1 * 1000);
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
#ifdef USE_POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
#endif
	}

	if (ret == 1)
		return 0;
err:
	return -EIO;
}

int sec_ts_i2c_read(struct sec_ts_data *ts, u8 reg, u8 *data, int len)
{
	u8 buf[4];
	int ret;
	unsigned char retry;
	struct i2c_msg msg[2];
	int remain = len;

#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		dev_err(&ts->client->dev,
			"%s: TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EBUSY;
	}
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &ts->client->dev,
			"%s: TSP no accessible from Linux, TRUSTED_UI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF\n", __func__);
		goto err;
	}

	buf[0] = reg;

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	mutex_lock(&ts->i2c_mutex);

	if (len <= ts->i2c_burstmax) {

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 2);
			if (ret == 2)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}

	} else {
		/*
		 * I2C read buffer is 256 byte. do not support long buffer over than 256.
		 * So, try to seperate reading data about 256 bytes.
		 */

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
			if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
				input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
				mutex_unlock(&ts->i2c_mutex);
				goto err;
			}
		}

		do {
			if (remain > ts->i2c_burstmax)
				msg[1].len = ts->i2c_burstmax;
			else
				msg[1].len = remain;

			remain -= ts->i2c_burstmax;

			for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
				ret = i2c_transfer(ts->client->adapter, &msg[1], 1);
				if (ret == 1)
					break;
				usleep_range(1 * 1000, 1 * 1000);
				if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
					input_err(true, &ts->client->dev, "%s: POWER_STATUS : OFF, retry:%d\n", __func__, retry);
					mutex_unlock(&ts->i2c_mutex);
					goto err;
				}
			}

			msg[1].buf += msg[1].len;

		} while (remain > 0);

	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;
#ifdef USE_POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
#endif

	}

	return ret;

err:
	return -EIO;
}

static int sec_ts_i2c_write_burst(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	int retry;

#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		dev_err(&ts->client->dev,
			"%s: TSP no accessible from Linux, TUI is enabled\n", __func__);
		return -EBUSY;
	}
#endif

	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_master_send(ts->client, data, len)) == len)
			break;

		usleep_range(1 * 1000, 1 * 1000);
	}

	mutex_unlock(&ts->i2c_mutex);
	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
	}

	return ret;
}

static int sec_ts_i2c_read_bulk(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	unsigned char retry;
	int remain = len;
	struct i2c_msg msg;

#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&ts->secure_enabled) == SECURE_TOUCH_ENABLE) {
		dev_err(&ts->client->dev,
			"%s: TSP no accessible from Linux, TUI is enabled\n", __func__);
		return -EBUSY;
	}
#endif

	msg.addr = ts->client->addr;
	msg.flags = I2C_M_RD;
	msg.len = len;
	msg.buf = data;

	mutex_lock(&ts->i2c_mutex);

	do {
		if (remain > ts->i2c_burstmax)
			msg.len = ts->i2c_burstmax;
		else
			msg.len = remain;

		remain -= ts->i2c_burstmax;

		for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
			ret = i2c_transfer(ts->client->adapter, &msg, 1);
			if (ret == 1)
				break;
			usleep_range(1 * 1000, 1 * 1000);
		}

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
		ret = -EIO;

		break;
	}
		msg.buf += msg.len;

	} while (remain > 0);

	mutex_unlock(&ts->i2c_mutex);

	if (ret == 1)
		return 0;

	return -EIO;
}
static int sec_ts_read_from_sponge(struct sec_ts_data *ts, u8 *data)
{
	int ret;

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);

	ret = sec_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, sizeof(data));
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);

	return ret;
}

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include <linux/qcom/sec_debug.h>
extern struct tsp_dump_callbacks dump_callbacks;
static struct delayed_work * p_ghost_check;

extern void sec_ts_run_rawdata_all(struct sec_ts_data *ts);
static void sec_ts_check_rawdata(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data, ghost_check.work);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s, ignored ## already checking..\n", __func__);
		return;
	}
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s, ignored ## IC is power off\n", __func__);
		return;
	}

	ts->tsp_dump_lock = 1;
	input_err(true, &ts->client->dev, "%s, start ##\n", __func__);
	sec_ts_run_rawdata_all((void *)ts);
	msleep(100);

	input_err(true, &ts->client->dev, "%s, done ##\n", __func__);
	ts->tsp_dump_lock = 0;

}

static void dump_tsp_log(void)
{
	printk(KERN_ERR "%s sec_ts %s: start \n", SECLOG, __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		printk(KERN_ERR "%s sec_ts %s, ignored ## lpm charging Mode!!\n", SECLOG, __func__);
		return;
	}
#endif

	if (p_ghost_check == NULL) {
		printk(KERN_ERR "%s sec_ts %s, ignored ## tsp probe fail!!\n", SECLOG, __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#endif


void sec_ts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

int sec_ts_wait_for_ready(struct sec_ts_data *ts, unsigned int ack)
{
	int rc = -1;
	int retry = 0;
	u8 tBuff[SEC_TS_EVENT_BUFF_SIZE] = {0,};

	while (sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, tBuff, SEC_TS_EVENT_BUFF_SIZE)) {
		if (((tBuff[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_INFO) {
			if (tBuff[1] == ack) {
				rc = 0;
				break;
			}
		} else if (((tBuff[0] >> 2) & 0xF) == TYPE_STATUS_EVENT_VENDOR_INFO) {
			if (tBuff[1] == ack) {
				rc = 0;
				break;
			}
		}

		if (retry++ > SEC_TS_WAIT_RETRY_CNT) {
			input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
			break;
		}
		sec_ts_delay(20);
	}

	input_info(true, &ts->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X [%d]\n",
		__func__, tBuff[0], tBuff[1], tBuff[2], tBuff[3],
		tBuff[4], tBuff[5], tBuff[6], tBuff[7], retry);

	return rc;
}

int sec_ts_read_calibration_report(struct sec_ts_data *ts)
{
	int ret;
	u8 buf[5] = { 0 };

	buf[0] = SEC_TS_READ_CALIBRATION_REPORT;

	ret = sec_ts_i2c_read(ts, buf[0], &buf[1], 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: count:%d, pass count:%d, fail count:%d, status:0x%X\n",
				__func__, buf[1], buf[2], buf[3], buf[4]);

	return buf[4];
}

#define MAX_EVENT_COUNT 32
static void sec_ts_read_event(struct sec_ts_data *ts)
{
	int ret;
	int t_id;
	int event_id;
	int left_event_count;
	u8 read_event_buff[MAX_EVENT_COUNT][SEC_TS_EVENT_BUFF_SIZE] = {{0}};
	u8 *event_buff;
	struct sec_ts_event_coordinate *p_event_coord;
	struct sec_ts_coordinate *coordinate = NULL;
	struct sec_ts_gesture_status *p_gesture_status;
	struct sec_ts_event_status *p_event_status;
	int curr_pos;
	int remain_event_count = 0;
	struct sec_ts_plat_data *pdata = ts->plat_data;

	/* in LPM, waiting blsp block resume */
	if (ts->power_status == SEC_TS_STATE_LPM_SUSPEND) {

		wake_lock_timeout(&ts->wakelock, msecs_to_jiffies(3 * MSEC_PER_SEC));
		/* waiting for blsp block resuming, if not occurs i2c error */
		ret = wait_for_completion_interruptible_timeout(&ts->resume_done, msecs_to_jiffies(3 * MSEC_PER_SEC));
		if (ret == 0) {
			input_err(true, &ts->client->dev, "%s: LPM: pm resume is not handled\n", __func__);
			return;
		}

		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: LPM: -ERESTARTSYS if interrupted, %d\n", __func__, ret);
			return;
		}

		input_err(true, &ts->client->dev, "%s: run LPM interrupt handler, %d\n", __func__, ret);
		/* run lpm interrupt handler */
	}

	ret = t_id = event_id = curr_pos = remain_event_count = 0;
	/* repeat READ_ONE_EVENT until buffer is empty(No event) */
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, (u8*)read_event_buff[0], SEC_TS_EVENT_BUFF_SIZE);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
		return;
	}

	if (read_event_buff[0][0] == 0) {
		input_info(true, &ts->client->dev, "%s: event buffer is empty\n", __func__);
		return;	
	}

	left_event_count = read_event_buff[0][7] & 0x3F;
	remain_event_count = left_event_count;

	if (left_event_count > MAX_EVENT_COUNT - 1 || left_event_count == 0xFF) {
		input_err(true, &ts->client->dev, "%s: event buffer overflow\n", __func__);

		/* write clear event stack command when read_event_count > MAX_EVENT_COUNT */
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);
		return;
	}

	if (left_event_count > 0) {
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_ALL_EVENT, (u8*)read_event_buff[1],
				sizeof(u8) * (SEC_TS_EVENT_BUFF_SIZE) * (left_event_count));
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return;
		}
	}

	do {
		event_buff = read_event_buff[curr_pos];
		event_id = event_buff[0] & 0x3;

		switch (event_id) {
		case SEC_TS_STATUS_EVENT:
			p_event_status = (struct sec_ts_event_status *)event_buff;

			/* tchsta == 0 && ttype == 0 && eid == 0 : buffer empty */
			if (p_event_status->stype > 0)
				input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
						event_buff[0], event_buff[1], event_buff[2],
						event_buff[3], event_buff[4], event_buff[5],
						event_buff[6], event_buff[7]);

			/* watchdog reset -> send SENSEON command */ /*=>?????*/
			if ((p_event_status->stype == TYPE_STATUS_EVENT_INFO) &&
				(p_event_status->status_id == SEC_TS_ACK_BOOT_COMPLETE) &&
				(p_event_status->status_data_1 == 0x20)) {

				sec_ts_unlocked_release_all_finger(ts);

				ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);
			}

			/* event queue full-> all finger release */
			if ((p_event_status->stype == TYPE_STATUS_EVENT_ERR) &&
				(p_event_status->status_id == SEC_TS_ERR_EVENT_QUEUE_FULL)) {
				input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
				sec_ts_unlocked_release_all_finger(ts);
			}

			if ((p_event_status->stype == TYPE_STATUS_EVENT_ERR) &&
				(p_event_status->status_id == SEC_TS_ERR_EVENT_ESD)) {
				input_err(true, &ts->client->dev, "%s: ESD detected. run reset\n", __func__);
#ifdef USE_RESET_DURING_POWER_ON
				schedule_work(&ts->reset_work.work);
#endif
			}

			if ((p_event_status->stype == TYPE_STATUS_EVENT_INFO) &&
				(p_event_status->status_id == SEC_TS_ACK_WET_MODE)) {
					ts->wet_mode = p_event_status->status_data_1;
					input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
						__func__, ts->wet_mode);
			}

			if ((p_event_status->stype == TYPE_STATUS_EVENT_USER_INPUT) &&
				(p_event_status->status_id == SEC_TS_EVENT_FORCE_KEY)) {
				u8 sponge[3] = { 0 };
				u8 data[4] = {0x52, 0x00, 0x00, 0x00};

				ret = sec_ts_read_from_sponge(ts, sponge);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to read sponge data\n", __func__);

				input_info(true, &ts->client->dev, "%s: Sponge, %x, %x, %x\n",
											__func__, sponge[0], sponge[1], sponge[2]);

				ret = sec_ts_read_from_sponge(ts, data);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to read sponge data\n", __func__);

				ts->scrub_id = SPECIAL_EVENT_TYPE_AOD_HOMEKEY;
				ts->scrub_x = (data[1] & 0xFF) << 8 | (data[0] & 0xFF);
				ts->scrub_y = (data[3] & 0xFF) << 8 | (data[2] & 0xFF);

				if (sponge[1] == 0x40)
					input_report_key(ts->input_dev, KEY_HOMEPAGE, 1);
				else
					input_report_key(ts->input_dev, KEY_HOMEPAGE, 0);

				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);

				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_HOMEPAGE, 0);
				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);

				input_info(true, &ts->client->dev,
					"%s: [HOME key] %s\n", __func__, sponge[1] == 0x40 ? "press" : "release");
			}

			break;

		case SEC_TS_COORDINATE_EVENT:
			if (ts->input_closed) {
				input_info(true, &ts->client->dev, "%s: device is closed\n", __func__);
				remain_event_count = 0;
				break;
			}
			p_event_coord = (struct sec_ts_event_coordinate *)event_buff;

			t_id = (p_event_coord->tid - 1);

			if (t_id < MAX_SUPPORT_TOUCH_COUNT + MAX_SUPPORT_HOVER_COUNT) {
				coordinate = &ts->coord[t_id];
				coordinate->id = t_id;
				coordinate->action = p_event_coord->tchsta;
				coordinate->x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
				coordinate->y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
				coordinate->z = p_event_coord->z & 0x3F;
				coordinate->ttype = p_event_coord->ttype_3_2 << 2 | p_event_coord->ttype_1_0 << 0;
				coordinate->major = p_event_coord->major;
				coordinate->minor = p_event_coord->minor;

				if ((strcmp(pdata->project_name, "lassen") == 0) &&
						(strcmp(pdata->model_name, "universal7885_FHD") == 0)) {
					if (coordinate->y < 203 || coordinate->y >= 3745)
						break;
					coordinate->y -= 203;
				}

				if (!coordinate->palm && (coordinate->ttype == SEC_TS_TOUCHTYPE_PALM))
					coordinate->palm_count++;

				coordinate->palm = (coordinate->ttype == SEC_TS_TOUCHTYPE_PALM);
				coordinate->left_event = p_event_coord->left_event;

				if (coordinate->z <= 0)
					coordinate->z = 1;

				if ((coordinate->ttype == SEC_TS_TOUCHTYPE_NORMAL)
						|| (coordinate->ttype == SEC_TS_TOUCHTYPE_PALM)
						|| (coordinate->ttype == SEC_TS_TOUCHTYPE_GLOVE)) {

					if (coordinate->action == SEC_TS_COORDINATE_ACTION_RELEASE) {
						input_mt_slot(ts->input_dev, t_id);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

						if (ts->touch_count > 0)
							ts->touch_count--;
						if (ts->touch_count == 0) {
							input_report_key(ts->input_dev, BTN_TOUCH, 0);
							input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
						}

					} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_PRESS) {
						ts->touch_count++;
						input_mt_slot(ts->input_dev, t_id);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
						input_report_key(ts->input_dev, BTN_TOUCH, 1);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate->x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate->y);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate->major);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate->minor);
						input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate->palm);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, coordinate->z);

					} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_MOVE) {
						if ((coordinate->ttype == SEC_TS_TOUCHTYPE_GLOVE) && !ts->touchkey_glove_mode_status) {
							ts->touchkey_glove_mode_status = true;
							input_report_switch(ts->input_dev, SW_GLOVE, 1);
						} else if ((coordinate->ttype != SEC_TS_TOUCHTYPE_GLOVE) && ts->touchkey_glove_mode_status) {
							ts->touchkey_glove_mode_status = false;
							input_report_switch(ts->input_dev, SW_GLOVE, 0);
						}

						input_mt_slot(ts->input_dev, t_id);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
						input_report_key(ts->input_dev, BTN_TOUCH, 1);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate->x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate->y);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate->major);
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate->minor);
						input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate->palm);
						input_report_abs(ts->input_dev, ABS_MT_PRESSURE, coordinate->z);
						coordinate->mcount++;
					} else {
						input_dbg(true, &ts->client->dev,
								"%s: do not support coordinate action(%d)\n", __func__, coordinate->action);
					}
				} else {
					input_dbg(true, &ts->client->dev,
							"%s: do not support coordinate type(%d)\n", __func__, coordinate->ttype);
				}
			} else {
				input_err(true, &ts->client->dev, "%s: tid(%d) is out of range\n", __func__, t_id);
			}
			break;

		case SEC_TS_GESTURE_EVENT:
			p_gesture_status = (struct sec_ts_gesture_status *)event_buff;
			if ((p_gesture_status->eid == 0x02) && (p_gesture_status->stype == 0x00)) {
				u8 sponge[3] = { 0 };

				ret = sec_ts_read_from_sponge(ts, sponge);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to read sponge data\n", __func__);

				input_info(true, &ts->client->dev, "%s: Sponge, %x, %x, %x\n",
							__func__, sponge[0], sponge[1], sponge[2]);

				if (p_gesture_status->gesture_id == SEC_TS_GESTURE_CODE_SPAY ||
					p_gesture_status->gesture_id == SEC_TS_GESTURE_CODE_DOUBLE_TAP) {
					/* will be fixed to data structure */
					if (sponge[1] & SEC_TS_MODE_SPONGE_AOD) {
						u8 data[5] = {0x0A, 0x00, 0x00, 0x00};
						ret = sec_ts_read_from_sponge(ts, data);
						if (ret < 0)
							input_err(true, &ts->client->dev, "%s: fail to read sponge data\n", __func__);

						if (data[4] & SEC_TS_AOD_GESTURE_DOUBLETAB)
							ts->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;

						ts->scrub_x = (data[1] & 0xFF) << 8 | (data[0] & 0xFF);
						ts->scrub_y = (data[3] & 0xFF) << 8 | (data[2] & 0xFF);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
						input_info(true, &ts->client->dev, "%s: aod: %d\n",
								__func__, ts->scrub_id);
#else
						input_info(true, &ts->client->dev, "%s: aod: %d, %d, %d\n",
								__func__, ts->scrub_id, ts->scrub_x, ts->scrub_y);
#endif
					}
					if (sponge[1] & SEC_TS_MODE_SPONGE_SPAY) {
						ts->scrub_id = SPONGE_EVENT_TYPE_SPAY;
						input_info(true, &ts->client->dev, "%s: SPAY: %d\n",
									__func__, ts->scrub_id);
					}
					input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);
					input_sync(ts->input_dev);
					input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);
				}
			}

			/*
			input_info(true, &ts->client->dev, "%s: GESTURE %x %x %x %x %x %x\n", __func__,
					event_buff[0], event_buff[1], event_buff[2],
					event_buff[3], event_buff[4], event_buff[5]);
			*/
			break;

		default:
			input_err(true, &ts->client->dev, "%s: unknown event %x %x %x %x %x %x\n", __func__,
					event_buff[0], event_buff[1], event_buff[2],
					event_buff[3], event_buff[4], event_buff[5]);
			break;
		}

		if (coordinate != NULL) {
			if (coordinate->action == SEC_TS_COORDINATE_ACTION_PRESS) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev,
					"%s: [P] tID:%d, x:%d, y:%d, z:%d, major:%d, minor:%d, tc:%d, type:%X\n",
					__func__, t_id, coordinate->x, coordinate->y, coordinate->z,
					coordinate->major, coordinate->minor, ts->touch_count,
					coordinate->ttype);
#else
				input_info(true, &ts->client->dev,
					"%s: [P] tID:%d, z:%d, major:%d, minor:%d, tc:%d, type:%X\n",
					__func__, t_id, coordinate->z, coordinate->major,
					coordinate->minor, ts->touch_count, coordinate->ttype);
#endif

			} else if (coordinate->action == SEC_TS_COORDINATE_ACTION_RELEASE) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_info(true, &ts->client->dev,
					"%s: [R] tID:%d mc: %d tc:%d lx:%d ly:%d, v:%02X%02X, cal:%X(%X|%X), id(%d,%d), p:%d\n",
					__func__, t_id, coordinate->mcount, ts->touch_count,
					coordinate->x, coordinate->y,
					ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status, ts->nv, ts->cal_count, ts->tspid_val,
					ts->tspicid_val, coordinate->palm_count);
#else
				input_info(true, &ts->client->dev,
					"%s: [R] tID:%d mc: %d tc:%d, v:%02X%02X, cal:%X(%X|%X), id(%d,%d), p:%d\n",
					__func__, t_id, coordinate->mcount, ts->touch_count,
					ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status, ts->nv, ts->cal_count, ts->tspid_val,
					ts->tspicid_val, coordinate->palm_count);
#endif
				coordinate->action = SEC_TS_COORDINATE_ACTION_NONE;
				coordinate->mcount = 0;
				coordinate->palm_count = 0;
			}/* else {
				input_info(true, &ts->client->dev,
						"%s: undefined status: %X\n",
						__func__, coordinate->action);
			}*/

			coordinate = NULL;
		}
		curr_pos++;
		remain_event_count--;
		input_dbg(true, &ts->client->dev,
			"%s: curr_pos=%d, remain_event_count=%d\n", __func__, curr_pos, remain_event_count);
	} while (remain_event_count >= 0);

	input_sync(ts->input_dev);
}

static irqreturn_t sec_ts_irq_thread(int irq, void *ptr)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)ptr;

#ifdef CONFIG_SECURE_TOUCH
	if (secure_filter_interrupt(ts) == IRQ_HANDLED) {
		wait_for_completion_interruptible_timeout(&ts->secure_interrupt,
					msecs_to_jiffies(5 * MSEC_PER_SEC));

		input_info(true, &ts->client->dev,
					"%s: secure interrupt handled\n", __func__);

		return IRQ_HANDLED;
	}
#endif

	mutex_lock(&ts->eventlock);

	sec_ts_read_event(ts);

	mutex_unlock(&ts->eventlock);

	return IRQ_HANDLED;
}

int get_tsp_status(void)
{
	return 0;
}
EXPORT_SYMBOL(get_tsp_status);

void sec_ts_set_charger(bool enable)
{
	return;
/*
	int ret;
	u8 noise_mode_on[] = {0x01};
	u8 noise_mode_off[] = {0x00};

	if (enable) {
		input_info(true, &ts->client->dev, "sec_ts_set_charger : charger CONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_NOISE_MODE, noise_mode_on, sizeof(noise_mode_on));
		if (ret < 0)
			input_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_ON\n");
	} else {
		input_info(true, &ts->client->dev, "sec_ts_set_charger : charger DISCONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_NOISE_MODE, noise_mode_off, sizeof(noise_mode_off));
		if (ret < 0)
			input_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_OFF\n");
	}
 */
}
EXPORT_SYMBOL(sec_ts_set_charger);

int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode)
{
	int ret;

	if (mode)
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_GLOVE | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC);
	else
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_GLOVE)) | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: pwr off, glove:%d, status:%x\n", __func__,
					mode, ts->touch_functions);
		goto glove_enable_err;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, (u8*)&ts->touch_functions, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto glove_enable_err;
	}

	input_info(true, &ts->client->dev, "%s: glove:%d, status:%x\n", __func__,
		mode, ts->touch_functions);

	return 0;

glove_enable_err:
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_glove_mode_enables);

int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable)
{
	int ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->cover_type);


	switch (ts->cover_type) {
	case SEC_TS_VIEW_WIRELESS:
	case SEC_TS_VIEW_COVER:
	case SEC_TS_VIEW_WALLET:
	case SEC_TS_FLIP_WALLET:
	case SEC_TS_LED_COVER:
	case SEC_TS_MONTBLANC_COVER:
	case SEC_TS_CLEAR_FLIP_COVER:
	case SEC_TS_QWERTY_KEYBOARD_EUR:
	case SEC_TS_QWERTY_KEYBOARD_KOR:
		ts->cover_cmd = (u8)ts->cover_type;
		break;
	case SEC_TS_CHARGER_COVER:
	case SEC_TS_COVER_NOTHING1:
	case SEC_TS_COVER_NOTHING2:
	default:
		ts->cover_cmd = 0;
		input_err(true, &ts->client->dev, "%s: not chage touch state, %d\n",
				__func__, ts->cover_type);
		break;
	}

	if (enable)
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC);
	else
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER)) | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: pwr off, close:%d, status:%x\n", __func__,
					enable, ts->touch_functions);
		goto cover_enable_err;
	}

	if (enable) {
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to send covertype command: %d", __func__, ts->cover_cmd);
			goto cover_enable_err;
		}
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, (u8*)&(ts->touch_functions), 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto cover_enable_err;
	}

	input_info(true, &ts->client->dev, "%s: close:%d, status:%x\n", __func__,
		enable, ts->touch_functions);

	return 0;

cover_enable_err:
	return -EIO;


}
EXPORT_SYMBOL(sec_ts_set_cover_type);

#ifdef TWO_LEVEL_GRIP_CONCEPT
void sec_ts_set_grip_type(struct sec_ts_data *ts, u8 set_type)
{
	u8 mode = G_NONE;

	if (!(ts->plat_data->grip_concept & 0x2))
		return;

	input_info(true, &ts->client->dev, "%s: re-init grip(%d), edh:%d, edg:%d, lan:%d\n", __func__,\
		set_type, ts->grip_edgehandler_direction, ts->grip_edge_range, ts->grip_landscape_mode);

	/* edge handler */
	if (ts->grip_edgehandler_direction != 0)
		mode |= G_SET_EDGE_HANDLER;

	if (set_type == GRIP_ALL_DATA) {
		/* edge */
		if (ts->grip_edge_range != 60)
			mode |= G_SET_EDGE_ZONE;

		/* dead zone */
		if (ts->grip_landscape_mode == 1)	/* default 0 mode, 32 */
			mode |= G_SET_LANDSCAPE_MODE;
		else
			mode |= G_SET_NORMAL_MODE;
	}

	if (mode)
		set_grip_data_to_ic(ts, mode);

}
#endif

/* for debugging--------------------------------------------------------------------------------------*/
static ssize_t sec_ts_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
		return -EIO;
	}

	if (size > 0)
		sec_ts_i2c_write_burst(ts, (u8 *)buf, size);

	input_info(true, &ts->client->dev, "sec_ts_reg: 0x%x, 0x%x, size %d\n", buf[0], buf[1], (int)size);
	return size;
}

static ssize_t sec_ts_regread_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	int length;
	int remain;
	int offset;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
		return -EIO;
	}

	disable_irq(ts->client->irq);

	read_lv1_buff = kzalloc(lv1_readsize, GFP_KERNEL);
	if (!read_lv1_buff) {
		input_err(true, &ts->client->dev, "%s kzalloc failed\n", __func__);
		goto malloc_err;
	}

	mutex_lock(&ts->device_mutex);
	remain = lv1_readsize;
	offset = 0;
	do {
		if (remain >= ts->i2c_burstmax)
			length = ts->i2c_burstmax;
		else
			length = remain;

		if (offset == 0)
			ret = sec_ts_i2c_read(ts, lv1cmd, &read_lv1_buff[offset], length);
		else
			ret = sec_ts_i2c_read_bulk(ts, &read_lv1_buff[offset], length);

		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read %x command, remain =%d\n", __func__, lv1cmd, remain);
			goto i2c_err;
		}

		remain -= length;
		offset += length;
	} while (remain > 0);

	input_info(true, &ts->client->dev, "%s: lv1_readsize = %d\n", __func__, lv1_readsize);
	memcpy(buf, read_lv1_buff + lv1_readoffset, lv1_readsize);

i2c_err:
	kfree(read_lv1_buff);
malloc_err:
	mutex_unlock(&ts->device_mutex);
	lv1_readremain = 0;
	enable_irq(ts->client->irq);

	return lv1_readsize;
}

static ssize_t sec_ts_gesture_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->device_mutex);
	memcpy(buf, ts->gesture_status, sizeof(ts->gesture_status));
	input_info(true, &ts->client->dev,
				"sec_sec_ts_gesture_status_show GESTURE STATUS %x %x %x %x %x %x\n",
				ts->gesture_status[0], ts->gesture_status[1], ts->gesture_status[2],
				ts->gesture_status[3], ts->gesture_status[4], ts->gesture_status[5]);
	mutex_unlock(&ts->device_mutex);

	return sizeof(ts->gesture_status);
}

static ssize_t sec_ts_regreadsize_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	lv1cmd = buf[0];
	lv1_readsize = ((unsigned int)buf[4] << 24) |
			((unsigned int)buf[3] << 16) | ((unsigned int) buf[2] << 8) | ((unsigned int)buf[1] << 0);
	lv1_readoffset = 0;
	lv1_readremain = 0;
	return size;
}

static ssize_t sec_ts_enter_recovery_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	struct sec_ts_plat_data *pdata = ts->plat_data;
	int ret;
	int on;

	sscanf(buf, "%d", &on);

	if (on == 1) {
		disable_irq(ts->client->irq);
		gpio_free(pdata->gpio);

		input_info(true, &ts->client->dev, "%s: gpio free\n", __func__);
		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_OUT_INIT_LOW, "sec,tsp_int");
			input_info(true, &ts->client->dev, "%s: gpio request one\n", __func__);
			if (ret < 0)
				input_err(true, &ts->client->dev, "Unable to request tsp_int [%d]: %d\n", pdata->gpio, ret);
		} else {
			input_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(100);
		pdata->power(ts, true);
	} else {
		gpio_free(pdata->gpio);

		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
			if (ret) {
				input_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			input_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(500);
		pdata->power(ts, true);
		sec_ts_delay(500);

		/* AFE Calibration */
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CALIBRATION_AMBIENT, NULL, 0);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: fail to write AFE_CAL\n", __func__);

		sec_ts_delay(1000);
		enable_irq(ts->client->irq);
	}

	sec_ts_read_information(ts);

	return size;
}

static int sec_ts_raw_device_init(struct sec_ts_data *ts)
{
	int ret;

#ifdef CONFIG_DRV_SAMSUNG
	ts->dev = sec_device_create(ts, "sec_ts");
#else
	ts->dev = device_create(sec_class, NULL, 0, ts, "sec_ts");
#endif
	ret = IS_ERR(ts->dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail - device_create\n", __func__);
		return ret;
	}

	ret = sysfs_create_group(&ts->dev->kobj, &cmd_attr_group);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail - sysfs_create_group\n", __func__);
		goto err_sysfs;
	}
/*
	ret = sysfs_create_link(&ts->dev->kobj,
			&ts->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto err_sysfs;
	}
*/
	return ret;
err_sysfs:
	input_err(true, &ts->client->dev, "%s: fail\n", __func__);
	return ret;
}

/* for debugging--------------------------------------------------------------------------------------*/

static int sec_ts_pinctrl_configure(struct sec_ts_data *ts, bool enable)
{
	struct pinctrl_state *state;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, enable ? "ACTIVE" : "SUSPEND");

	if (enable) {
		state = pinctrl_lookup_state(ts->plat_data->pinctrl, "on_state");
		if (IS_ERR(ts->plat_data->pinctrl))
			input_err(true, &ts->client->dev, "could not get active pinstate\n");
	} else {
		state = pinctrl_lookup_state(ts->plat_data->pinctrl, "off_state");
		if (IS_ERR(ts->plat_data->pinctrl))
			input_err(true, &ts->client->dev, "could not get suspend pinstate\n");
	}

	if (!IS_ERR_OR_NULL(state))
		return pinctrl_select_state(ts->plat_data->pinctrl, state);

	return 0;

}

static int sec_ts_power(void *data, bool on)
{
	/*struct sec_ts_data *ts = (struct sec_ts_data *)data;
	const struct sec_ts_plat_data *pdata = ts->plat_data;
	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;
	static bool enabled;
	int ret = 0;

	if (enabled == on)
		return ret;

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}

	if (on) {
		ret = regulator_enable(regulator_avdd);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
			return ret;
		}

		sec_ts_delay(1);

		ret = regulator_enable(regulator_dvdd);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to enable vdd: %d\n", __func__, ret);
			return ret;
		}

		sec_ts_delay(5);
	} else {
		regulator_disable(regulator_avdd);
		regulator_disable(regulator_dvdd);
	}

	input_info(true, &ts->client->dev, "%s: %s: avdd:%s, dvdd:%s\n", __func__, on ? "on" : "off",
		regulator_is_enabled(regulator_avdd) ? "on" : "off",
		regulator_is_enabled(regulator_dvdd) ? "on" : "off");

	enabled = on;

	regulator_put(regulator_avdd);
	regulator_put(regulator_dvdd);*/

	return 0;
}

static int sec_ts_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	u32 coords[2], lines[2];
	int ret = 0;
	int count = 0;
	u32 ic_match_value;
	u32 lcdtype = 0;
#if 0 
	int connected;
#endif
	pdata->tsp_icid = of_get_named_gpio(np, "sec,tsp-icid_gpio", 0);
	if (gpio_is_valid(pdata->tsp_icid)) {
		input_info(true, dev, "TSP_ICID : %d\n", gpio_get_value(pdata->tsp_icid));
		if (of_property_read_u32(np, "sec,icid_match_value", &ic_match_value)) {
			input_err(true, dev, "Failed to get icid match value\n");
			return -EINVAL;
		}

		if (gpio_get_value(pdata->tsp_icid) != ic_match_value) {
			input_err(true, dev, "Do not match TSP_ICID\n");
			return -EINVAL;
		}
	} else {
		input_err(true, dev, "Failed to get tsp-icid gpio\n");
	}

	pdata->tsp_vsync = of_get_named_gpio(np, "sec,tsp_vsync_gpio", 0);
	if (gpio_is_valid(pdata->tsp_vsync))
		input_info(true, &client->dev, "vsync %s\n", gpio_get_value(pdata->tsp_vsync) ? "disable" : "enable");

	pdata->gpio = of_get_named_gpio(np, "sec,irq_gpio", 0);
	if (gpio_is_valid(pdata->gpio)) {
		ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
		if (ret) {
			input_err(true, &client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
			return -EINVAL;
		}
	} else {
		input_err(true, &client->dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}

	client->irq = gpio_to_irq(pdata->gpio);

	if (of_property_read_u32(np, "sec,irq_type", &pdata->irq_type)) {
		input_err(true, dev, "Failed to get irq_type property\n");
		pdata->irq_type = IRQF_TRIGGER_LOW | IRQF_ONESHOT;
	}

	if (of_property_read_u32(np, "sec,use_ic_info", &use_ic_info)) {
		input_err(true, dev, "Failed to get ic_info property\n");
	}

	if (of_property_read_u32(np, "sec,i2c-burstmax", &pdata->i2c_burstmax)) {
		input_err(true, &client->dev, "Failed to get i2c_burstmax property\n");
		pdata->i2c_burstmax = 256;
	}

	if (of_property_read_u32_array(np, "sec,max_coords", coords, 2)) {
		input_err(true, &client->dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pdata->max_x = coords[0] - 1;
	pdata->max_y = coords[1] - 1;

	of_property_read_u32(np, "sec,grip_area", &pdata->grip_area);

#ifdef PAT_CONTROL
	if (of_property_read_u32(np, "sec,pat_function", &pdata->pat_function) < 0) {
		pdata->pat_function = 0;
		input_err(true, dev, "Failed to get pat_function property\n");
	}

	if (of_property_read_u32(np, "sec,afe_base", &pdata->afe_base) < 0) {
		pdata->afe_base = 0;
		input_err(true, dev, "Failed to get afe_base property\n");
	}
#endif

	if (of_property_read_u32_array(np, "sec,num_lines", lines, 2)) {
		input_info(true, &client->dev, "skipped to get num_lines property\n");
	} else {
		pdata->num_rx = lines[0];
		pdata->num_tx = lines[1];
		input_info(true, &client->dev, "num_of[rx,tx]: [%d,%d]\n",
				pdata->num_rx, pdata->num_tx);
	}

	pdata->tsp_id = of_get_named_gpio(np, "sec,tsp-id_gpio", 0);
	if (gpio_is_valid(pdata->tsp_id))
		input_info(true, dev, "TSP_ID : %d\n", gpio_get_value(pdata->tsp_id));
	else
		input_err(true, dev, "Failed to get tsp-id gpio\n");

	count = of_property_count_strings(np, "sec,firmware_name");
	if (count <= 0) {
		pdata->firmware_name = NULL;
	} else {
		if (gpio_is_valid(pdata->tsp_id))
			of_property_read_string_index(np, "sec,firmware_name", gpio_get_value(pdata->tsp_id), &pdata->firmware_name);
		else
			of_property_read_string_index(np, "sec,firmware_name", 0, &pdata->firmware_name);
	}

	if (of_property_read_string_index(np, "sec,project_name", 0, &pdata->project_name))
		input_info(true, &client->dev, "skipped to get project_name property\n");
	if (of_property_read_string_index(np, "sec,project_name", 1, &pdata->model_name))
		input_info(true, &client->dev, "skipped to get model_name property\n");

#if defined(CONFIG_FB_MSM_MDSS_SAMSUNG)
		lcdtype = get_lcd_attached("GET");
		if (lcdtype == 0xFFFFFF) {
			input_err(true, &client->dev, "%s: lcd is not attached\n", __func__);
			return -ENODEV;
		}
#endif

#if 0
	connected = get_lcd_info("connected");
	if (connected < 0) { 
		input_err(true, dev, "Failed to get lcd info\n");
		return -EINVAL;
	}	 

	if (!connected) {
		input_err(true, &client->dev, "%s: lcd is disconnected\n", __func__);
		return -ENODEV;
	}	 

	input_info(true, &client->dev, "%s: lcd is connected\n", __func__);

	lcdtype = get_lcd_info("id");
	if (lcdtype < 0) { 
		input_err(true, dev, "Failed to get lcd info\n");
		return -EINVAL;
	}	 

	input_info(true, &client->dev, "%s: lcdtype 0x%08X\n", __func__, lcdtype);
#endif 

	if (strncmp(pdata->model_name, "G950", 4) == 0)
		pdata->panel_revision = 0;
	else
		pdata->panel_revision = ((lcdtype >> 8) & 0xFF) >> 4;

	/*if (of_property_read_string(np, "sec,regulator_dvdd", &pdata->regulator_dvdd)) {
		input_err(true, dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}

	if (of_property_read_string(np, "sec,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}*/

	pdata->power = sec_ts_power;

	if (of_property_read_u32(np, "sec,always_lpmode", &pdata->always_lpmode) < 0)
		pdata->always_lpmode = 0;

	if (of_property_read_u32(np, "sec,bringup", &pdata->bringup) < 0)
		pdata->bringup = 0;

	if (of_property_read_u32(np, "sec,mis_cal_check", &pdata->mis_cal_check) < 0)
		pdata->mis_cal_check = 0;

	if (of_property_read_u32(np, "sec,grip_concept", &pdata->grip_concept) < 0)
		pdata->grip_concept = 1;	// default 1(set_tunning_data) for Hero.

#ifdef PAT_CONTROL
	input_err(true, &client->dev, "%s: i2c buffer limit: %d, lcd_id:%06X, bringup:%d, FW:%s(%d), id:%d,%d, grip:%d pat_function:%d mis_cal:%d grip_cc:%d\n",
			__func__, pdata->i2c_burstmax, lcdtype, pdata->bringup, pdata->firmware_name, \
			count, pdata->tsp_id, pdata->tsp_icid, pdata->grip_area, pdata->pat_function, \
			pdata->mis_cal_check, pdata->grip_concept);
#else
	input_err(true, &client->dev, "%s: i2c buffer limit: %d, lcd_id:%06X, bringup:%d, FW:%s(%d), id:%d,%d, grip:%d pat_function:%d mis_cal:%d grip_cc:%d\n",
		__func__, pdata->i2c_burstmax, lcdtype, pdata->bringup, pdata->firmware_name, \
		count, pdata->tsp_id, pdata->tsp_icid, pdata->grip_area, pdata->grip_concept);
#endif
	return ret;
}

static int sec_ts_read_information(struct sec_ts_data *ts)
{
	unsigned char data[13] = { 0 };
	int ret;

	memset(data, 0x0, 3);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_ID, data, 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
					"%s: failed to read device id(%d)\n",
					__func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev,
				"%s: %X, %X, %X\n",
				__func__, data[0], data[1], data[2]);
	memset(data, 0x0, 11);
	ret = sec_ts_i2c_read(ts,  SEC_TS_READ_PANEL_INFO, data, 11);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev,
				"%s: nTX:%X, nRX:%X, rY:%d, rX:%d\n",
				__func__, data[8], data[9],
				(data[2] << 8) | data[3], (data[0] << 8) | data[1]);

	/* Set X,Y Resolution from IC information. */

	if(use_ic_info) {
		if (((data[0] << 8) | data[1]) > 0)
			ts->plat_data->max_x = ((data[0] << 8) | data[1]) - 1;

		if (((data[2] << 8) | data[3]) > 0)
			ts->plat_data->max_y = ((data[2] << 8) | data[3]) - 1;
	}

	ts->tx_count = data[8];
	ts->rx_count = data[9];

	data[0] = 0;
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, data, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev,
				"%s: STATUS : %X\n",
				__func__, data[0]);

	memset(data, 0x0, 4);
	ret = sec_ts_i2c_read(ts, SEC_TS_READ_TS_STATUS, data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev,
				"%s: TOUCH STATUS : %02X, %02X, %02X, %02X\n",
				__func__, data[0], data[1], data[2], data[3]);
	ret = sec_ts_i2c_read(ts, SEC_TS_CMD_SET_TOUCHFUNCTION,  (u8*)&(ts->touch_functions), 2);
	if (ret < 0) {
		dev_err(&ts->client->dev,
					"%s: failed to read touch functions(%d)\n",
					__func__, ret);
		return ret;
	}

	dev_info(&ts->client->dev,
				"%s: Functions : %02X\n",
				__func__, ts->touch_functions);

	return ret;
}

#ifdef SEC_TS_SUPPORT_SPONGELIB
int sec_ts_set_custom_library(struct sec_ts_data *ts)
{
	u8 data[3] = { 0 };
	int ret;

	input_err(true, &ts->client->dev, "%s: Sponge (%d)\n",
				__func__, ts->lowpower_mode);

	data[2] = ts->lowpower_mode;

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_WRITE_PARAM, &data[0], 3);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to Sponge\n", __func__);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_NOTIFY_PACKET, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send NOTIFY SPONGE\n", __func__);

	return ret;
}

int sec_ts_check_custom_library(struct sec_ts_data *ts)
{
	u8 data[10] = { 0 };
	int ret = -1;

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_GET_INFO, &data[0], 10);

	input_info(true, &ts->client->dev,
				"%s: (%d) %c%c%c%c, || %02X, %02X, %02X, %02X, || %02X, %02X\n",
				__func__, ret, data[0], data[1], data[2], data[3], data[4],
				data[5], data[6], data[7], data[8], data[9]);

	/* compare model name with device tree */
	if (ts->plat_data->model_name)
		ret = strncmp(data, ts->plat_data->model_name, 4);

	if (ret == 0)
		ts->use_sponge = true;
	else
		ts->use_sponge = false;

	input_info(true, &ts->client->dev, "%s: use %s\n",
				__func__, ts->use_sponge ? "SPONGE" : "VENDOR");

	return ret;
}
#endif

static int sec_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct sec_ts_data *ts;
	struct exynos5_i2c *i2c_master = (struct exynos5_i2c *)client->adapter->algo_data;
	struct sec_ts_plat_data *pdata;
	static char sec_ts_phys[64] = { 0 };
	int ret = 0;
	bool force_update = false;
	bool valid_firmware_integrity = false;
	unsigned char data[5] = { 0 };
	unsigned char deviceID[5] = { 0 };
	unsigned char result = 0;
/* TEMP
	if (tsp_init_done) {
		input_err(true, &client->dev, "%s: tsp already init done\n", __func__);
		return -ENODEV;
	}
*/
	input_info(true, &client->dev, "%s\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s : EIO err!\n", __func__);
		return -EIO;
	}

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate platform data\n");
			goto error_allocate_pdata;
		}

		client->dev.platform_data = pdata;

		ret = sec_ts_parse_dt(client);
		if (ret) {
			input_err(true, &client->dev, "Failed to parse dt\n");
			goto error_allocate_mem;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			input_err(true, &client->dev, "No platform data found\n");
			goto error_allocate_pdata;
		}
	}

	if (!pdata->power) {
		input_err(true, &client->dev, "No power contorl found\n");
		goto error_allocate_mem;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl))
		input_err(true, &client->dev, "could not get pinctrl\n");

	ts = kzalloc(sizeof(struct sec_ts_data), GFP_KERNEL);
	if (!ts) {
		input_err(true, &client->dev, "%s: Failed to alloc mem for info\n", __func__);
		goto error_allocate_mem;
	}

	ts->client = client;
	ts->plat_data = pdata;
	ts->crc_addr = 0x0001FE00;
	ts->fw_addr = 0x00002000;
	ts->para_addr = 0x18000;
	ts->flash_page_size = SEC_TS_FW_BLK_SIZE_DEFAULT;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_write_burst = sec_ts_i2c_write_burst;
	ts->sec_ts_i2c_read_bulk = sec_ts_i2c_read_bulk;
	ts->i2c_burstmax = pdata->i2c_burstmax;
#ifdef USE_RESET_DURING_POWER_ON
	INIT_DELAYED_WORK(&ts->reset_work, sec_ts_reset_work);
#endif
	INIT_DELAYED_WORK(&ts->work_read_nv, sec_ts_read_nv_work);

	i2c_set_clientdata(client, ts);

	if (gpio_is_valid(ts->plat_data->tsp_id))
		ts->tspid_val = gpio_get_value(ts->plat_data->tsp_id);

	if (gpio_is_valid(ts->plat_data->tsp_icid))
		ts->tspicid_val = gpio_get_value(ts->plat_data->tsp_icid);

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		input_err(true, &ts->client->dev, "%s: allocate device err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_device;
	}

	i2c_master->stop_after_trans = 1;

	ts->input_dev->name = "sec_touchscreen";
	snprintf(sec_ts_phys, sizeof(sec_ts_phys), "%s/input1",
			ts->input_dev->name);
	ts->input_dev->phys = sec_ts_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &client->dev;
	ts->touch_count = 0;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
	ts->sec_ts_read_sponge = sec_ts_read_from_sponge;
	mutex_init(&ts->lock);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);
	mutex_init(&ts->eventlock);

	wake_lock_init(&ts->wakelock, WAKE_LOCK_SUSPEND, "tsp_wakelock");
	init_completion(&ts->resume_done);

#ifdef USE_OPEN_CLOSE
	ts->input_dev->open = sec_ts_input_open;
	ts->input_dev->close = sec_ts_input_close;
#endif

	if (pdata->always_lpmode)
		ts->lowpower_mode |= SEC_TS_MODE_SPONGE_FORCE_KEY;
	else
		ts->lowpower_mode &= ~SEC_TS_MODE_SPONGE_FORCE_KEY;

	input_err(true, &client->dev, "%s init resource\n", __func__);

	sec_ts_pinctrl_configure(ts, true);

	/* power enable */
	sec_ts_power(ts, true);

	sec_ts_delay(70);
	ts->power_status = SEC_TS_STATE_POWER_ON;

	sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);

	input_err(true, &client->dev, "%s power enable\n", __func__);

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_DEVICE_ID, deviceID, 5);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to read device ID(%d)\n", __func__, ret);
	else
		input_info(true, &ts->client->dev, 
			"%s: TOUCH DEVICE ID : %02X, %02X, %02X, %02X, %02X\n", __func__,
			deviceID[0], deviceID[1], deviceID[2], deviceID[3], deviceID[4]); 

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_FIRMWARE_INTEGRITY, &result, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to integrity check (%d)\n", __func__, ret);
	} else {
		if (result & 0x80) {
			valid_firmware_integrity= true;
		} else if (result & 0x40) {
			valid_firmware_integrity= false;
			input_err(true, &ts->client->dev, "%s: invalid firmware (0x%x)\n", __func__, result);
		} else {
			valid_firmware_integrity = false;
			input_err(true, &ts->client->dev, "%s: invalid integrity result (0x%x)\n", __func__, result);
		}
	}

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, &data[0], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
					"%s: failed to read sub id(%d)\n",
					__func__, ret);
	} else {
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_TS_STATUS, &data[1], 4);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
						"%s: failed to touch status(%d)\n",
						__func__, ret);
		}
	}
	input_info(true, &ts->client->dev,
				"%s: TOUCH STATUS : %02X || %02X, %02X, %02X, %02X\n",
				__func__, data[0], data[1], data[2], data[3], data[4]);
	if ((((data[0] == SEC_TS_STATUS_APP_MODE) && (data[2] == TOUCH_SYSTEM_MODE_FLASH)) || (ret < 0))
		 && ( valid_firmware_integrity == false))
		force_update = true;
	else
		force_update = false;

	force_update = true;
#ifdef SEC_TS_FW_UPDATE_ON_PROBE
	ret = sec_ts_firmware_update_on_probe(ts, force_update);
	if (ret < 0)
		goto err_init;
#else
	input_info(true, &ts->client->dev, "%s: fw update on probe disabled!\n", __func__);
#endif

	ret = sec_ts_read_information(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "sec_ts_probe: fail to read information 0x%x\n",ret);
		goto err_init;
	}

	ts->touch_functions = ts->touch_functions | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC;
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, (u8*)&ts->touch_functions, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send tuoch func_mode command", __func__);

	/* Sense_on */
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");
		goto err_init;
	}

	ts->pFrame = kzalloc(ts->tx_count * ts->rx_count * 2, GFP_KERNEL);
	if (!ts->pFrame) {
		input_err(true, &ts->client->dev, "%s: allocate pFrame err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_frame;
	}

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_SW, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, ts->input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, ts->input_dev->keybit);
#ifdef SEC_TS_SUPPORT_TOUCH_KEY
	if (ts->plat_data->support_mskey) {
		int i;

		for (i = 0 ; i < ts->plat_data->num_touchkey ; i++)
			set_bit(ts->plat_data->touchkey[i].keycode, ts->input_dev->keybit);

		set_bit(EV_LED, ts->input_dev->evbit);
		set_bit(LED_MISC, ts->input_dev->ledbit);
	}
#endif
	set_bit(KEY_SIDE_GESTURE, ts->input_dev->keybit);
	set_bit(KEY_SIDE_GESTURE_RIGHT, ts->input_dev->keybit);
	set_bit(KEY_SIDE_GESTURE_LEFT, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
	set_bit(KEY_HOMEPAGE, ts->input_dev->keybit);

	input_set_capability(ts->input_dev, EV_SW, SW_GLOVE);

	input_mt_init_slots(ts->input_dev, MAX_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->plat_data->max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->plat_data->max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#ifdef SEC_TS_SUPPORT_GRIP_EVENT
	input_set_abs_params(ts->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_drvdata(ts->input_dev, ts);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device;
	}

	input_info(true, &ts->client->dev, "sec_ts_probe request_irq = %d\n" , client->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_ts_irq_thread,
			ts->plat_data->irq_type, SEC_TS_I2C_NAME, ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "sec_ts_probe: Unable to request threaded irq\n");
		goto err_irq;
	}


#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tsp_info = ts;

	trustedui_set_tsp_irq(client->irq);
	input_info(true, &client->dev, "%s[%d] called!\n",
		__func__, client->irq);
	#endif

	sec_ts_raw_device_init(ts);
	sec_ts_fn_init(ts);

#ifdef CONFIG_SECURE_TOUCH
	if (sysfs_create_group(&ts->input_dev->dev.kobj, &secure_attr_group) < 0)
		input_err(true, &ts->client->dev, "%s: do not make secure group\n", __func__);
	else
		secure_touch_init(ts);
#endif

	device_init_wakeup(&client->dev, true);

	schedule_delayed_work(&ts->work_read_nv, msecs_to_jiffies(100));

#ifdef SEC_TS_SUPPORT_SPONGELIB
	sec_ts_check_custom_library(ts);
	if (ts->use_sponge)
		sec_ts_set_custom_library(ts);

#endif

#if defined(CONFIG_TOUCHSCREEN_DUMP_MODE)
		dump_callbacks.inform_dump = dump_tsp_log;
		INIT_DELAYED_WORK(&ts->ghost_check, sec_ts_check_rawdata);
		p_ghost_check = &ts->ghost_check;
#endif

	ts_dup = ts;
/* TEMP
	tsp_init_done = true;
*/
	return 0;

err_irq:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device:
	if (ts->input_dev)
		input_free_device(ts->input_dev);
	kfree(ts->pFrame);
err_allocate_frame:
err_init:
	wake_lock_destroy(&ts->wakelock);
	sec_ts_power(ts, false);
err_allocate_device:
	kfree(ts);

error_allocate_mem:
	if (gpio_is_valid(pdata->gpio))
		gpio_free(pdata->gpio);
	if (gpio_is_valid(pdata->tsp_id))
		gpio_free(pdata->tsp_id);
	if (gpio_is_valid(pdata->tsp_icid))
		gpio_free(pdata->tsp_icid);

error_allocate_pdata:
	if (ret == -ECONNREFUSED)
		sec_ts_delay(100);
	ret = -ENODEV;
#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	p_ghost_check = NULL;
#endif
	ts_dup = NULL;
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tsp_info = NULL;
#endif
	return ret;
}

void sec_ts_unlocked_release_all_finger(struct sec_ts_data *ts)
{
	int i;

	for (i = 0; i < MAX_SUPPORT_TOUCH_COUNT; i++) {
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);

		if ((ts->coord[i].action == SEC_TS_COORDINATE_ACTION_PRESS) ||
 			(ts->coord[i].action == SEC_TS_COORDINATE_ACTION_MOVE)) {

 			ts->coord[i].action = SEC_TS_COORDINATE_ACTION_RELEASE;
			input_info(true, &ts->client->dev,
					"%s: [RA] tID:%d mc: %d tc:%d, v:%02X%02X, cal:%X(%X|%X), id(%d,%d), p:%d\n",
					__func__, i, ts->coord[i].mcount, ts->touch_count,
					ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status, ts->nv, ts->cal_count, ts->tspid_val,
					ts->tspicid_val, ts->coord[i].palm_count);
 		}

		ts->coord[i].mcount = 0;
		ts->coord[i].palm_count = 0;

	}

	input_mt_slot(ts->input_dev, 0);

	input_report_key(ts->input_dev, BTN_TOUCH, false);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, false);
	input_report_switch(ts->input_dev, SW_GLOVE, false);
	ts->touchkey_glove_mode_status = false;
	ts->touch_count = 0;

	input_report_key(ts->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
	input_report_key(ts->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
	input_report_key(ts->input_dev, KEY_HOMEPAGE, 0);

	input_sync(ts->input_dev);

}

void sec_ts_locked_release_all_finger(struct sec_ts_data *ts)
{
	int i;

	mutex_lock(&ts->eventlock);

	for (i = 0; i < MAX_SUPPORT_TOUCH_COUNT; i++) {
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);

		if ((ts->coord[i].action == SEC_TS_COORDINATE_ACTION_PRESS) ||
 			(ts->coord[i].action == SEC_TS_COORDINATE_ACTION_MOVE)) {

 			ts->coord[i].action = SEC_TS_COORDINATE_ACTION_RELEASE;
			input_info(true, &ts->client->dev,
					"%s: [RA] tID:%d mc: %d tc:%d, v:%02X%02X, cal:%X(%X|%X), id(%d,%d), p:%d\n",
					__func__, i, ts->coord[i].mcount, ts->touch_count,
					ts->plat_data->img_version_of_ic[2],
					ts->plat_data->img_version_of_ic[3],
					ts->cal_status, ts->nv, ts->cal_count, ts->tspid_val,
					ts->tspicid_val, ts->coord[i].palm_count);
 		}

		ts->coord[i].mcount = 0;
		ts->coord[i].palm_count = 0;

	}

	input_mt_slot(ts->input_dev, 0);

	input_report_key(ts->input_dev, BTN_TOUCH, false);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, false);
	input_report_switch(ts->input_dev, SW_GLOVE, false);
	ts->touchkey_glove_mode_status = false;
	ts->touch_count = 0;

	input_report_key(ts->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
	input_report_key(ts->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
	input_report_key(ts->input_dev, KEY_HOMEPAGE, 0);

	input_sync(ts->input_dev);

	mutex_unlock(&ts->eventlock);

}

#ifdef USE_RESET_DURING_POWER_ON
static void sec_ts_reset_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
							reset_work.work);

	u8 temp_lpm = 0;
	u8 temp_status = 0;

/* TEMP
	if (!tsp_init_done) {
		input_err(true, &ts->client->dev, "%s: is not done, return\n", __func__);
		return;
	}
*/
	input_info(true, &ts->client->dev, "%s\n", __func__);

	temp_lpm = ts->lowpower_mode;
	temp_status = ts->power_status;

	ts->lowpower_mode = 0;
	
	sec_ts_stop_device(ts);

	sec_ts_delay(30);

	sec_ts_start_device(ts);

	ts->lowpower_mode = temp_lpm;
	if ((ts->lowpower_mode) && (temp_status < SEC_TS_STATE_POWER_ON))
		sec_ts_input_close(ts->input_dev);

}
#endif

static void sec_ts_read_nv_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
							work_read_nv.work);

	ts->nv = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
	ts->cal_count = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_COUNT);

	input_info(true, &ts->client->dev, "%s: fac_nv:%02X, cal_nv:%02X\n", __func__, ts->nv, ts->cal_count);
}

static int sec_ts_set_lowpowermode(struct sec_ts_data *ts, u8 mode)
{
	int ret;
	int retrycnt = 0;
	u8 data;
	char para = 0;

	input_err(true, &ts->client->dev, "%s: %s(%X)\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT", ts->lowpower_mode);

	if (mode) {
		if (ts->use_sponge)
			sec_ts_set_custom_library(ts);

		data = (ts->lowpower_mode & SEC_TS_MODE_LOWPOWER_FLAG) >> 1;
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_WAKEUP_GESTURE_MODE, &data, 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to set\n", __func__);
	}

retry_pmode:
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &mode, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: failed\n", __func__);
	sec_ts_delay(50);

	/* read data */

	ret = sec_ts_i2c_read(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: read power mode failed!\n", __func__);
	else
		input_err(true, &ts->client->dev, "%s: power mode - write(%d) read(%d)\n", __func__, mode, para);

	if (mode != para) {
		retrycnt++;
		if (retrycnt < 5)
			goto retry_pmode;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);


	sec_ts_locked_release_all_finger(ts);

	if (device_may_wakeup(&ts->client->dev)) {
		if (mode)
			enable_irq_wake(ts->client->irq);
		else
			disable_irq_wake(ts->client->irq);
	}

	ts->lowpower_status = mode;
	input_info(true, &ts->client->dev, "%s end\n", __func__);

	return ret;
}

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);
	int ret;

	ts->input_closed = false;

	input_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef CONFIG_SECURE_TOUCH
	secure_touch_stop(ts, 0);
#endif

	if (ts->lowpower_status) {
#ifdef USE_RESET_EXIT_LPM
		schedule_delayed_work(&ts->reset_work, msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));
#else
		sec_ts_set_lowpowermode(ts, TO_TOUCH_MODE);
#endif
	} else {
		ret = sec_ts_start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

#ifdef TWO_LEVEL_GRIP_CONCEPT
	sec_ts_set_grip_type(ts, ONLY_EDGE_HANDLER);	// because edge and dead zone will recover soon
#endif

	return 0;
}

static void sec_ts_input_close(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);

	ts->input_closed = true;

	input_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef CONFIG_SECURE_TOUCH
	secure_touch_stop(ts, 1);
#endif
#ifdef USE_RESET_DURING_POWER_ON
	cancel_delayed_work(&ts->reset_work);
#endif

	if (ts->lowpower_mode) {
		sec_ts_set_lowpowermode(ts, TO_LOWPOWER_MODE);

		ts->power_status = SEC_TS_STATE_LPM_RESUME;
	} else {
		sec_ts_stop_device(ts);
	}

}
#endif

static int sec_ts_remove(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

#ifdef USE_RESET_DURING_POWER_ON
	cancel_delayed_work(&ts->reset_work);
#endif
	sec_ts_fn_remove(ts);

	free_irq(client->irq, ts);

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
	p_ghost_check = NULL;
#endif
	device_init_wakeup(&client->dev, false);
	wake_lock_destroy(&ts->wakelock);

	input_mt_destroy_slots(ts->input_dev);
	input_unregister_device(ts->input_dev);

	ts->input_dev = NULL;
	ts_dup = NULL;
	ts->plat_data->power(ts, false);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tsp_info = NULL;
#endif

	kfree(ts);
	return 0;
}

static void sec_ts_shutdown(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_ts_remove(client);
}

static int sec_ts_stop_device(struct sec_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}

	ts->power_status = SEC_TS_STATE_POWER_OFF;

	disable_irq(ts->client->irq);
	sec_ts_locked_release_all_finger(ts);

	ts->plat_data->power(ts, false);

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(false);

	sec_ts_pinctrl_configure(ts, false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

static int sec_ts_start_device(struct sec_ts_data *ts)
{
	int ret;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_ts_pinctrl_configure(ts, true);

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	sec_ts_locked_release_all_finger(ts);

	ts->plat_data->power(ts, true);
	sec_ts_delay(70);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(true);

	if (ts->flip_enable) {
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);

		ts->touch_functions = ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER;
		input_info(true, &ts->client->dev,
				"%s: cover cmd write type:%d, mode:%x, ret:%d", __func__, ts->touch_functions, ts->cover_cmd, ret);
	} else {
		ts->touch_functions = (ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER));
		input_info(true, &ts->client->dev,
				"%s: cover open, not send cmd", __func__);
	}

	ts->touch_functions = ts->touch_functions | SEC_TS_DEFAULT_ENABLE_BIT_SETFUNC;
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, (u8*)&ts->touch_functions, 2);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: Failed to send touch function command", __func__);

	/* Sense_on */
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");

	enable_irq(ts->client->irq);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

#ifdef CONFIG_PM
static int sec_ts_pm_suspend(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
/*
	mutex_lock(&ts->input_dev->mutex);
	if (ts->input_dev->users)
		sec_ts_stop_device(ts);
	mutex_unlock(&ts->input_dev->mutex);
*/

	if (ts->lowpower_mode) {
		ts->power_status = SEC_TS_STATE_LPM_SUSPEND;
		reinit_completion(&ts->resume_done);
	}

	return 0;
}

static int sec_ts_pm_resume(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
/*
	mutex_lock(&ts->input_dev->mutex);
	if (ts->input_dev->users)
		sec_ts_start_device(ts);
	mutex_unlock(&ts->input_dev->mutex);
*/

	if (ts->lowpower_mode) {
		ts->power_status = SEC_TS_STATE_LPM_RESUME;
		complete_all(&ts->resume_done);
	}

	return 0;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	if (!tui_tsp_info)
		return;

	sec_ts_unlocked_release_all_finger(tui_tsp_info);
}

void trustedui_mode_off(void){
	if (!tui_tsp_info)
		return;
}
#endif


static const struct i2c_device_id sec_ts_id[] = {
	{ SEC_TS_I2C_NAME, 0 },
	{ },
};

#ifdef CONFIG_PM
static const struct dev_pm_ops sec_ts_dev_pm_ops = {
	.suspend = sec_ts_pm_suspend,
	.resume = sec_ts_pm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id sec_ts_match_table[] = {
	{ .compatible = "sec,sec_ts",},
	{ },
};
#else
#define sec_ts_match_table NULL
#endif

static struct i2c_driver sec_ts_driver = {
	.probe		= sec_ts_probe,
	.remove		= sec_ts_remove,
	.shutdown	= sec_ts_shutdown,
	.id_table	= sec_ts_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SEC_TS_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = sec_ts_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &sec_ts_dev_pm_ops,
#endif
	},
};

static int __init sec_ts_init(void)
{
	pr_err("%s %s\n", SECLOG, __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		pr_err("%s %s : Do not load driver due to : lpm %d\n",
				SECLOG, __func__, lpcharge);
		return -ENODEV;
	}
#endif

	return i2c_add_driver(&sec_ts_driver);
}

static void __exit sec_ts_exit(void)
{
	i2c_del_driver(&sec_ts_driver);
}

MODULE_AUTHOR("Hyobae, Ahn<hyobae.ahn@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics TouchScreen driver");
MODULE_LICENSE("GPL");

module_init(sec_ts_init);
module_exit(sec_ts_exit);
