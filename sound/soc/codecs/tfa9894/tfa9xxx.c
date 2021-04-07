/*
 * Copyright (C) 2014-2018 NXP Semiconductors, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/module.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include "tfa9xxx.h"

/* required for enum tfa9912_irq */
/* #include "tfa9xxx_tfafieldnames.h" */

#if defined(TFA9XXX_GIT_VERSION)
  #define TFA9XXX_VERSION TFA9XXX_GIT_VERSION
#else
  #define TFA9XXX_VERSION "v8.0.5-Oct.31,2018"
#endif

#define TFA_READ_BATTERY_TEMP
#if defined(TFA_READ_BATTERY_TEMP)
#include <linux/power_supply.h>
#endif

#if defined(TFA_NO_SND_FORMAT_CHECK)
#define TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION
#endif

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */

/* Supported rates and data formats */
#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
#define TFA9XXX_RATES SNDRV_PCM_RATE_8000_48000
#else
#define TFA9XXX_RATES SNDRV_PCM_RATE_8000_192000
#endif
#define TFA9XXX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

int tfa9xxx_log_revision;
int tfa9xxx_log_subrevision;
int tfa9xxx_log_i2c_devicenum;
int tfa9xxx_log_i2c_slaveaddress;
int tfa9xxx_log_start_cnt;

#if defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
static unsigned int sr_converted = 48000;
#endif

/* data accessible by all instances */
/* static struct kmem_cache *tfa9xxx_cache = NULL; */
/* Memory pool used for DSP messages */
/* Mutex protected data */
static DEFINE_MUTEX(tfa9xxx_mutex);
static LIST_HEAD(tfa9xxx_device_list);
static int tfa9xxx_device_count = 0;
static int tfa9xxx_sync_count = 0;
static LIST_HEAD(profile_list);        /* list of user selectable profiles */
static int tfa9xxx_mixer_profiles = 0; /* number of user selectable profiles */
static int tfa9xxx_mixer_profile = 0;  /* current mixer profile */
static struct snd_kcontrol_new *tfa9xxx_controls;
static struct tfa_container *tfa9xxx_container = NULL;

static int tfa9xxx_kmsg_regs = 0;
static int tfa9xxx_ftrace_regs = 0;

static char *fw_name = "Tfa9xxx.cnt";
module_param(fw_name, charp, 0644);
MODULE_PARM_DESC(fw_name, "TFA9xxx DSP firmware (container file) name\n");

static DEFINE_MUTEX(probe_lock);

static int trace_level;
module_param(trace_level, int, 0444);
MODULE_PARM_DESC(trace_level, "TFA9xxx debug trace level"
	"(0=off, bits:1=verbose,2=regdmesg,3=regftrace,4=timing)\n");

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, 0444);

static int no_start;
module_param(no_start, int, 0444);
MODULE_PARM_DESC(no_start, "do not start the work queue;"
	"for debugging via user\n");

static int no_reset;
module_param(no_reset, int, 0444);
MODULE_PARM_DESC(no_reset, "do not use the reset line;"
	"for debugging via user\n");

static int no_monitor;
module_param(no_monitor, int, 0444);
MODULE_PARM_DESC(no_monitor, "do not use status monitor thread\n");

static int pcm_sample_format;
module_param(pcm_sample_format, int, 0444);
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format:"
	"0=S16_LE, 1=S24_LE, 2=S32_LE\n");

#if defined(TFA_NO_SND_FORMAT_CHECK)
static int pcm_no_constraint = 1;
#else
static int pcm_no_constraint;
#endif
module_param(pcm_no_constraint, int, 0444);
MODULE_PARM_DESC(pcm_no_constraint,
	"do not use constraints for PCM parameters\n");

static int haptic_version_check = 1;
module_param(haptic_version_check, int, 0444);
MODULE_PARM_DESC(haptic_version_check, "Haptic firmware version check\n");

static int haptic_recalc_play_object;
module_param(haptic_recalc_play_object, int, 0444);
MODULE_PARM_DESC(haptic_version_check,
	"Haptic play object during recalculation\n");

static int tfa9xxx_get_fssel(unsigned int rate);
#if defined(TFA_ENABLE_INTERRUPT)
static void tfa9xxx_interrupt_enable(struct tfa9xxx *tfa9xxx, bool enable);
#endif

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

#if defined(TFA_READ_BATTERY_TEMP)
static int tfa9xxx_read_battery_temp(int *value);
#endif

struct tfa9xxx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static const struct tfa9xxx_rate rate_to_fssel[] = {
	{8000, 0},
	{11025, 1},
	{12000, 2},
	{16000, 3},
	{22050, 4},
	{24000, 5},
	{32000, 6},
	{44100, 7},
	{48000, 8},
#if defined(TFA_NO_SND_FORMAT_CHECK)
	/* out of range */
	{64000, 9},
	{88200, 10},
	{96000, 11},
	{176400, 12},
	{192000, 13},
#endif
};

extern void exynos9610_set_xclkout0_13(void);

static inline char *tfa_cnt_profile_name(struct tfa9xxx *tfa9xxx, int prof_idx)
{
	if (!tfa9xxx->tfa->cnt)
		return NULL;
	return tfa2_cnt_profile_name(tfa9xxx->tfa->cnt,
		tfa9xxx->tfa->dev_idx, prof_idx);
}

/* Wrapper for tfa start */
static int tfa9xxx_tfa_start(struct tfa9xxx *tfa9xxx,
	int next_profile, int vstep)
{
	int err;
	ktime_t start_time, stop_time;
	u64 delta_time;

	start_time = ktime_get_boottime();

	err = tfa2_dev_start(tfa9xxx->tfa, next_profile, vstep);

	if (trace_level & 8) {
		stop_time = ktime_get_boottime();
		delta_time = ktime_to_ns(ktime_sub(stop_time, start_time));
		do_div(delta_time, 1000);
		dev_dbg(&tfa9xxx->i2c->dev, "tfa2_dev_start(%d,%d) time = %lld us\n",
			next_profile, vstep, delta_time);
	}

	/* Remove sticky bit by reading it once */
	tfa2_get_noclk(tfa9xxx->tfa);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
#if defined(TFA_ENABLE_INTERRUPT)
	tfa9xxx_interrupt_enable(tfa9xxx, true);
#endif

	return err;
}

#if defined(CONFIG_DEBUG_FS)
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa9xxx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa9xxx->dsp_lock);
	value = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa9xxx->dsp_lock);

	if (value < 0) {
		dev_err(&i2c->dev, "%s: Unable to read OTC: %d\n",
			__func__, value);
		return -EIO;
	}

	*val = value;
	dev_dbg(&i2c->dev, "%s: OTC : %d\n", __func__, value);

	return 0;
}

static int tfa9xxx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int err;

	if ((val != 0) && (val != 1)) {
		dev_err(&i2c->dev, "%s: Unexpected value %llu\n",
			__func__, val);
		return -EINVAL;
	}

	mutex_lock(&tfa9xxx->dsp_lock);
	err = tfa2_dev_mtp_set(tfa9xxx->tfa, TFA_MTP_OTC, val);
	mutex_unlock(&tfa9xxx->dsp_lock);

	if (err < 0) {
		dev_err(&i2c->dev, "%s: Unable to write OTC: %d\n",
			__func__, err);
		return -EIO;
	}

	dev_dbg(&i2c->dev, "%s: OTC < %llu\n", __func__, val);

	return 0;
}

static int tfa9xxx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int value;

	mutex_lock(&tfa9xxx->dsp_lock);
	value = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa9xxx->dsp_lock);

	if (value < 0) {
		dev_err(&i2c->dev, "%s: Unable to read EX: %d\n",
			__func__, value);
		return -EIO;
	}

	*val = value;
	dev_dbg(&i2c->dev, "%s: MTPEX : %d\n", __func__, value);

	return 0;
}

static int tfa9xxx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	uint16_t value;
	int err;

	if ((val != 0) && (val != 1)) {
		dev_err(&i2c->dev, "%s: Unexpected value %llu\n",
			__func__, val);
		return -EINVAL;
	}

	value = (uint16_t)val;

	mutex_lock(&tfa9xxx->dsp_lock);
	err = tfa2_dev_mtp_set(tfa9xxx->tfa, TFA_MTP_EX, value);
	mutex_unlock(&tfa9xxx->dsp_lock);

	if (err < 0) {
		dev_err(&tfa9xxx->i2c->dev, "%s: Unable to write EX: %d\n",
			__func__, err);
		return -EIO;
	}

	dev_dbg(&i2c->dev, "%s: MTPEX < %d\n", __func__, value);

	return 0;
}

static int tfa9xxx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa9xxx->dsp_lock);
	*val = tfa2_get_exttemp(tfa9xxx->tfa);
	mutex_unlock(&tfa9xxx->dsp_lock);

	dev_dbg(&i2c->dev, "%s: TEMP : %llu\n", __func__, *val);

	return 0;
}

static int tfa9xxx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);

	mutex_lock(&tfa9xxx->dsp_lock);
	tfa2_set_exttemp(tfa9xxx->tfa, (short)val);
	mutex_unlock(&tfa9xxx->dsp_lock);

	dev_dbg(&i2c->dev, "%s: TEMP < %llu\n", __func__, val);

	return 0;
}

static ssize_t tfa9xxx_dbgfs_start_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int ret;
	char buf[32];
	const char ref_now[] = "please calibrate now"; /* SB + HB */
	const char ref_sb[] = "please calibrate SB";
	const char ref_hb[] = "please calibrate HB";
	int buf_size;
	bool sb_cal = false, hb_cal = false;
	int result = count;
	int temp_val = 25;

	/* check string length, and account for eol */
	if (count > sizeof(ref_now) + 1 || count < (sizeof(ref_sb) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (!strncmp(buf, ref_sb, sizeof(ref_sb) - 1))
		sb_cal = true;
	else if (!strncmp(buf, ref_hb, sizeof(ref_hb) - 1))
		hb_cal = tfa9xxx->haptic_mode;
	else if (!strncmp(buf, ref_now, sizeof(ref_now) - 1)) {
		sb_cal = true;
		hb_cal = tfa9xxx->haptic_mode;
	}

	if (!sb_cal && !hb_cal)
		return -EINVAL;

	/* EXT_TEMP */
#if defined(TFA_READ_BATTERY_TEMP)
	ret = tfa9xxx_read_battery_temp(&temp_val);
	if (ret)
		pr_err("%s: error in reading battery temp\n", __func__);
#endif
	tfa2_set_exttemp(tfa9xxx->tfa, (short)temp_val);

	if (sb_cal) {
		mutex_lock(&tfa9xxx->dsp_lock);
		ret = tfa2_sb_calibrate(tfa9xxx->tfa);
		mutex_unlock(&tfa9xxx->dsp_lock);

		if (ret) {
			dev_err(&tfa9xxx->i2c->dev,
				"%s: SB Calibration failed (%d)\n",
				__func__, ret);
			result = -EIO;
		} else {
			dev_info(&tfa9xxx->i2c->dev,
				"%s: SB Calibration done\n", __func__);
		}
	}

	if (hb_cal) {
		mutex_lock(&tfa9xxx->dsp_lock);
		ret = tfa2_hap_calibrate(tfa9xxx->tfa);
		mutex_unlock(&tfa9xxx->dsp_lock);

		if (ret) {
			dev_err(&tfa9xxx->i2c->dev,
				"%s: HB Calibration failed (%d)\n",
				__func__,  ret);
			result = -EIO;
		} else {
			dev_info(&tfa9xxx->i2c->dev,
				"%s: HB Calibration done\n", __func__);
		}
	}

	return result;
}

static int tfa9xxx_dbgfs_r_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int mtpex, otc, value;

	mutex_lock(&tfa9xxx->dsp_lock);
	mtpex = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_EX);
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (mtpex < 0) {
		dev_err(&i2c->dev, "%s: Unable to read EX: %d\n",
			__func__, mtpex);
		return -EIO;
	}

	mutex_lock(&tfa9xxx->dsp_lock);
	otc = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_OTC);
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (otc < 0) {
		dev_err(&i2c->dev, "%s: Unable to read OTC %d\n",
			__func__, otc);
		return -EIO;
	}

	mutex_lock(&tfa9xxx->dsp_lock);
	if ((mtpex > 0) && (otc > 0)) {
		value = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_R25C);
	} else {
		value = tfa2_get_calibration_impedance(tfa9xxx->tfa);

	}
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (value < 0) {
		dev_err(&i2c->dev, "%s: Unable to read R: %d\n",
			__func__, value);
		return -EIO;
	}

	*val = value;

	dev_dbg(&i2c->dev, "%s: R : %llu\n", __func__, *val);

	return 0;
}

static int tfa9xxx_dbgfs_r_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	uint16_t value = (uint16_t)val;

	dev_dbg(&i2c->dev, "%s: R : %d\n", __func__, value);

	mutex_lock(&tfa9xxx->dsp_lock);
	value = tfa2_dev_mtp_set(tfa9xxx->tfa, TFA_MTP_R25C, value);
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (value < 0) {
		dev_err(&i2c->dev, "%s: Unable to write R: %d\n",
			__func__, value);
		return -EIO;
	}

	return 0;
}

static int tfa9xxx_dbgfs_f0_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int mtp_f0;

	mutex_lock(&tfa9xxx->dsp_lock);
	mtp_f0 = tfa2_dev_mtp_get(tfa9xxx->tfa, TFA_MTP_F0);
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (mtp_f0 < 0) {
		dev_err(&i2c->dev, "%s: Unable to read F0: %d\n",
			__func__, mtp_f0);
		return -EIO;
	}

	*val = mtp_f0 / 2 + 80;

	dev_dbg(&i2c->dev, "%s: F0 : %llu\n", __func__, *val);

	return 0;
}

static int tfa9xxx_dbgfs_f0_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int value = (int)val;
	uint16_t mtp_f0 =  2 * (value - 80);

	dev_dbg(&i2c->dev, "%s: F0 : %d, MTP_F0: %d\n", __func__,
		value, mtp_f0);

	mutex_lock(&tfa9xxx->dsp_lock);
	mtp_f0 = tfa2_dev_mtp_set(tfa9xxx->tfa, TFA_MTP_F0, mtp_f0);
	mutex_unlock(&tfa9xxx->dsp_lock);
	if (mtp_f0 < 0) {
		dev_err(&i2c->dev, "%s: Unable to write F0: %d\n",
			__func__, mtp_f0);
		return -EIO;
	}

	return 0;
}

static ssize_t tfa9xxx_dbgfs_version_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char str[] = TFA9XXX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa9xxx_dbgfs_dsp_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa9xxx->dsp_init) {
	case TFA9XXX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA9XXX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA9XXX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA9XXX_DSP_INIT_PENDING:
		str =  "Pending init\n";
		break;
	case TFA9XXX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
	}

	dev_dbg(&tfa9xxx->i2c->dev, "%s: dsp_state : %s\n", __func__, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

	return ret;
}

static ssize_t tfa9xxx_dbgfs_fw_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa9xxx->dsp_fw_state) {
	case TFA9XXX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA9XXX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA9XXX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA9XXX_DSP_FW_OK:
		str =  "Ok\n";
		break;
	default:
		str = "Invalid\n";
	}

	dev_dbg(&tfa9xxx->i2c->dev, "%s: fw_state : %s", __func__, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

	return ret;
}

static int tfa9xxx_haptic_objs_open(struct inode *inode, struct file *file)
{
	struct i2c_client *i2c = (struct i2c_client *)inode->i_private;
	struct tfa9xxx *drv = i2c_get_clientdata(i2c);

	return single_open(file, tfa9xxx_haptic_dump_objs, drv);
}

static ssize_t tfa9xxx_dbgfs_rpc_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);
	struct tfa2_device *tfa = tfa9xxx->tfa;
	int ready = 0, ret = 0, err = 0;
	uint8_t *buffer;

	if (tfa == NULL) {
		dev_dbg(&i2c->dev, "%s: dsp is not available\n", __func__);
		return -ENODEV;
	}

	if (tfa9xxx->rpc_buffer_size <= 0) {
		dev_dbg(&i2c->dev, "%s: rpc cmd is not available\n", __func__);
		return -EINVAL;
	}

	if (count == 0)
		return 0;

	if (count > TFA2_MAX_PARAM_SIZE)
		count = TFA2_MAX_PARAM_SIZE;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		dev_err(&i2c->dev, "%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&tfa9xxx->dsp_lock);
	tfa2_dev_dsp_system_stable(tfa, &ready);
	if (ready)
		err = tfa2_dsp_execute(tfa, tfa9xxx->rpc_buffer,
			tfa9xxx->rpc_buffer_size, buffer, count);
	mutex_unlock(&tfa9xxx->dsp_lock);

	if (!ready) {
		dev_dbg(&i2c->dev, "%s: no clock\n", __func__);
		err = -EIO;
		goto error_out;
	}
	if (err < 0) {
		dev_err(&i2c->dev, "%s: dsp_msg_read error: %d\n", __func__, err);
		goto error_out;
	}

	ret = copy_to_user(user_buf, buffer, count);
	err = ret ? -EFAULT : 0;

error_out:
	kfree(buffer);

	if (err)
		return err;

	*ppos += count;

	return count;
}

static ssize_t tfa9xxx_dbgfs_rpc_send(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);

	if (tfa9xxx->tfa == NULL) {
		dev_err(&i2c->dev, "%s: dsp is not available\n", __func__);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	if (count > TFA2_MAX_PARAM_SIZE)
		count = TFA2_MAX_PARAM_SIZE;

	if (copy_from_user(tfa9xxx->rpc_buffer, user_buf, count)) {
		tfa9xxx->rpc_buffer_size = 0;
		return -EFAULT;
	}

	tfa9xxx->rpc_buffer_size = count;

	return count;
}
/* -- RPC */

DEFINE_SIMPLE_ATTRIBUTE(tfa9xxx_dbgfs_calib_otc_fops,
	tfa9xxx_dbgfs_otc_get, tfa9xxx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa9xxx_dbgfs_calib_mtpex_fops,
	tfa9xxx_dbgfs_mtpex_get, tfa9xxx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa9xxx_dbgfs_calib_temp_fops,
	tfa9xxx_dbgfs_temp_get, tfa9xxx_dbgfs_temp_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa9xxx_dbgfs_r_fops,
	tfa9xxx_dbgfs_r_get, tfa9xxx_dbgfs_r_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa9xxx_dbgfs_f0_fops,
	tfa9xxx_dbgfs_f0_get, tfa9xxx_dbgfs_f0_set, "%llu\n");

static const struct file_operations tfa9xxx_dbgfs_calib_start_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.write  = tfa9xxx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa9xxx_dbgfs_version_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = tfa9xxx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa9xxx_dbgfs_dsp_state_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = tfa9xxx_dbgfs_dsp_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa9xxx_dbgfs_fw_state_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = tfa9xxx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa9xxx_dbgfs_haptic_objs_fops = {
	.owner   = THIS_MODULE,
	.open    = tfa9xxx_haptic_objs_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static const struct file_operations tfa9xxx_dbgfs_rpc_fops = {
	.owner  = THIS_MODULE,
	.open   = simple_open,
	.read   = tfa9xxx_dbgfs_rpc_read,
	.write  = tfa9xxx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static void tfa9xxx_debugfs_init(struct tfa9xxx *tfa9xxx,
	struct i2c_client *i2c)
{
	char name[MAX_CONTROL_NAME];

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa9xxx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("version", 0444, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_fw_state_fops);
	debugfs_create_file("calibrate", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_calib_start_fops);
	debugfs_create_file("R", 0444, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_r_fops);
	debugfs_create_file("TEMP", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_calib_temp_fops);

	tfa9xxx->rpc_buffer = devm_kzalloc(&i2c->dev,
		TFA2_MAX_PARAM_SIZE, GFP_KERNEL);
	tfa9xxx->rpc_buffer_size = 0; /* no data yet */

	debugfs_create_file("rpc", 0664, tfa9xxx->dbg_dir,
		i2c, &tfa9xxx_dbgfs_rpc_fops);

	if (tfa9xxx->haptic_mode) {
		debugfs_create_file("F0", 0444, tfa9xxx->dbg_dir, i2c,
			&tfa9xxx_dbgfs_f0_fops);
		debugfs_create_file("haptic-objs", 0444, tfa9xxx->dbg_dir, i2c,
			&tfa9xxx_dbgfs_haptic_objs_fops);
	}
}

static void tfa9xxx_debug_remove(struct tfa9xxx *tfa9xxx)
{
	devm_kfree(&tfa9xxx->i2c->dev, tfa9xxx->rpc_buffer);

	if (tfa9xxx->dbg_dir)
		debugfs_remove_recursive(tfa9xxx->dbg_dir);
}
#endif /* CONFIG_DEBUG_FS */

/* copies the profile basename (i.e. part until .) into buf */
static void get_profile_basename(char *buf, char *profile)
{
	int cp_len = 0, idx = 0;
	char *pch;

	pch = strnchr(profile, '.', strlen(profile));
	idx = pch - profile;
	cp_len = (pch != NULL) ? idx : (int) strlen(profile);
	memcpy(buf, profile, cp_len);
	buf[cp_len] = 0;
}

static int get_profile_sr(struct tfa9xxx *tfa9xxx, unsigned int prof_idx)
{
	unsigned long value;
	int rate = 48000; /* default */
	int ret;
	char *pch;

	pch = strrchr(tfa_cnt_profile_name(tfa9xxx, prof_idx), '.');
	if (pch) {
		ret = kstrtoul(&pch[1], 10, &value);
		if (ret == 0)
			rate = (int)value;
	}

	dev_dbg(&tfa9xxx->i2c->dev, "%s: prof_idx = %d, rate = %d\n",
		__func__, prof_idx, rate);

	return rate;
}

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa9xxx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (bprof->item_id == id) {
			strlcpy(buf, bprof->basename, MAX_CONTROL_NAME);
			return 0;
		}
	}

	return TFA_ERROR;
}

/* search for the profile in the profile list */
static int is_profile_in_list(char *profile, int len)
{
	struct tfa9xxx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, profile, len) == 0))
			return 1;
	}

	return 0;
}

/*
 * for the profile with id, look if the requested samplerate is
 * supported, if found return the (container)profile for this
 * samplerate, on error or if not found return -1
 */
static int get_profile_id_for_sr(int id, unsigned int rate)
{
	int idx = 0;
	struct tfa9xxx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa9xxx_get_fssel(rate);
			if (idx < 0) {
				/* samplerate not supported */
				return TFA_ERROR;
			}

			return bprof->sr_rate_sup[idx];
		}
	}

	/* profile not found */
	return TFA_ERROR;
}

/* check if this profile is a calibration profile */
static bool is_calibration_profile(char *profile)
{
	if (strnstr(profile, ".cal",
		strlen(profile)) != NULL)
		return true;
	return false;
}

/* check if this profile is a haptic profile */
static bool is_haptic_profile(char *profile)
{
	if (strnstr(profile, ".hap",
		strlen(profile)) != NULL)
		return true;
	return false;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa9xxx *tfa9xxx, char *basename, int len, int profile)
{
	struct tfa9xxx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;

	list_for_each_entry(bprof, &profile_list, list) {
		if ((len == bprof->len)
			&& (strncmp(bprof->basename, basename, len) == 0)) {
			/* add supported samplerate for this profile */
			sr = get_profile_sr(tfa9xxx, profile);
			if (!sr) {
				dev_err(&tfa9xxx->i2c->dev,
					"%s: unable to identify supported sample rate for %s\n",
					__func__, bprof->basename);
				return TFA_ERROR;
			}
#if defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
			if (sr_converted != sr) {
				pr_info("sr_converted: %d to %d\n",
					sr_converted, sr);
				sr_converted = sr;
			}
#endif

			/* get the index for this samplerate */
			idx = tfa9xxx_get_fssel(sr);
			if (idx < 0 || idx >= TFA9XXX_NUM_RATES) {
				dev_err(&tfa9xxx->i2c->dev,
					"%s: invalid index for samplerate %d\n",
					__func__, idx);
				return TFA_ERROR;
			}

			/* enter the (container) profile for this samplerate
			 * at the corresponding index
			 */
			bprof->sr_rate_sup[idx] = profile;

			dev_dbg(&tfa9xxx->i2c->dev,
				"%s: added profile:samplerate = [%d:%d] for mixer profile: %s\n",
				__func__, profile, sr, bprof->basename);
		}
	}

	return 0;
}

#if KERNEL_VERSION(3,16,0) > LINUX_VERSION_CODE
static struct snd_soc_codec *
	snd_soc_kcontrol_codec(struct snd_kcontrol *kcontrol)
{
	return snd_kcontrol_chip(kcontrol);
}
#endif

#if defined(VOLUME_FIXED)
static int tfa9xxx_get_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
	int mixer_profile = kcontrol->private_value;
	int ret = 0;
	int profile;

	profile = get_profile_id_for_sr(mixer_profile, tfa9xxx->rate);
	if (profile < 0) {
		dev_err(&tfa9xxx->i2c->dev,
			"%s: tfa9xxx_get_vstep: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa9xxx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa9xxx_mutex);
	list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
		int vstep = tfa9xxx->prof_vsteps[profile];

		ucontrol->value.integer.value[tfa9xxx->tfa->dev_idx] =
			tfa_cont_get_max_vstep(tfa9xxx->tfa, profile)
			- vstep - 1; /* TODO */
	}
	mutex_unlock(&tfa9xxx_mutex);

	return ret;
}

static int tfa9xxx_set_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
	int mixer_profile = kcontrol->private_value;
	int profile;
	int err = 0;
	int change = 0;

	if (no_start != 0)
		return 0;

	profile = get_profile_id_for_sr(mixer_profile, tfa9xxx->rate);
	if (profile < 0) {
		dev_err(&tfa9xxx->i2c->dev,
			"%s: tfa9xxx_set_vstep: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa9xxx->rate);
		return -EINVAL;
	}

	mutex_lock(&tfa9xxx_mutex);
	list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
		int vstep, vsteps;
		int ready = 0;
		int new_vstep;
		int value = ucontrol->value.integer.value[tfa9xxx->tfa->dev_idx];

		vstep = tfa9xxx->prof_vsteps[profile];
		vsteps = tfa_cont_get_max_vstep(tfa9xxx->tfa, profile); /* TODO */

		if (vstep == vsteps - value - 1)
			continue;

		new_vstep = vsteps - value - 1;

		if (new_vstep < 0)
			new_vstep = 0;

		tfa9xxx->prof_vsteps[profile] = new_vstep;

		if (profile == tfa9xxx->profile) {
			/* this is the active profile, program the new vstep */
			tfa9xxx->vstep = new_vstep;
			mutex_lock(&tfa9xxx->dsp_lock);
			tfa2_dev_dsp_system_stable(tfa9xxx->tfa, &ready);

			if (ready) {
				err = tfa9xxx_tfa_start(tfa9xxx,
					tfa9xxx->profile, tfa9xxx->vstep);
				if (err) {
					dev_err(&tfa9xxx->i2c->dev,
						"%s: Write vstep error: %d\n", __func__, err);
				} else {
					dev_dbg(&tfa9xxx->i2c->dev,
						"%s: Succesfully changed vstep index!\n", __func__);
					change = 1;
				}
			}

			mutex_unlock(&tfa9xxx->dsp_lock);
		}

		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: %d: vstep:%d, (control value: %d) - profile %d\n",
			__func__, tfa9xxx->tfa->dev_idx, new_vstep, value, profile);
	}

	if (change) {
		list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
			mutex_lock(&tfa9xxx->dsp_lock);
			tfa2_dev_set_state(tfa9xxx->tfa, TFA_STATE_UNMUTE);
			mutex_unlock(&tfa9xxx->dsp_lock);
		}
	}

	mutex_unlock(&tfa9xxx_mutex);

	return change;
}

static int tfa9xxx_info_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);

	int mixer_profile = tfa9xxx_mixer_profile;
	int profile = get_profile_id_for_sr(mixer_profile, tfa9xxx->rate);
	if (profile < 0) {
		dev_err(&tfa9xxx->i2c->dev,
			"%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa9xxx->rate);
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	mutex_lock(&tfa9xxx_mutex);
	uinfo->count = tfa9xxx_device_count;
	mutex_unlock(&tfa9xxx_mutex);
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max(0,
		tfa_cont_get_max_vstep(tfa9xxx->tfa, profile) - 1); /* TODO */
	dev_dbg(&tfa9xxx->i2c->dev, "vsteps count: %d [prof=%d]\n",
		__func__, tfa_cont_get_max_vstep(tfa9xxx->tfa, profile),
		profile); /* TODO */
	return 0;
}
#endif /* VOLUME_FIXED */

static int tfa9xxx_get_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	mutex_lock(&tfa9xxx_mutex);
	ucontrol->value.integer.value[0] = tfa9xxx_mixer_profile;
	mutex_unlock(&tfa9xxx_mutex);

	return 0;
}

static int tfa9xxx_set_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
	int change = 0;
	int new_profile;
	int prof_idx;
	int profile_count = tfa9xxx_mixer_profiles;
	int profile = tfa9xxx_mixer_profile;

	if (no_start != 0)
		return 0;

	new_profile = ucontrol->value.integer.value[0];
	if (new_profile == profile)
		return 0;

	if ((new_profile < 0) || (new_profile >= profile_count)) {
		dev_err(&tfa9xxx->i2c->dev, "%s:not existing profile (%d)\n",
			__func__, new_profile);
		return -EINVAL;
	}

	/* get the container profile for the requested sample rate */
	prof_idx = get_profile_id_for_sr(new_profile, tfa9xxx->rate);
	if (prof_idx < 0) {
		dev_err(&tfa9xxx->i2c->dev,
			"%s: sample rate [%d] not supported for this mixer profile [%d].\n",
			__func__, tfa9xxx->rate, new_profile);
		return 0;
	}
	dev_dbg(&tfa9xxx->i2c->dev,
		"%s: selected container profile [%d]\n",
		__func__, prof_idx);

	/* update mixer profile */
	tfa9xxx_mixer_profile = new_profile;

	mutex_lock(&tfa9xxx_mutex);
	list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
		int err;
		int ready = 0;

		/* update 'real' profile (container profile) */
		tfa9xxx->profile = prof_idx;
#if defined(VOLUME_FIXED)
		tfa9xxx->vstep = tfa9xxx->prof_vsteps[prof_idx];
#endif
		/* Don't call tfa2_dev_start() if there is no clock. */
		mutex_lock(&tfa9xxx->dsp_lock);
		tfa2_dev_dsp_system_stable(tfa9xxx->tfa, &ready);
		if (ready) {
			/* Also re-enables the interrupts */
			err = tfa9xxx_tfa_start(tfa9xxx, prof_idx, tfa9xxx->vstep);
			if (err) {
				dev_info(&tfa9xxx->i2c->dev,
					"%s: Write profile error: %d\n", __func__, err);
			} else {
				dev_dbg(&tfa9xxx->i2c->dev,
					"%s: Changed to profile %d (vstep = %d)\n",
					__func__, prof_idx, tfa9xxx->vstep);
				change = 1;
			}
		}
		mutex_unlock(&tfa9xxx->dsp_lock);

		/* Flag DSP as invalidated as the profile change may invalidate the
		 * current DSP configuration. That way, further stream start can
		 * trigger a tfa2_dev_start.
		 */
		tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_INVALIDATED;
	}

	if (change) {
		list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
			mutex_lock(&tfa9xxx->dsp_lock);
			tfa2_dev_set_state(tfa9xxx->tfa, TFA_STATE_UNMUTE);
			mutex_unlock(&tfa9xxx->dsp_lock);
		}
	}

	mutex_unlock(&tfa9xxx_mutex);

	return change;
}

static int tfa9xxx_info_profile(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	char profile_name[MAX_CONTROL_NAME] = {0};
	int count = tfa9xxx_mixer_profiles, err = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	err = get_profile_from_list(profile_name, uinfo->value.enumerated.item);
	if (err != 0)
		return -EINVAL;

	strlcpy(uinfo->value.enumerated.name,
		profile_name, MAX_CONTROL_NAME);

	return 0;
}

#if defined(TFA_READ_BATTERY_TEMP)
static int tfa9xxx_read_battery_temp(int *value)
{
	struct power_supply *psy;
	union power_supply_propval prop_read = {0};
	int ret = 0;

	/* get power supply of "battery" */
	/* value is preserved with default when error happens */
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: failed to get power supply\n", __func__);
		return -EIO;
	}
#if KERNEL_VERSION(4, 1, 0) > LINUX_VERSION_CODE
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &prop_read);
#else
	if (!psy->desc) {
		pr_err("%s: failed to get desc of power supply\n", __func__);
		return -EIO;
	}

	ret = psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP, &prop_read);
#endif
	if (!ret) {
		*value = (int)(prop_read.intval / 10); /* in degC */
		pr_info("%s: read battery temp (%d)\n", __func__, *value);
	} else {
		pr_err("%s: failed to get temp property\n", __func__);
		return -EIO;
	}

	return 0;
}
#endif /* TFA_READ_BATTERY_TEMP */

static int tfa9xxx_create_controls(struct tfa9xxx *tfa9xxx)
{
	int prof, nprof;
	int mix_index = 0;
	int  nr_controls = 0, id = 0;
	char *name;
	struct tfa9xxx_baseprofile *bprofile;

	/* Create the following controls:
	 *  - enum control to select the active profile
	 *  - one volume control for each profile hosting a vstep
	 */

	nr_controls = 1; /* Profile control */

	/* allocate the tfa9xxx_controls base on the nr of profiles */
	nprof = tfa2_dev_get_dev_nprof(tfa9xxx->tfa);
#if defined(VOLUME_FIXED)
	for (prof = 0; prof < nprof; prof++) {
		if (tfa_cont_get_max_vstep(tfa9xxx->tfa, prof)) /* TODO */
			nr_controls++; /* Playback Volume control */
	}
#endif

	tfa9xxx_controls = devm_kzalloc(tfa9xxx->codec->dev,
			nr_controls * sizeof(tfa9xxx_controls[0]), GFP_KERNEL);
	if (!tfa9xxx_controls)
		return -ENOMEM;

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(tfa9xxx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	scnprintf(name, MAX_CONTROL_NAME, "%s Profile", tfa9xxx->fw.name);
	tfa9xxx_controls[mix_index].name = name;
	tfa9xxx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa9xxx_controls[mix_index].info = tfa9xxx_info_profile;
	tfa9xxx_controls[mix_index].get = tfa9xxx_get_profile;
	tfa9xxx_controls[mix_index].put = tfa9xxx_set_profile;
	/* save number of profiles */
	/* tfa9xxx_controls[mix_index].private_value = profs; */
	mix_index++;

	/* create mixer items for each profile that has volume */
	for (prof = 0; prof < nprof; prof++) {
		/* create an new empty profile */
		bprofile = devm_kzalloc(tfa9xxx->codec->dev,
			sizeof(*bprofile), GFP_KERNEL);
		if (!bprofile)
			return -ENOMEM;

		bprofile->len = 0;
		bprofile->item_id = -1;
		INIT_LIST_HEAD(&bprofile->list);

		/* copy profile name into basename until the . */
		get_profile_basename(bprofile->basename,
			tfa_cnt_profile_name(tfa9xxx, prof));
		bprofile->len = strlen(bprofile->basename);

		/*
		 * sarch the profile list for a profile with basename,
		 * if it is not found then
		 * add it to the list and add a new mixer control (if it has vsteps)
		 * also, if it is a calibration profile, do not add it to the list
		 */
		if ((is_profile_in_list(bprofile->basename, bprofile->len) == 0) &&
			!is_calibration_profile(tfa_cnt_profile_name(tfa9xxx, prof)) &&
			!is_haptic_profile(tfa_cnt_profile_name(tfa9xxx, prof))) {
			/* the profile is not present, add it to the list */
			list_add(&bprofile->list, &profile_list);
			bprofile->item_id = id++;

			dev_dbg(&tfa9xxx->i2c->dev, "%s: profile added [%d]: %s\n",
				__func__, bprofile->item_id, bprofile->basename);
#if defined(VOLUME_FIXED)
			if (tfa_cont_get_max_vstep(tfa9xxx->tfa, prof)) { /* TODO */
				name = devm_kzalloc(tfa9xxx->codec->dev,
					MAX_CONTROL_NAME, GFP_KERNEL);
				if (!name)
					return -ENOMEM;

				scnprintf(name, MAX_CONTROL_NAME, "%s %s Playback Volume",
				tfa9xxx->fw.name, bprofile->basename);

				tfa9xxx_controls[mix_index].name = name;
				tfa9xxx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
				tfa9xxx_controls[mix_index].info = tfa9xxx_info_vstep;
				tfa9xxx_controls[mix_index].get = tfa9xxx_get_vstep;
				tfa9xxx_controls[mix_index].put = tfa9xxx_set_vstep;
				tfa9xxx_controls[mix_index].private_value = bprofile->item_id;
				/* save profile index */
				mix_index++;
			}
#endif
		}

		/* look for the basename profile in the list of mixer profiles
		 * and add the container profile index to the supported samplerates
		 * of this mixer profile */
		add_sr_to_profile(tfa9xxx, bprofile->basename, bprofile->len, prof);
	}

	/* set the number of user selectable profiles in the mixer */
	tfa9xxx_mixer_profiles = id;

	return snd_soc_add_codec_controls(tfa9xxx->codec,
		tfa9xxx_controls, mix_index);
}

static void *tfa9xxx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);

	if (!str)
		return str;
	memcpy(str, buf, strlen(buf));

	return str;
}

static int tfa9xxx_append_i2c_address(struct device *dev,
	struct i2c_client *i2c,
	struct snd_soc_dapm_widget *widgets,
	int num_widgets,
	struct snd_soc_dai_driver *dai_drv,
	int num_dai)
{
	char buf[50];
	int i;
	int i2cbus = i2c->adapter->nr;
	int addr = i2c->addr;

	if (dai_drv && num_dai > 0)
		for (i = 0; i < num_dai; i++) {
			snprintf(buf, 50, "%s-%x-%x",dai_drv[i].name, i2cbus,
				addr);
			dai_drv[i].name = tfa9xxx_devm_kstrdup(dev, buf);

			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].playback.stream_name,
				i2cbus, addr);
			dai_drv[i].playback.stream_name = tfa9xxx_devm_kstrdup(dev, buf);

			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].capture.stream_name,
				i2cbus, addr);
			dai_drv[i].capture.stream_name = tfa9xxx_devm_kstrdup(dev, buf);
		}

	/* the idea behind this is convert:
	 * SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	 * into:
	 * SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback-2-36", 0, SND_SOC_NOPM, 0, 0),
	 */
	if (widgets && num_widgets > 0)
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;
			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				snprintf(buf, 50, "%s-%x-%x", widgets[i].sname,
					i2cbus, addr);
				widgets[i].sname = tfa9xxx_devm_kstrdup(dev, buf);
			}
		}

	return 0;
}

static struct snd_soc_dapm_widget tfa9xxx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static const struct snd_soc_dapm_route tfa9xxx_dapm_routes_common[] = {
	{"OUTL", NULL, "AIF IN"},
	{"AIF OUT", NULL, "AEC Loopback"},
};

#if KERNEL_VERSION(4,2,0) > LINUX_VERSION_CODE
static struct snd_soc_dapm_context *
	snd_soc_codec_get_dapm(struct snd_soc_codec *codec)
{
	return &codec->dapm;
}
#endif

static void tfa9xxx_add_widgets(struct tfa9xxx *tfa9xxx)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(tfa9xxx->codec);
	struct snd_soc_dapm_widget *widgets;
	unsigned int num_dapm_widgets = ARRAY_SIZE(tfa9xxx_dapm_widgets_common);

	widgets = devm_kzalloc(&tfa9xxx->i2c->dev,
		sizeof(struct snd_soc_dapm_widget) *
		ARRAY_SIZE(tfa9xxx_dapm_widgets_common),
		GFP_KERNEL);
	if (!widgets)
		return;

	memcpy(widgets, tfa9xxx_dapm_widgets_common,
		sizeof(struct snd_soc_dapm_widget) *
		ARRAY_SIZE(tfa9xxx_dapm_widgets_common));

	tfa9xxx_append_i2c_address(&tfa9xxx->i2c->dev,
		tfa9xxx->i2c,
		widgets,
		num_dapm_widgets,
		NULL,
		0);

	snd_soc_dapm_new_controls(dapm, widgets,
		ARRAY_SIZE(tfa9xxx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, tfa9xxx_dapm_routes_common,
		ARRAY_SIZE(tfa9xxx_dapm_routes_common));
}

#if defined(TFA_ENABLE_INTERRUPT)
/* Interrupts management */
static void tfa9xxx_interrupt_enable_tfa2(struct tfa9xxx *tfa9xxx, bool enable)
{
	/* Only for 0x72 we need to enable NOCLK interrupts */
	if (tfa9xxx->flags & TFA9XXX_FLAG_REMOVE_PLOP_NOISE)
		tfa_irq_ena(tfa9xxx->tfa, tfa9912_irq_stnoclk, enable); /* TODO */

	if (tfa9xxx->flags & TFA9XXX_FLAG_LP_MODES) {
		/* FIXME: IELP0 does not excist for 9912 */ /* TODO */
		tfa_irq_ena(tfa9xxx->tfa, 36, enable);
		tfa_irq_ena(tfa9xxx->tfa, tfa9912_irq_stclpr, enable); /* TODO */
	}
}

/* global enable / disable interrupts */
static void tfa9xxx_interrupt_enable(struct tfa9xxx *tfa9xxx, bool enable)
{
	if (tfa9xxx->flags & TFA9XXX_FLAG_SKIP_INTERRUPTS)
		return;

	tfa9xxx_interrupt_enable_tfa2(tfa9xxx, enable);
}
#endif /* TFA_ENABLE_INTERRUPT */

/* Firmware management */
static void tfa9xxx_container_loaded(const struct firmware *cont,
	void *context)
{
	struct tfa_container *container;
	struct tfa9xxx *tfa9xxx = context;
	int err;
	int container_size;

	mutex_lock(&probe_lock);

	tfa9xxx->dsp_fw_state = TFA9XXX_DSP_FW_FAIL;

	if (!cont) {
		dev_err(&tfa9xxx->i2c->dev, "Failed to read %s\n", fw_name);
		mutex_unlock(&probe_lock);
		return;
	}

	dev_err(&tfa9xxx->i2c->dev, "loaded %s - size: %zu\n",
		fw_name, cont->size);

	mutex_lock(&tfa9xxx_mutex);
	if (tfa9xxx_container == NULL) {
		container = kzalloc(cont->size, GFP_KERNEL);
		if (container == NULL) {
			mutex_unlock(&tfa9xxx_mutex);
			release_firmware(cont);
			dev_err(&tfa9xxx->i2c->dev, "%s: Error allocating memory\n",
				__func__);
			mutex_unlock(&probe_lock);
			return;
		}

		container_size = cont->size;
		memcpy(container, cont->data, container_size);
		release_firmware(cont);

		dev_dbg(&tfa9xxx->i2c->dev, "%.2s%.2s\n",
			container->version, container->subversion);
		dev_dbg(&tfa9xxx->i2c->dev, "%.8s\n", container->customer);
		dev_dbg(&tfa9xxx->i2c->dev, "%.8s\n", container->application);
		dev_dbg(&tfa9xxx->i2c->dev, "%.8s\n", container->type);
		dev_dbg(&tfa9xxx->i2c->dev, "%d ndev\n", container->ndev);
		dev_dbg(&tfa9xxx->i2c->dev, "%d nprof\n", container->nprof);

		err = tfa2_load_cnt(container, container_size);
		if (err < 0) {
			mutex_unlock(&tfa9xxx_mutex);
			kfree(container);
			dev_err(tfa9xxx->dev, "Cannot load container file, aborting\n");
			mutex_unlock(&probe_lock);
			return;
		}

		tfa9xxx_container = container;
	} else {
		dev_dbg(&tfa9xxx->i2c->dev, "container file already loaded...\n");
		container = tfa9xxx_container;
		release_firmware(cont);
	}
	mutex_unlock(&tfa9xxx_mutex);

	tfa9xxx->tfa->cnt = container;

	/* i2c */
	tfa9xxx->tfa->i2c = tfa9xxx->i2c;

	/*
		i2c transaction limited to 64k
		(Documentation/i2c/writing-clients)
	*/
	tfa9xxx->tfa->buffer_size = 65536;

	if (tfa2_dev_probe(tfa9xxx->tfa) < 0) {
		dev_err(tfa9xxx->dev, "Failed to probe TFA9xxx @ 0x%.2x\n",
			tfa9xxx->i2c->addr);
		mutex_unlock(&probe_lock);
		return;
	}

	tfa9xxx->tfa->dev_idx = tfa2_cnt_get_idx(tfa9xxx->tfa);
	if (tfa9xxx->tfa->dev_idx < 0) {
		dev_err(tfa9xxx->dev, "Failed to find TFA9xxx @ 0x%.2x in container file\n",
			tfa9xxx->i2c->addr);
		mutex_unlock(&probe_lock);
		return;
	}

	/* enable object playback during haptic recalculation */
	tfa9xxx->tfa->hap_data.recalc_play_object = haptic_recalc_play_object;

	/* Enable debug traces */
	tfa9xxx->tfa->verbose = trace_level & 1;

	/* prefix is the application name from the cnt */
	tfa2_cnt_get_app_name(tfa9xxx->tfa, tfa9xxx->fw.name);

	/* set default profile/vstep */
	tfa9xxx->profile = 0;
	tfa9xxx->vstep = 0;

	/* Override default profile if requested */
	if (strncmp(dflt_prof_name, "", strlen(dflt_prof_name))) {
		unsigned int i;
		int nprof = tfa2_dev_get_dev_nprof(tfa9xxx->tfa);
		for (i = 0; i < nprof; i++) {
			if (strncmp(tfa_cnt_profile_name(tfa9xxx, i),
				dflt_prof_name,
				strlen(tfa_cnt_profile_name(tfa9xxx, i))) == 0) {
				tfa9xxx->profile = i;
				dev_info(tfa9xxx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa9xxx->profile);
				break;
			}
		}
		if (i >= nprof)
			dev_info(tfa9xxx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa9xxx->dsp_fw_state = TFA9XXX_DSP_FW_OK;
	dev_err(&tfa9xxx->i2c->dev, "Firmware init complete\n");

	if (no_start != 0) {
		mutex_unlock(&probe_lock);
		return;
	}

	/* Only controls for master device */
	if (tfa9xxx->tfa->dev_idx == 0)
		tfa9xxx_create_controls(tfa9xxx);

	/* force ISTVDDS to be set */
	tfa2_dev_force_cold(tfa9xxx->tfa);

	/* Preload settings using internal clock on TFA2 */
	mutex_lock(&tfa9xxx->dsp_lock);
	err = tfa9xxx_tfa_start(tfa9xxx, tfa9xxx->profile, tfa9xxx->vstep);
	if ((err < 0) && (err != -EPERM)) {
		dev_err(&tfa9xxx->i2c->dev,
			"%s: Error loading settings on internal clock (err = %d)\n",
			__func__, err);
		tfa9xxx->dsp_fw_state = TFA9XXX_DSP_FW_FAIL;
	} else {
		tfa9xxx->patch_loaded = tfa9xxx->haptic_mode;
	}
	tfa2_dev_stop(tfa9xxx->tfa);
	mutex_unlock(&tfa9xxx->dsp_lock);

#if defined(TFA_ENABLE_INTERRUPT)
	tfa9xxx_interrupt_enable(tfa9xxx, true);
#endif

	mutex_unlock(&probe_lock);
}

static int tfa9xxx_load_container(struct tfa9xxx *tfa9xxx)
{
	tfa9xxx->dsp_fw_state = TFA9XXX_DSP_FW_PENDING;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		fw_name, tfa9xxx->dev, GFP_KERNEL,
		tfa9xxx, tfa9xxx_container_loaded);
}

static void tfa9xxx_monitor(struct work_struct *work)
{
	struct tfa9xxx *tfa9xxx;
	int err;

	tfa9xxx = container_of(work, struct tfa9xxx, monitor_work.work);

	mutex_lock(&tfa9xxx->dsp_lock);
	err = tfa2_dev_status(tfa9xxx->tfa);
	mutex_unlock(&tfa9xxx->dsp_lock);

	/* reschedule */
	queue_delayed_work(tfa9xxx->tfa9xxx_wq, &tfa9xxx->monitor_work, 5*HZ);
}

static int tfa9xxx_dsp_init(struct tfa9xxx *tfa9xxx)
{
	int ret;
	bool do_sync;

	if (tfa9xxx->dsp_fw_state != TFA9XXX_DSP_FW_OK) {
		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: Skipping tfa2_dev_start (no FW: %d)\n",
			__func__, tfa9xxx->dsp_fw_state);
		return -EINVAL;
	}

	if (tfa9xxx->dsp_init == TFA9XXX_DSP_INIT_DONE) {
		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: Stream already started, skipping DSP power-on\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&tfa9xxx->dsp_lock);
#if defined(TFA_CONTROL_MCLK)
	if (tfa9xxx->clk_users == 0) {
		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: enable mclk\n", __func__);
#if defined(TFA_USE_GPIO_FOR_MCLK)
		if (gpio_is_valid(tfa9xxx->mclk_gpio)) {
			gpio_set_value_cansleep(tfa9xxx->mclk_gpio, 1);
#else
		if (tfa9xxx->mclk) {
			ret = clk_prepare_enable(tfa9xxx->mclk);
			exynos9610_set_xclkout0_13();
			if (ret < 0) {
				dev_warn(&tfa9xxx->i2c->dev,
					"%s: failed in enabling mclk\n",
					__func__);
				return ret;
			} else
				dev_dbg(&tfa9xxx->i2c->dev,
					"%s: Success enable mclk\n", __func__);
#endif
			msleep(MCLK_START_DELAY);
		}
	}
#endif /* TFA_CONTROL_MCLK */

	tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_PENDING;

	/* directly try to start DSP */
	ret = tfa9xxx_tfa_start(tfa9xxx, tfa9xxx->profile, tfa9xxx->vstep);
	if (ret < 0) {
		dev_err(&tfa9xxx->i2c->dev, "%s: Failed starting device (err=%d)\n",
			__func__, ret);
		tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_FAIL;
		mutex_unlock(&tfa9xxx->dsp_lock);
		return ret;
	} else {
		/* Subsystem ready, tfa init complete */
		tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_DONE;
		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: tfa2_dev_start success\n", __func__);
	}

	mutex_unlock(&tfa9xxx->dsp_lock);

	/* check if all devices have started */
	mutex_lock(&tfa9xxx_mutex);

	if (tfa9xxx_sync_count < tfa9xxx_device_count)
		tfa9xxx_sync_count++;

	do_sync = (tfa9xxx_sync_count >= tfa9xxx_device_count);
	mutex_unlock(&tfa9xxx_mutex);

	/* when all devices have started then unmute */
	if (do_sync) {
		tfa9xxx_sync_count = 0;
		list_for_each_entry(tfa9xxx, &tfa9xxx_device_list, list) {
			mutex_lock(&tfa9xxx->dsp_lock);
			tfa2_dev_set_state(tfa9xxx->tfa, TFA_STATE_UNMUTE);

			/*
			 * start monitor thread to check IC status bit
			 * periodically, and re-init IC to recover if
			 * needed.
			 */
			if (tfa9xxx->enable_monitor)
				queue_delayed_work(tfa9xxx->tfa9xxx_wq,
					&tfa9xxx->monitor_work, 1*HZ);
			mutex_unlock(&tfa9xxx->dsp_lock);
		}

	}

	return 0;
}

#if defined(TFA_ENABLE_INTERRUPT)
static void tfa9xxx_interrupt(struct work_struct *work)
{
	struct tfa9xxx *tfa9xxx = container_of(work, struct tfa9xxx,
		interrupt_work.work);

	dev_info(&tfa9xxx->i2c->dev, "%s\n", __func__);

	if (tfa9xxx->flags & TFA9XXX_FLAG_REMOVE_PLOP_NOISE) {
		int start_triggered;

		mutex_lock(&tfa9xxx->dsp_lock);
		start_triggered = tfa_plop_noise_interrupt(tfa9xxx->tfa,
			tfa9xxx->profile, tfa9xxx->vstep); /* TODO */
		/* Only enable when the return value is 1,
		 * otherwise the interrupt is triggered twice
		 */
		if (start_triggered)
			tfa9xxx_interrupt_enable(tfa9xxx, true);
		mutex_unlock(&tfa9xxx->dsp_lock);
	} /* TFA9XXX_FLAG_REMOVE_PLOP_NOISE */

	if (tfa9xxx->flags & TFA9XXX_FLAG_LP_MODES) {
		tfa_lp_mode_interrupt(tfa9xxx->tfa); /* TODO */
	} /* TFA9XXX_FLAG_LP_MODES */

	/* unmask interrupts masked in IRQ handler */
	 tfa_irq_unmask(tfa9xxx->tfa); /* TODO */
}
#endif /* TFA_ENABLE_INTERRUPT */

static int tfa9xxx_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	unsigned int sr;
	int len, prof, nprof, idx = 0;
	char *basename;
#endif
	u64 formats;
	int err;

	/*
	 * Support CODEC to CODEC links,
	 * these are called with a NULL runtime pointer.
	 */
	if (!substream->runtime)
		return 0;

	if (pcm_no_constraint != 0)
		return 0;

	switch (pcm_sample_format) {
	case 1:
		formats = SNDRV_PCM_FMTBIT_S24_LE;
		break;
	case 2:
		formats = SNDRV_PCM_FMTBIT_S32_LE;
		break;
	default:
		formats = SNDRV_PCM_FMTBIT_S16_LE;
		break;
	}

	err = snd_pcm_hw_constraint_mask64(substream->runtime,
		SNDRV_PCM_HW_PARAM_FORMAT, formats);
	if (err < 0)
		return err;

	if (no_start != 0)
		return 0;

	if (tfa9xxx->dsp_fw_state != TFA9XXX_DSP_FW_OK) {
		dev_info(codec->dev, "%s: Container file not loaded (state=%d)\n",
			__func__, tfa9xxx->dsp_fw_state);
		return -EINVAL;
	}

#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	basename = kzalloc(MAX_CONTROL_NAME, GFP_KERNEL);
	if (!basename)
		return -ENOMEM;

	/* copy profile name into basename until the . */
	get_profile_basename(basename, tfa_cnt_profile_name(tfa9xxx,
		tfa9xxx->profile));
	len = strlen(basename);
#endif /* TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION */

	/* loop over all profiles and get the supported samples rate(s) from
	 * the profiles with the same basename
	 */
	tfa9xxx->rate_constraint.list = &tfa9xxx->rate_constraint_list[0];
	tfa9xxx->rate_constraint.count = 0;

#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	nprof = tfa2_dev_get_dev_nprof(tfa9xxx->tfa);
	for (prof = 0; prof < nprof; prof++) {
		if (strncmp(basename,
			tfa_cnt_profile_name(tfa9xxx, prof), len) == 0) {
			/* Check which sample rate is supported with current profile,
			 * and enforce this.
			 */
			sr = get_profile_sr(tfa9xxx, prof);
			if (!sr)
				dev_info(codec->dev,
					"%s: Unable to identify supported sample rate\n",
					__func__);

			if (tfa9xxx->rate_constraint.count >= TFA9XXX_NUM_RATES) {
				dev_err(codec->dev, "%s: too many sample rates\n",
					__func__);
			} else {
				tfa9xxx->rate_constraint_list[idx++] = sr;
				tfa9xxx->rate_constraint.count += 1;
			}
		}
	}

	kfree(basename);

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_RATE,
		&tfa9xxx->rate_constraint);
#else
	dev_info(codec->dev, "%s: skip setting constraint, assuming fixed format\n",
		__func__);

	return 0;
#endif /* TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION */
}

static int tfa9xxx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec_dai->codec);

	tfa9xxx->sysclk = freq;

	return 0;
}

static int tfa9xxx_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s\n", __func__);

	return 0;
}

static int tfa9xxx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;

	dev_dbg(codec->dev, "%s: fmt=0x%x\n", __func__, fmt);

	/* Supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
			!= SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(codec->dev, "%s: Invalid Codec master mode\n",
				__func__);
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(codec->dev, "%s: Unsupported DAI format %d\n",
			__func__, fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	return 0;
}

static int tfa9xxx_get_fssel(unsigned int rate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_to_fssel); i++) {
		if (rate_to_fssel[i].rate == rate)
			return rate_to_fssel[i].fssel;
	}

	return -EINVAL;
}

static int tfa9xxx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
	unsigned int rate;
	int prof_idx;

	/* Supported */
	rate = params_rate(params);
	dev_dbg(&tfa9xxx->i2c->dev,
		"%s: Requested rate: %d, sample size: %d, physical size: %d\n",
		__func__, rate, snd_pcm_format_width(params_format(params)),
		snd_pcm_format_physical_width(params_format(params)));
#if defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	dev_info(&tfa9xxx->i2c->dev,
		"forced to change rate: %d to %d\n", rate, sr_converted);
	rate = sr_converted;
#endif

	if (no_start != 0)
		return 0;

	/* check if samplerate is supported for this mixer profile */
	prof_idx = get_profile_id_for_sr(tfa9xxx_mixer_profile, rate);
	if (prof_idx < 0) {
		dev_err(&tfa9xxx->i2c->dev, "%s: invalid sample rate %d.\n",
			__func__, rate);
		return -EINVAL;
	}
	dev_dbg(&tfa9xxx->i2c->dev,
		"%s: mixer profile:container profile = [%d:%d]\n",
		__func__, tfa9xxx_mixer_profile, prof_idx);

	/* update 'real' profile (container profile) */
	tfa9xxx->profile = prof_idx;

	/* update to new rate */
	tfa9xxx->rate = rate;

	return 0;
}

static int tfa9xxx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(&tfa9xxx->i2c->dev,
		"%s: state: %d (stream = %d)\n", __func__, mute, stream);

	if (no_start) {
		dev_dbg(&tfa9xxx->i2c->dev,
			"%s: no_start parameter set, returning\n", __func__);
		return 0;
	}

#if defined(TFA_USE_PSTREAM_ONLY)
	if (tfa9xxx->tfa->is_probus_device == 0) /* to check probus (DSP-free) */
		if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			pr_info("%s: skip cstream if running in non-probus device\n",
				__func__);
			return 0;
		}
#endif

	if (mute) {
		/* re-enable haptic f0 tracking when stopping audio playback */
		if (tfa9xxx->haptic_mode
			&& (stream == SNDRV_PCM_STREAM_PLAYBACK))
			tfa9xxx_disable_f0_tracking(tfa9xxx, false);

		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (tfa9xxx->pstream == 0) {
				pr_debug("mute:%d [pstream duplicated]\n",
					mute);
				return 0;
			}
			tfa9xxx->pstream = 0;
		} else {
			if (tfa9xxx->cstream == 0) {
				pr_debug("mute:%d [cstream duplicated]\n",
					mute);
				return 0;
			}
			tfa9xxx->cstream = 0;
		}
		pr_info("mute:%d [pstream %d, cstream %d]\n",
			mute,
			tfa9xxx->pstream, tfa9xxx->cstream);
		tfa9xxx->tfa->stream_state = (tfa9xxx->pstream & BIT_PSTREAM)
			|((tfa9xxx->cstream<<1) & BIT_CSTREAM);

		if (tfa9xxx->pstream != 0 || tfa9xxx->cstream != 0)
			return 0;

		pr_info("mute is triggered\n");
		mutex_lock(&tfa9xxx_mutex);
		tfa9xxx_sync_count = 0;
		mutex_unlock(&tfa9xxx_mutex);

		cancel_delayed_work_sync(&tfa9xxx->monitor_work);

		if (tfa9xxx->dsp_fw_state != TFA9XXX_DSP_FW_OK)
			return 0;

		mutex_lock(&tfa9xxx->dsp_lock);
		if (tfa9xxx->clk_users == 0) {
			tfa2_dev_stop(tfa9xxx->tfa);

#if defined(TFA_CONTROL_MCLK)
			dev_dbg(&tfa9xxx->i2c->dev,
				"%s: disable mclk\n", __func__);
#if defined(TFA_USE_GPIO_FOR_MCLK)
			if (gpio_is_valid(tfa9xxx->mclk_gpio))
				gpio_set_value_cansleep(tfa9xxx->mclk_gpio, 0);
#else
			if (tfa9xxx->mclk)
				clk_disable_unprepare(tfa9xxx->mclk);
#endif
#endif /* TFA_CONTROL_MCLK */
		}
		tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_STOPPED;
		mutex_unlock(&tfa9xxx->dsp_lock);
	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (tfa9xxx->pstream == 1) {
				pr_debug("mute:%d [pstream duplicated]\n",
					mute);
				return 0;
			}
			tfa9xxx->pstream = 1;
		} else {
			if (tfa9xxx->cstream == 1) {
				pr_debug("mute:%d [cstream duplicated]\n",
					mute);
				return 0;
			}
			tfa9xxx->cstream = 1;
		}

		pr_info("mute:%d [pstream %d, cstream %d]\n",
			mute,
			tfa9xxx->pstream, tfa9xxx->cstream);
		tfa9xxx->tfa->stream_state = (tfa9xxx->pstream & BIT_PSTREAM)
			|((tfa9xxx->cstream<<1) & BIT_CSTREAM);

		pr_info("unmute is triggered\n");
		/* Start DSP */
		tfa9xxx_dsp_init(tfa9xxx);

		/* disable haptic f0 tracking for audio playback */
		if (tfa9xxx->haptic_mode
			&& (stream == SNDRV_PCM_STREAM_PLAYBACK))
			tfa9xxx_disable_f0_tracking(tfa9xxx, true);
	}

	return 0;
}

static int tfa9xxx_i2s_trigger(struct snd_pcm_substream *substream, int cmd,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(&tfa9xxx->i2c->dev, "%s: cmd = %d stream = %d\n", __func__,
		cmd, substream->stream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tfa9xxx->trg_pstream = 1;
		else
			tfa9xxx->trg_cstream = 1;
		if (tfa9xxx->haptic_mode)
			tfa9xxx_bck_starts(tfa9xxx);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			tfa9xxx->trg_pstream = 0;
		else
			tfa9xxx->trg_cstream = 0;
		if (tfa9xxx->trg_pstream != 0 || tfa9xxx->trg_cstream != 0)
			return 0;
		if (tfa9xxx->haptic_mode)
			tfa9xxx_bck_stops(tfa9xxx);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct snd_soc_dai_ops tfa9xxx_dai_ops = {
	.startup = tfa9xxx_startup,
	.set_fmt = tfa9xxx_set_fmt,
	.set_sysclk = tfa9xxx_set_dai_sysclk,
	.set_tdm_slot = tfa9xxx_set_tdm_slot,
	.hw_params = tfa9xxx_hw_params,
	.mute_stream = tfa9xxx_mute,
	.trigger= tfa9xxx_i2s_trigger,
};

static struct snd_soc_dai_driver tfa9xxx_dai[] = {
	{
		.name = "tfa9xxx-aif", /* dedicated to tfa9xxx driver */
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 4,
			.rates = TFA9XXX_RATES,
			.formats = TFA9XXX_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF Capture",
			 .channels_min = 1,
			 .channels_max = 4,
			 .rates = TFA9XXX_RATES,
			 .formats = TFA9XXX_FORMATS,
		},
		.ops = &tfa9xxx_dai_ops,
#if !defined(TFA_USE_PSTREAM_ONLY)
		.symmetric_rates = 1,
#if KERNEL_VERSION(3,14,0) <= LINUX_VERSION_CODE
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
#endif
#endif
	},
};

static int tfa9xxx_probe(struct snd_soc_codec *codec)
{
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);
	int ret;

	dev_dbg(codec->dev, "%s\n", __func__);

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa9xxx->tfa9xxx_wq = create_singlethread_workqueue("tfa9xxx");
	if (!tfa9xxx->tfa9xxx_wq)
		return -ENOMEM;

	/* INIT_DELAYED_WORK(&tfa9xxx->init_work, tfa9xxx_dsp_init_work); */
	INIT_DELAYED_WORK(&tfa9xxx->monitor_work, tfa9xxx_monitor);
#if defined(TFA_ENABLE_INTERRUPT)
	INIT_DELAYED_WORK(&tfa9xxx->interrupt_work, tfa9xxx_interrupt);
#endif

	tfa9xxx->codec = codec;

	ret = tfa9xxx_load_container(tfa9xxx);
	dev_dbg(codec->dev, "%s: Container loading requested: %d\n",
		__func__, ret);

#if KERNEL_VERSION(3,16,0) > LINUX_VERSION_CODE
	codec->control_data = tfa9xxx->regmap;
	ret = snd_soc_codec_set_cache_io(codec, 8, 16, SND_SOC_REGMAP);
	if (ret != 0) {
		dev_err(codec->dev, "%s: Failed to set cache I/O: %d\n",
			__func__, ret);
		return ret;
	}
#endif

	tfa9xxx_add_widgets(tfa9xxx);

	dev_info(codec->dev, "%s: tfa9xxx codec registered (%s)",
		__func__, tfa9xxx->fw.name);

	return ret;
}

static int tfa9xxx_remove(struct snd_soc_codec *codec)
{
	struct tfa9xxx *tfa9xxx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);

#if defined(TFA_ENABLE_INTERRUPT)
	tfa9xxx_interrupt_enable(tfa9xxx, false);

	cancel_delayed_work_sync(&tfa9xxx->interrupt_work);
#endif
	cancel_delayed_work_sync(&tfa9xxx->monitor_work);
	/* cancel_delayed_work_sync(&tfa9xxx->init_work); */

	if (tfa9xxx->tfa9xxx_wq)
		destroy_workqueue(tfa9xxx->tfa9xxx_wq);

	return 0;
}

#if KERNEL_VERSION(3,16,0) <= LINUX_VERSION_CODE
static struct regmap *tfa9xxx_get_regmap(struct device *dev)
{
	struct tfa9xxx *tfa9xxx = dev_get_drvdata(dev);

	return tfa9xxx->regmap;
}
#endif

static struct snd_soc_codec_driver soc_codec_dev_tfa9xxx = {
	.probe =	tfa9xxx_probe,
	.remove =	tfa9xxx_remove,
#if KERNEL_VERSION(3,16,0) <= LINUX_VERSION_CODE
	.get_regmap = tfa9xxx_get_regmap,
#endif
};

static bool tfa9xxx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa9xxx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa9xxx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa9xxx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA9XXX_MAX_REGISTER,
	.writeable_reg = tfa9xxx_writeable_register,
	.readable_reg = tfa9xxx_readable_register,
	.volatile_reg = tfa9xxx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

#if defined(TFA_ENABLE_INTERRUPT)
static void tfa9xxx_irq_tfa2(struct tfa9xxx *tfa9xxx)
{
	dev_info(&tfa9xxx->i2c->dev, "%s\n", __func__);

	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	tfa_irq_mask(tfa9xxx->tfa); /* TODO */

	queue_delayed_work(tfa9xxx->tfa9xxx_wq, &tfa9xxx->interrupt_work, 0);
}
#endif /* TFA_ENABLE_INTERRUPT */

static irqreturn_t tfa9xxx_irq(int irq, void *data)
{
#if defined(TFA_ENABLE_INTERRUPT)
	struct tfa9xxx *tfa9xxx = data;

	tfa9xxx_irq_tfa2(tfa9xxx);
#endif

	return IRQ_HANDLED;
}

static int tfa9xxx_ext_reset(struct tfa9xxx *tfa9xxx)
{
	if (tfa9xxx && gpio_is_valid(tfa9xxx->reset_gpio)) {
		gpio_set_value_cansleep(tfa9xxx->reset_gpio, 1);
		mdelay(1);
		gpio_set_value_cansleep(tfa9xxx->reset_gpio, 0);
		mdelay(1);
	}
	return 0;
}

static int tfa9xxx_parse_dt(struct device *dev,
	struct tfa9xxx *tfa9xxx, struct device_node *np)
{
	tfa9xxx->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (tfa9xxx->reset_gpio < 0)
		dev_dbg(dev, "No reset GPIO provided, will not HW reset device\n");

	tfa9xxx->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (tfa9xxx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");

	tfa9xxx->haptic_mode = of_find_property(np, "haptic", NULL);
	if (tfa9xxx->haptic_mode)
		dev_dbg(dev, "using Haptic mode\n");

#if defined(TFA_USE_GPIO_FOR_MCLK)
	tfa9xxx->mclk_gpio = of_get_named_gpio(np, "mclk-gpio", 0);
	if (tfa9xxx->mclk_gpio < 0) {
		dev_dbg(dev, "No mclk GPIO provided, will not control mclk\n");
		tfa9xxx->mclk = NULL;
	}
#endif

	return 0;
}

static ssize_t tfa9xxx_reg_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa9xxx *tfa9xxx = dev_get_drvdata(dev);

	if (count != 1) {
		dev_dbg(dev, "%s: invalid register address\n",
			__func__);
		return -EINVAL;
	}

	tfa9xxx->reg = buf[0];

	return 1;
}

static ssize_t tfa9xxx_rw_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa9xxx *tfa9xxx = dev_get_drvdata(dev);
	u8 *data;
	int ret;
	int retries = I2C_RETRIES;

	data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		dev_dbg(dev, "%s: can not allocate memory\n",
			__func__);
		return  -ENOMEM;
	}

	data[0] = tfa9xxx->reg;
	memcpy(&data[1], buf, count);

retry:
	ret = i2c_master_send(tfa9xxx->i2c, data, count+1);
	if (ret < 0) {
		dev_warn(dev, "%s: i2c error, retries left: %d\n",
			__func__, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);

	/* the number of data bytes written without the register address */
	return ((ret > 1) ? count : -EIO);
}

static ssize_t tfa9xxx_rw_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa9xxx *tfa9xxx = dev_get_drvdata(dev);
	struct i2c_msg msgs[] = {
		{
			.addr = tfa9xxx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa9xxx->reg,
		},
		{
			.addr = tfa9xxx->i2c->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;

retry:
	ret = i2c_transfer(tfa9xxx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		dev_warn(dev, "%s: i2c error, retries left: %d\n",
			__func__, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}

	/* ret contains the number of i2c transaction */
	/* return the number of bytes read */
	return ((ret > 1) ? count : -EIO);
}

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = 0600,
	},
	.size = 0,
	.read = tfa9xxx_rw_read,
	.write = tfa9xxx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = 0200,
	},
	.size = 0,
	.read = NULL,
	.write = tfa9xxx_reg_write,
};

int tfa2_i2c_write_raw(struct i2c_client *client, int len, const u8 *data)
{
	int error = 0;
	int ret;
	int retries = I2C_RETRIES;

	if (!client) {
		pr_err("No device available\n");
		return -ENODEV;
	}

retry:
	ret = i2c_master_send(client, data, len);
	if (ret < 0) {
		dev_warn(&client->dev, "%s: i2c error, retries left: %d\n",
			__func__, retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	if (ret == len) {
		return 0;
	}

	dev_err(&client->dev,
		"%s: WR-RAW (len=%d) Error I2C send size mismatch %d\n",
		__func__, len, ret);
	error = -ENODEV;

	return error;
}

int tfa2_i2c_write_read_raw(struct i2c_client *client,
	int wrlen, u8 *wrdata, int rdlen, u8 *rddata)
{
	int error = 0;
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = wrlen,
			.buf = wrdata,
		},
		{
			.flags = I2C_M_RD,
			.len = rdlen,
			.buf = rddata,
		},
	};

	if (!client) {
		pr_err("No device available\n");
		return -ENODEV;
	}

	msgs[0].addr = client->addr;
	msgs[1].addr = client->addr;

	do {
		err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
		if (err != ARRAY_SIZE(msgs))
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

	if (err != ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s: write/read transfer error %d\n",
			__func__, err);
		error = -ENODEV;
	}

	return error;
}

static int tfa9xxx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct tfa9xxx *tfa9xxx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags;
	unsigned int reg;
	int ret;

	dev_info(&i2c->dev, "%s\n", __func__);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	tfa9xxx = devm_kzalloc(&i2c->dev, sizeof(struct tfa9xxx), GFP_KERNEL);
	if (tfa9xxx == NULL)
		return -ENOMEM;

	tfa9xxx->dev = &i2c->dev;
	tfa9xxx->i2c = i2c;
	tfa9xxx->dsp_init = TFA9XXX_DSP_INIT_STOPPED;
	tfa9xxx->rate = 48000; /* init to the default sample rate (48kHz) */
	tfa9xxx->tfa = NULL;

	tfa9xxx->regmap = devm_regmap_init_i2c(i2c, &tfa9xxx_regmap);
	if (IS_ERR(tfa9xxx->regmap)) {
		ret = PTR_ERR(tfa9xxx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}
	i2c_set_clientdata(i2c, tfa9xxx);
	mutex_init(&tfa9xxx->dsp_lock);
	init_waitqueue_head(&tfa9xxx->wq);

	if (np) {
		ret = tfa9xxx_parse_dt(&i2c->dev, tfa9xxx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			return ret;
		}
		if (no_start)
			tfa9xxx->irq_gpio = -1;
		if (no_reset)
			tfa9xxx->reset_gpio = -1;
	} else {
		tfa9xxx->reset_gpio = -1;
		tfa9xxx->irq_gpio = -1;
#if defined(TFA_USE_GPIO_FOR_MCLK)
		tfa9xxx->mclk_gpio = -1;
#endif
	}

	if (gpio_is_valid(tfa9xxx->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa9xxx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TFA9XXX_RST");
		if (ret)
			return ret;
	}

	if (gpio_is_valid(tfa9xxx->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa9xxx->irq_gpio,
			GPIOF_DIR_IN, "TFA9XXX_INT");
		if (ret)
			return ret;
	}

#if defined(TFA_USE_GPIO_FOR_MCLK)
	if (gpio_is_valid(tfa9xxx->mclk_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa9xxx->mclk_gpio,
			GPIOF_OUT_INIT_LOW, "TFA9XXX_MCLK");
		if (ret)
			return ret;
	}
#endif

	/* enable status monitor thread */
	if (!no_monitor && !no_start)
		tfa9xxx->enable_monitor = true;

#if defined(TFA_USE_GPIO_FOR_MCLK)
	/* skip setting clock, assuming synchronous setting with cnt */
	if (gpio_is_valid(tfa9xxx->mclk_gpio)) {
		dev_info(&i2c->dev, "using mclk_gpio\n");
#if !defined(TFA_CONTROL_MCLK)
		dev_info(&i2c->dev, "set mclk_gpio active\n");
		gpio_set_value_cansleep(tfa9xxx->mclk_gpio, 1);
#endif
	}
#else
	tfa9xxx->mclk = devm_clk_get(&i2c->dev, "mclk");
	if (IS_ERR(tfa9xxx->mclk)) {
		dev_warn(&i2c->dev, "mclk not set\n");
		tfa9xxx->mclk = NULL;
	}

	if (tfa9xxx->mclk) {
		dev_info(&i2c->dev, "using mclk\n");
/*
		ret = clk_set_rate(tfa9xxx->mclk, FIXED_MCLK_RATE);
		if (ret < 0) {
			dev_warn(&i2c->dev, "failed in setting rate to mclk\n");
			tfa9xxx->mclk = NULL;
		}
*/
#if defined(TFA_CONTROL_MCLK)
		dev_info(&i2c->dev, "set mclk active\n");
		if (tfa9xxx->mclk) {
			ret = clk_prepare_enable(tfa9xxx->mclk);
			exynos9610_set_xclkout0_13();
			if (ret < 0)
				dev_warn(&i2c->dev, "failed in enabling mclk\n");
		}
#endif
	}
#endif /* TFA_USE_GPIO_FOR_MCLK */

	/* Power up! */
	tfa9xxx_ext_reset(tfa9xxx);

	if ((no_start == 0) && (no_reset == 0)) {
		ret = regmap_read(tfa9xxx->regmap, 0x03, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to read Revision register: %d\n",
				ret);
			return -EIO;
		}

		dev_info(&i2c->dev, "TFA REVID:0x%04X", reg);

		tfa9xxx_log_revision = reg & 0xff;
		tfa9xxx_log_subrevision = (reg >> 8) & 0xff;

		switch (reg & 0xff) {
#if defined (FULL_TFA_SUPPORT) /* TODO: other TFA2 mono devices */
		case 0x72: /* tfa9872 */
			dev_info(&i2c->dev, "TFA9872 detected\n");
			tfa9xxx->flags |= TFA9XXX_FLAG_REMOVE_PLOP_NOISE;
			/* tfa9xxx->flags |= TFA9XXX_FLAG_LP_MODES; */
			break;
		case 0x74: /* tfa9874 */
			dev_info(&i2c->dev, "TFA9874 detected\n");
			break;
		case 0x13: /* tfa9912 */
			dev_info(&i2c->dev, "TFA9912 detected\n");
			break;
#endif
		case 0x94: /* tfa9894 */
			dev_info(&i2c->dev, "TFA9894 detected\n");
			break;
		default:
			dev_err(&i2c->dev, "Unsupported device revision (0x%x)\n",
				reg & 0xff);
			return -EINVAL;
		}
	}

	tfa9xxx->tfa = devm_kzalloc(&i2c->dev,
		sizeof(struct tfa2_device), GFP_KERNEL);
	if (tfa9xxx->tfa == NULL)
		return -ENOMEM;

	tfa9xxx->tfa->data = (void *)tfa9xxx;
	/* tfa9xxx->tfa->cachep = tfa9xxx_cache; */

	/* Modify the stream names, by appending the i2c device address.
	 * This is used with multicodec, in order to discriminate the devices.
	 * Stream names appear in the dai definition and in the stream  	 .
	 * We create copies of original structures because each device will
	 * have its own instance of this structure, with its own address.
	 */
	dai = devm_kzalloc(&i2c->dev, sizeof(tfa9xxx_dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;
	memcpy(dai, tfa9xxx_dai, sizeof(tfa9xxx_dai));

	tfa9xxx_append_i2c_address(&i2c->dev,
		i2c,
		NULL,
		0,
		dai,
		ARRAY_SIZE(tfa9xxx_dai));

	ret = snd_soc_register_codec(&i2c->dev,
		&soc_codec_dev_tfa9xxx, dai,
		ARRAY_SIZE(tfa9xxx_dai));
	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register TFA9xxx: %d\n", ret);
		return ret;
	}

	tfa2_haptic_set_version_check(haptic_version_check);

	if (tfa9xxx->haptic_mode) {
		ret = tfa9xxx_haptic_probe(tfa9xxx);
		if (ret < 0) {
			dev_err(&i2c->dev, "Failed to to setup haptic: %d\n", ret);
			tfa9xxx->haptic_mode = false;
		}
	}

	if (gpio_is_valid(tfa9xxx->irq_gpio) &&
		!(tfa9xxx->flags & TFA9XXX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
			gpio_to_irq(tfa9xxx->irq_gpio),
			NULL, tfa9xxx_irq, irq_flags,
			"tfa9xxx", tfa9xxx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
				gpio_to_irq(tfa9xxx->irq_gpio), ret);
			return ret;
		}
	} else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa9xxx->flags |= TFA9XXX_FLAG_SKIP_INTERRUPTS;
	}

#if defined(CONFIG_DEBUG_FS)
	if (no_start == 0)
		tfa9xxx_debugfs_init(tfa9xxx, i2c);
#endif
	/* Register the sysfs files for climax backdoor access */
	ret = device_create_bin_file(&i2c->dev, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");

	tfa9xxx_log_i2c_devicenum = i2c->adapter->nr;
	tfa9xxx_log_i2c_slaveaddress = i2c->addr;
	tfa9xxx_log_start_cnt = 0;

	dev_info(&i2c->dev, "%s Probe completed successfully!\n", __func__);

	INIT_LIST_HEAD(&tfa9xxx->list);

	mutex_lock(&tfa9xxx_mutex);
	tfa9xxx_device_count++;
	list_add(&tfa9xxx->list, &tfa9xxx_device_list);
	mutex_unlock(&tfa9xxx_mutex);

	return 0;
}

static int tfa9xxx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa9xxx *tfa9xxx = i2c_get_clientdata(i2c);

	dev_info(&i2c->dev, "%s\n", __func__);

#if defined(TFA_ENABLE_INTERRUPT)
	tfa9xxx_interrupt_enable(tfa9xxx, false);
#endif

#if defined(TFA_ENABLE_INTERRUPT)
	cancel_delayed_work_sync(&tfa9xxx->interrupt_work);
#endif
	cancel_delayed_work_sync(&tfa9xxx->monitor_work);
	/* cancel_delayed_work_sync(&tfa9xxx->init_work); */

	device_remove_bin_file(&i2c->dev, &dev_attr_reg);
	device_remove_bin_file(&i2c->dev, &dev_attr_rw);
#if defined(CONFIG_DEBUG_FS)
	tfa9xxx_debug_remove(tfa9xxx);
#endif

	snd_soc_unregister_codec(&i2c->dev);

	if (tfa9xxx->haptic_mode)
		tfa9xxx_haptic_remove(tfa9xxx);

#if defined(TFA_USE_GPIO_FOR_MCLK)
	if (gpio_is_valid(tfa9xxx->mclk_gpio)) {
		gpio_set_value_cansleep(tfa9xxx->mclk_gpio, 0);
		devm_gpio_free(&i2c->dev, tfa9xxx->mclk_gpio);
	}
#else
	if (tfa9xxx->mclk) {
		clk_disable_unprepare(tfa9xxx->mclk);
		devm_clk_put(&i2c->dev, tfa9xxx->mclk);
	}
#endif

	if (gpio_is_valid(tfa9xxx->irq_gpio))
		devm_gpio_free(&i2c->dev, tfa9xxx->irq_gpio);
	if (gpio_is_valid(tfa9xxx->reset_gpio))
		devm_gpio_free(&i2c->dev, tfa9xxx->reset_gpio);

	mutex_lock(&tfa9xxx_mutex);
	list_del(&tfa9xxx->list);
	tfa9xxx_device_count--;
	if (tfa9xxx_device_count == 0) {
		kfree(tfa9xxx_container);
		tfa9xxx_container = NULL;
	}
	mutex_unlock(&tfa9xxx_mutex);

	return 0;
}

static const struct i2c_device_id tfa9xxx_i2c_id[] = {
	{"tfa9xxx", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tfa9xxx_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id tfa9xxx_dt_match[] = {
	{.compatible = "nxp,tfa9xxx"},
	{},
};
#endif

static struct i2c_driver tfa9xxx_i2c_driver = {
	.driver = {
		.name = "tfa9xxx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa9xxx_dt_match),
	},
	.probe =    tfa9xxx_i2c_probe,
	.remove =   tfa9xxx_i2c_remove,
	.id_table = tfa9xxx_i2c_id,
};

static int __init tfa9xxx_drv_init(void)
{
	int ret = 0;

	pr_info("TFA9XXX driver version %s\n", TFA9XXX_VERSION);

	/* Enable debug traces */
	tfa9xxx_kmsg_regs = trace_level & 2;
	tfa9xxx_ftrace_regs = trace_level & 4;

#if defined(TFA_USE_KMEM_CACHE)
	/* Initialize kmem_cache */
	tfa9xxx_cache = kmem_cache_create("tfa9xxx_cache",
		/* Cache name /proc/slabinfo */
		PAGE_SIZE, /* Structure size, to fit in single page */
		0, /* Structure alignment */
		(SLAB_HWCACHE_ALIGN | SLAB_RECLAIM_ACCOUNT |
		SLAB_MEM_SPREAD), /* Cache property */
		NULL); /* Object constructor */
	if (!tfa9xxx_cache) {
		pr_err("tfa9xxx can't create memory pool\n");
		ret = -ENOMEM;
	}
#endif

	ret = i2c_add_driver(&tfa9xxx_i2c_driver);
	if (ret < 0)
		return ret;

	ret = tfa9xxx_haptic_init();

	return ret;
}
module_init(tfa9xxx_drv_init);

static void __exit tfa9xxx_drv_exit(void)
{
	tfa9xxx_haptic_exit();
	i2c_del_driver(&tfa9xxx_i2c_driver);
#if defined(TFA_USE_KMEM_CACHE)
	kmem_cache_destroy(tfa9xxx_cache);
#endif
}
module_exit(tfa9xxx_drv_exit);

MODULE_DESCRIPTION("ASoC TFA9XXX driver");
MODULE_LICENSE("GPL");
