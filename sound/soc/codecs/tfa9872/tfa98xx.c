/*
 * tfa98xx.c   tfa98xx codec module
 *
 * Copyright (c) 2015 NXP Semiconductors
 *
 *  Author: Sebastien Jan <sjan@baylibre.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define pr_fmt(fmt) "%s(): " fmt, __func__

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/input.h>
#include "config.h"

#define I2C_RETRIES 50
#define I2C_RETRY_DELAY 5 /* ms */
/* TODO: remove genregs usage? */
#ifdef N1A
#include "tfa98xx_genregs_N1A12.h"
#else
#include "tfa98xx_genregs_N1C.h"
#endif
#if defined(USE_TFA9891)
#include "tfa9891_genregs.h"
#endif

#include "tfa98xx_tfafieldnames.h"
#include "tfa_internal.h"
#include "tfa.h"
#include "tfa_service.h"
#include "tfa_container.h"
#include "tfa98xx_parameters.h"

#if defined(USE_TFA9896) /* boosting only for Samsung */
#undef TFA_READ_BATTERY_TEMP
#else
#define TFA_READ_BATTERY_TEMP
#endif
#if defined(TFA_READ_BATTERY_TEMP)
#include <linux/power_supply.h>
#endif

#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
#include <sound/samsung/sec_audio_debug.h>
#endif

#define TFA98XX_VERSION	TFA98XX_API_REV_STR

#if defined(TFA_NO_SND_FORMAT_CHECK)
#define TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION
#endif

/* Change volume selection behavior:
 * Uncomment following line to generate a profile change when updating
 * a volume control (also changes to the profile of the modified  volume
 * control)
 */
/* #define TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL	1 */

/* Supported rates and data formats */
#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_48000
#else
#define TFA98XX_RATES SNDRV_PCM_RATE_8000_192000
#endif
#define TFA98XX_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

#define TF98XX_MAX_DSP_START_TRY_COUNT	10

#define TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE
#define TFA_CHECK_CALIBRATE_DONE
#define TFA_DBGFS_CHECK_MTPEX

#define XMEM_TAP_ACK  0x0122
#define XMEM_TAP_READ 0x010f

int tfa98xx_log_revision;
int tfa98xx_log_subrevision;
int tfa98xx_log_i2c_devicenum;
int tfa98xx_log_i2c_slaveaddress;

#if defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
static unsigned int sr_converted = 48000;
#endif

static LIST_HEAD(profile_list); /* list of user selectable profiles */

static int tfa98xx_kmsg_regs;
static int tfa98xx_ftrace_regs;

static struct tfa98xx *tfa98xx_devices[MAX_HANDLES]
	= {NULL, NULL, NULL, NULL};
static int tfa98xx_registered_handles;
static int tfa98xx_vsteps[MAX_HANDLES] = {0, 0, 0, 0};
static int tfa98xx_profile; /* store profile */
static int tfa98xx_prof_vsteps[10] = {0};
/* store vstep per profile (single device) */
static int tfa98xx_mixer_profiles; /* number of user selectable profiles */
static int tfa98xx_mixer_profile; /* current mixer profile */
#if defined(TFADSP_DSP_BUFFER_POOL)
static int buf_pool_size[POOL_MAX_INDEX] = {
	64*1024,
	64*1024,
	64*1024,
	64*1024,
	64*1024,
	8*1024
};
#endif

static DEFINE_MUTEX(probe_lock);

static char *dflt_prof_name = "";
module_param(dflt_prof_name, charp, 0444);

static int no_start;
module_param(no_start, int, 0444);
MODULE_PARM_DESC(no_start, "do not start the work queue; for debugging via user\n");

static int partial_enable;
module_param(partial_enable, int, 0644);
MODULE_PARM_DESC(partial_enable, "enable partial update messaging\n");

static int pcm_sample_format;
module_param(pcm_sample_format, int, 0444);
MODULE_PARM_DESC(pcm_sample_format, "PCM sample format: 0=S16_LE, 1=S24_LE, 2=S32_LE\n");

#if defined(TFA_NO_SND_FORMAT_CHECK)
static int pcm_no_constraint = 1;
#else
static int pcm_no_constraint;
#endif
module_param(pcm_no_constraint, int, 0444);
MODULE_PARM_DESC(pcm_no_constraint, "do not use constraints for PCM parameters\n");

#if defined(USE_TFA9891)
static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx);
#endif
static int tfa98xx_get_fssel(unsigned int rate);
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable);

static int get_profile_from_list(char *buf, int id);
static int get_profile_id_for_sr(int id, unsigned int rate);

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream);
static int _tfa98xx_stop(struct tfa98xx *tfa98xx);

#if defined(TFA_READ_BATTERY_TEMP)
static enum tfa98xx_error tfa98xx_read_battery_temp(int *value);
#endif

struct tfa98xx_rate {
	unsigned int rate;
	unsigned int fssel;
};

static struct tfa98xx_rate rate_to_fssel[] = {
	{ 8000, 0 },
	{ 11025, 1 },
	{ 12000, 2 },
	{ 16000, 3 },
	{ 22050, 4 },
	{ 24000, 5 },
	{ 32000, 6 },
	{ 44100, 7 },
	{ 48000, 8 },
#if defined(TFA_NO_SND_FORMAT_CHECK)
/* out of range */
	{ 64000, 9 },
	{ 88200, 10 },
	{ 96000, 11 },
	{ 176400, 12 },
	{ 192000, 13 },
#endif
};

/* Wrapper for tfa start */
static enum tfa_error
tfa98xx_tfa_start(struct tfa98xx *tfa98xx, int next_profile, int *vstep)
{
	enum tfa_error err;

	err = tfa_start(next_profile, vstep);

#if defined(USE_TFA9891)
	/* Check and update tap-detection state (in case of profile change) */
	tfa98xx_tapdet_check_update(tfa98xx);
#endif

	/* Remove sticky bit by reading it once */
	TFA_GET_BF(tfa98xx->handle, NOCLK);

	/* A cold start erases the configuration, including interrupts setting.
	 * Restore it if required
	 */
	tfa98xx_interrupt_enable(tfa98xx, true);

	return err;
}

static int tfa98xx_input_open(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);

	dev_dbg(tfa98xx->codec->dev, "opening device file\n");

	/* note: open function is called only once by the framework.
	 * No need to count number of open file instances.
	 */
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		dev_dbg(&tfa98xx->i2c->dev,
			"DSP not loaded, cannot start tap-detection\n");
		return -EIO;
	}

#if defined(USE_TFA9891)
	/* enable tap-detection service */
	tfa98xx->tapdet_open = true;
	tfa98xx_tapdet_check_update(tfa98xx);
#endif

	return 0;
}

static void tfa98xx_input_close(struct input_dev *dev)
{
	struct tfa98xx *tfa98xx = input_get_drvdata(dev);

	dev_dbg(tfa98xx->codec->dev, "closing device file\n");

	/* Note: close function is called if the device is unregistered */

#if defined(USE_TFA9891)
	/* disable tap-detection service */
	tfa98xx->tapdet_open = false;
	tfa98xx_tapdet_check_update(tfa98xx);
#endif
}

static int tfa98xx_register_inputdev(struct tfa98xx *tfa98xx)
{
	int err;
	struct input_dev *input;

	input = input_allocate_device();

	if (!input) {
		dev_err(tfa98xx->codec->dev,
			"Unable to allocate input device\n");
		return -ENOMEM;
	}

	input->evbit[0] = BIT_MASK(EV_KEY);
	input->keybit[BIT_WORD(BTN_0)] |= BIT_MASK(BTN_0);
	input->keybit[BIT_WORD(BTN_1)] |= BIT_MASK(BTN_1);
	input->keybit[BIT_WORD(BTN_2)] |= BIT_MASK(BTN_2);
	input->keybit[BIT_WORD(BTN_3)] |= BIT_MASK(BTN_3);
	input->keybit[BIT_WORD(BTN_4)] |= BIT_MASK(BTN_4);
	input->keybit[BIT_WORD(BTN_5)] |= BIT_MASK(BTN_5);
	input->keybit[BIT_WORD(BTN_6)] |= BIT_MASK(BTN_6);
	input->keybit[BIT_WORD(BTN_7)] |= BIT_MASK(BTN_7);
	input->keybit[BIT_WORD(BTN_8)] |= BIT_MASK(BTN_8);
	input->keybit[BIT_WORD(BTN_9)] |= BIT_MASK(BTN_9);

	input->open = tfa98xx_input_open;
	input->close = tfa98xx_input_close;

	input->name = "tfa98xx-tapdetect";

	input->id.bustype = BUS_I2C;
	input_set_drvdata(input, tfa98xx);

	err = input_register_device(input);
	if (err) {
		dev_err(tfa98xx->codec->dev,
			"Unable to register input device\n");
		goto err_free_dev;
	}

	dev_dbg(tfa98xx->codec->dev,
		"Input device for tap-detection registered: %s\n",
		input->name);
	tfa98xx->input = input;
	return 0;

err_free_dev:
	input_free_device(input);
	return err;
}

/*
 * Check if an input device for tap-detection can and shall be registered.
 * Register it if appropriate.
 * If already registered, check if still relevant and remove it if necessary.
 * unregister: true to request inputdev unregistration.
 */
static void
__tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx,
	bool unregister)
{
	bool tap_profile = false;
	unsigned int i;

	for (i = 0; i < tfa_cont_max_profile(tfa98xx->handle); i++) {
		if (strnstr(tfa_cont_profile_name(tfa98xx->handle, i), ".tap",
			strlen(tfa_cont_profile_name(tfa98xx->handle, i)))
			!= NULL) {
			tap_profile = true;
			tfa98xx->tapdet_profiles |= 1 << i;
			dev_info(tfa98xx->codec->dev,
				"found a tap-detection profile (%d - %s)\n",
				i, tfa_cont_profile_name(tfa98xx->handle, i));
		}
	}

	/* Check for device support:
	 *  - at device level
	 *  - at container (profile) level
	 */
	if (!(tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) ||
		!tap_profile ||
		unregister) {
		/* No input device supported or required */
		if (tfa98xx->input) {
			input_unregister_device(tfa98xx->input);
			tfa98xx->input = NULL;
		}
		return;
	}

	/* input device required */
	if (tfa98xx->input)
		dev_info(tfa98xx->codec->dev,
			"Input device already registered, skipping\n");
	else
		tfa98xx_register_inputdev(tfa98xx);
}

static void tfa98xx_inputdev_check_register(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, false);
}

static void tfa98xx_inputdev_unregister(struct tfa98xx *tfa98xx)
{
	__tfa98xx_inputdev_check_register(tfa98xx, true);
}

#ifdef CONFIG_DEBUG_FS
/* OTC reporting
 * Returns the MTP0 OTC bit value
 */
static int tfa98xx_dbgfs_otc_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa98xx_control *otc = &(handles_local[tfa98xx->handle]
					.dev_ops.controls.otc);
	enum tfa98xx_error err, status;
	unsigned short value;

	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}

	err = tfa98xx_get_mtp(tfa98xx->handle, &value);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (otc->deferrable) {
		if (err != TFA98XX_ERROR_OK && err != TFA98XX_ERROR_NO_CLOCK) {
			pr_err("[0x%x] Unable to check DSP access: %d\n",
				tfa98xx->i2c->addr, err);
			return -EIO;
		} else if (err == TFA98XX_ERROR_NO_CLOCK) {
			if (otc->rd_valid) {
				/* read cached value */
				*val = otc->rd_value;
				pr_debug("[0x%x] Returning cached value of OTC: %llu\n",
					tfa98xx->i2c->addr, *val);
			} else {
				pr_info("[0x%x] OTC value never read!\n",
					tfa98xx->i2c->addr);
				return -EIO;
			}
			return 0;
		}
	}

	*val = (u64)((value & TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK)
		>> TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS);
	pr_debug("[0x%x] OTC : %d\n", tfa98xx->i2c->addr, value&1);

	if (otc->deferrable) {
		otc->rd_value = *val;
		otc->rd_valid = true;
	}

	return 0;
}

static int tfa98xx_dbgfs_otc_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa98xx_control *otc = &(handles_local[tfa98xx->handle]
					.dev_ops.controls.otc);
	enum tfa98xx_error err, status;

	if (val != 0 && val != 1) {
		pr_err("[0x%x] Unexpected value %llu\n\n",
			tfa98xx->i2c->addr, val);
		return -EINVAL;
	}
	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}
	err = tfa98xx_set_mtp(tfa98xx->handle,
			(val << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS)
			& TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK,
			TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (otc->deferrable) {
		if (err != TFA98XX_ERROR_OK && err != TFA98XX_ERROR_NO_CLOCK) {
			pr_err("[0x%x] Unable to check DSP access: %d\n",
				tfa98xx->i2c->addr, err);
			return -EIO;
		} else if (err == TFA98XX_ERROR_NO_CLOCK) {
			/* defer OTC */
			otc->wr_value = val;
			otc->triggered = true;
			pr_debug("[0x%x] Deferring write to OTC (%d)\n",
				tfa98xx->i2c->addr, otc->wr_value);
			return 0;
		}
	}

	/* deferrable: cache the value for subsequent offline read */
	if (otc->deferrable) {
		otc->rd_value = val;
		otc->rd_valid = true;
	}

	pr_debug("[0x%x] OTC < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error err, status;
	unsigned short value;

	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}
	err = tfa98xx_get_mtp(tfa98xx->handle, &value);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (err != TFA98XX_ERROR_OK) {
		pr_err("[0x%x] Unable to check DSP access: %d\n",
			tfa98xx->i2c->addr, err);
		return -EIO;
	}

	*val = (u64)((value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK)
		>> TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_POS);
	pr_debug("[0x%x] MTPEX : %d\n", tfa98xx->i2c->addr, value & 2 >> 1);

	return 0;
}

static int tfa98xx_dbgfs_mtpex_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa98xx_control *mtpex = &(handles_local[tfa98xx->handle]
		.dev_ops.controls.mtpex);
	enum tfa98xx_error err, status;

	if (val != 0) {
		pr_err("[0x%x] Can only clear MTPEX (0 value expected)\n",
			tfa98xx->i2c->addr);
		return -EINVAL;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}
	err = tfa98xx_set_mtp(tfa98xx->handle, 0,
					TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	if (mtpex->deferrable) {
		if (err != TFA98XX_ERROR_OK && err != TFA98XX_ERROR_NO_CLOCK) {
			pr_err("[0x%x] Unable to check DSP access: %d\n",
				tfa98xx->i2c->addr, err);
			return -EIO;
		} else if (err == TFA98XX_ERROR_NO_CLOCK) {
			/* defer OTC */
			mtpex->wr_value = 0;
			mtpex->triggered = true;
			pr_debug("[0x%x] Deferring write to MTPEX (%d)\n",
				tfa98xx->i2c->addr, mtpex->wr_value);
			return 0;
		}
	}

	pr_debug("[0x%x] MTPEX < 0\n", tfa98xx->i2c->addr);

	return 0;
}

static int tfa98xx_dbgfs_temp_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error status;

	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}
	*val = tfa98xx_get_exttemp(tfa98xx->handle);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP : %llu\n", tfa98xx->i2c->addr, *val);

	return 0;
}

static int tfa98xx_dbgfs_temp_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error status;
#if defined(TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE)
	struct tfa98xx_control *otc, *mtpex;
#endif /* TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE */

	mutex_lock(&tfa98xx->dsp_lock);
	status = tfa98xx_open(tfa98xx->handle);
	if (status) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}

#if defined(TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE)
	otc = &(handles_local[tfa98xx->handle].dev_ops.controls.otc);
	mtpex = &(handles_local[tfa98xx->handle].dev_ops.controls.mtpex);

	pr_debug("%s: otc->deferrable:%d, mtpex->deferrable:%d\n",
		__func__, otc->deferrable, mtpex->deferrable);
#endif /* TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE */

	tfa98xx_set_exttemp(tfa98xx->handle, (short)val);
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);

	pr_debug("[0x%x] TEMP < %llu\n", tfa98xx->i2c->addr, val);

	return 0;
}

/*
 * calibration:
 * write key phrase to the 'calibration' file to trigger a new calibration
 * read the calibration file once to get the calibration result
 */
/* tfa98xx_deferred_calibration_status - from tfa_run_wait_calibration */
void tfa98xx_deferred_calibration_status(tfa98xx_handle_t handle,
	int calibrate_done)
{
	struct tfa98xx *tfa98xx = tfa98xx_devices[handle];
	struct tfa98xx_control *calib = &(handles_local[handle]
		.dev_ops.controls.calib);

	if (calib->wr_value) {
		/* a calibration was programmed from the calibration file
		 * interface
		 */
		switch (calibrate_done) {
		case 1:
			/* calibration complete ! */
			calib->wr_value = false; /* calibration over */
			calib->rd_valid = true; /* result available */
			calib->rd_value = true; /* result valid */
			tfa_dsp_get_calibration_impedance(tfa98xx->handle);
			tfa98xx->calibrate_done = 1;
			wake_up_interruptible(&tfa98xx->wq);
			break;
		case 0:
			pr_info("[0x%x] Calibration not complete, still waiting...\n",
				tfa98xx->i2c->addr);
			break;
		case -1:
			pr_info("[0x%x] Calibration failed\n",
				tfa98xx->i2c->addr);
			calib->wr_value = false; /* calibration over */
			calib->rd_valid = true; /* result available */
			calib->rd_value = false; /* result not valid */
			tfa98xx->calibrate_done = 0;
			wake_up_interruptible(&tfa98xx->wq);
			break;
		default:
			pr_info("[0x%x] Unknown calibration status: %d\n",
				tfa98xx->i2c->addr, calibrate_done);
		}
	}
}

static ssize_t tfa98xx_dbgfs_start_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa98xx_control *calib = &(handles_local[tfa98xx->handle]
		.dev_ops.controls.calib);
	char *str;
	int ret;
#if defined(TFA_DBGFS_CHECK_MTPEX)
	unsigned short value;
#endif

	ret = wait_event_interruptible(tfa98xx->wq, calib->wr_value == false);

	if (ret == -ERESTARTSYS) {
		/* interrupted by signal */
		return ret;
	}

#if defined(TFA_DBGFS_CHECK_MTPEX)
	if (tfa98xx->calibrate_done && !calib->rd_valid) {
		/* calibration is ever done before */
		pr_debug("[0x%x] calibration is ever done before",
			tfa98xx->i2c->addr);
#if defined(TFA_CHECK_CALIBRATE_DONE)
		str = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		/* valid calibration data is stored in MTP */
		pr_info("[0x%x] Calibration succeeded (already done)\n",
			tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Success\n");
		ret = sizeof("Success");

		ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
		kfree(str);

		return ret;
#endif
	}
#endif

	if (!calib->rd_valid) {
		/* no calibration result available - skip */
		pr_debug("[0x%x] no valid calibration data is available",
			tfa98xx->i2c->addr);
#if defined(TFA_CHECK_CALIBRATE_DONE)
		str = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		/* valid calibration data is stored in MTP */
		pr_info("[0x%x] Calibration failed (not done yet)\n",
			tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "NoCalibration\n");
		ret = sizeof("NoCalibration");

		ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
		kfree(str);

		return ret;
#else
		return 0;
#endif
	}

#if !defined(TFA_CHECK_CALIBRATE_DONE)
	if (calib->rd_value) {
		/* Calibration already complete, return result */
		str = kmalloc(PAGE_SIZE, GFP_KERNEL);
		if (!str)
			return -ENOMEM;
		ret = print_calibration(tfa98xx->handle, str, PAGE_SIZE);
		if (ret < 0) {
			kfree(str);
			return ret;
		}
		ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

		pr_debug("[0x%x] %s", tfa98xx->i2c->addr, str);
		kfree(str);
		calib->rd_value = false;
	} else {
		/* Calibration failed, return the error code */
		const char estr[] = "-1\n";

		ret = copy_to_user(user_buf, estr, sizeof(estr));
		if (ret)
			return -EFAULT;
		ret = sizeof(estr);
	}
	calib->rd_valid = false;
#else
	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

#if defined(TFA_DBGFS_CHECK_MTPEX)
	if (!tfa98xx->calibrate_done) {
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_open(tfa98xx->handle);
		if (ret) {
			mutex_unlock(&tfa98xx->dsp_lock);
			kfree(str);
			return -EBUSY;
		}

		ret = tfa98xx_get_mtp(tfa98xx->handle, &value);

		tfa98xx_close(tfa98xx->handle);
		mutex_unlock(&tfa98xx->dsp_lock);

		if (!ret) {
			tfa98xx->calibrate_done =
				(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK)
				? 1 : 0;
			pr_debug("[0x%x] calibrate_done = MTPEX (%d)\n",
				tfa98xx->i2c->addr, tfa98xx->calibrate_done);

		} else {
			pr_debug("[0x%x] error in reading MTPEX\n",
				tfa98xx->i2c->addr);
			tfa98xx->calibrate_done = 0;
		}
	}
#endif

	if (tfa98xx->calibrate_done) {
		pr_info("[0x%x] Calibration Success\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Success\n");
		ret = sizeof("Success");
	} else {
		pr_info("[0x%x] Calibration Fail\n", tfa98xx->i2c->addr);
		snprintf(str, PAGE_SIZE, "Fail\n");
		ret = sizeof("Fail");
	}
	/* ret = sizeof(str); */

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);
	kfree(str);
#endif /* TFA_CHECK_CALIBRATE_DONE */

	return ret;
}

static ssize_t tfa98xx_dbgfs_start_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa98xx_error ret;
	const char ref[] = "1"; /* "please calibrate now" */
	char buf[32];
	int buf_size;
#if defined(TFA_DBGFS_CHECK_MTPEX)
	unsigned short value;
#endif

	pr_info("%s: begin\n", __func__);

	if (tfa98xx->pstream == 0) {
		pr_info("%s: Playback Fail. speaker init calibration Fail\n",
			__func__);
#if !defined(TFA_DBGFS_CHECK_MTPEX)
		tfa98xx->calibrate_done = 0;
#endif
		return count;
	}

#if defined(TFA_DBGFS_CHECK_MTPEX)
	if (!tfa98xx->calibrate_done) {
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_open(tfa98xx->handle);
		if (ret) {
			mutex_unlock(&tfa98xx->dsp_lock);
			return -EBUSY;
		}

		ret = tfa98xx_get_mtp(tfa98xx->handle, &value);

		tfa98xx_close(tfa98xx->handle);
		mutex_unlock(&tfa98xx->dsp_lock);

		if (!ret) {
			tfa98xx->calibrate_done =
				(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK)
				? 1 : 0;
			pr_debug("[0x%x] calibrate_done = MTPEX (%d)\n",
				tfa98xx->i2c->addr, tfa98xx->calibrate_done);

		} else {
			pr_debug("[0x%x] error in reading MTPEX\n",
				tfa98xx->i2c->addr);
			tfa98xx->calibrate_done = 0;
		}
	}

	if (tfa98xx->calibrate_done) {
		pr_info("[0x%x] Calibration is already done (Success)\n",
			tfa98xx->i2c->addr);
		return count;
	}
#endif

	/* check string length, and account for eol */
	if (count > sizeof(ref) + 1 || count < (sizeof(ref) - 1))
		return -EINVAL;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare string, excluding the trailing \0 and the potentials eol */
	if (strncmp(buf, ref, sizeof(ref) - 1))
		return -EINVAL;

	ret = tfa_run_calibrate(tfa98xx->handle, NULL);
	if (ret) {
		pr_info("[0x%x] calibration failed (%d), deferring...\n",
			tfa98xx->i2c->addr, ret);
	} else {
		pr_info("[0x%x] calibration succeeded\n", tfa98xx->i2c->addr);
	}

	pr_info("%s: end\n", __func__);

	return count;
}

static ssize_t tfa98xx_dbgfs_r_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;
	uint16_t status;
	int ret, calibrate_done;

	mutex_lock(&tfa98xx->dsp_lock);
	ret = tfa98xx_open(tfa98xx->handle);
	if (ret) {
		mutex_unlock(&tfa98xx->dsp_lock);
		return -EBUSY;
	}

	/* Need to ensure DSP is access-able, use mtp read access for this
	 * purpose
	 */
	ret = tfa98xx_get_mtp(tfa98xx->handle, &status);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] MTP read failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	ret = tfa_run_wait_calibration(tfa98xx->handle, &calibrate_done);
	if (ret) {
		ret = -EIO;
		pr_err("[0x%x] calibration failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	str = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		ret = -ENOMEM;
		pr_err("[0x%x] memory allocation failed\n", tfa98xx->i2c->addr);
		goto r_c_err;
	}

	switch (calibrate_done) {
	case 1:
		/* calibration complete ! */
		tfa_dsp_get_calibration_impedance(tfa98xx->handle);
		ret = print_calibration(tfa98xx->handle, str, PAGE_SIZE);
		break;
	case 0:
	case -1:
		ret = scnprintf(str, PAGE_SIZE, "%d\n", calibrate_done);
		break;
	default:
		pr_err("[0x%x] Unknown calibration status: %d\n",
			tfa98xx->i2c->addr, calibrate_done);
		ret = -EINVAL;
	}
	pr_debug("[0x%x] calib_done: %d - ret = %d - %s",
		tfa98xx->i2c->addr, calibrate_done, ret, str);

	if (ret < 0)
		goto r_err;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, ret);

r_err:
	kfree(str);
r_c_err:
	tfa98xx_close(tfa98xx->handle);
	mutex_unlock(&tfa98xx->dsp_lock);
	return ret;
}

enum tfa98xx_error
tfa_run_calibrate(tfa98xx_handle_t handle, uint16_t *value)
{
	struct tfa98xx *tfa98xx = tfa98xx_devices[handle];
	struct tfa98xx_control *calib = &(handles_local[handle]
					  .dev_ops.controls.calib);
	enum tfa98xx_error ret;
	int spkr_count, i;
#if defined(TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE)
	u64 otc_val = 1;
	int temp_val = 25;
#endif

	mutex_lock(&tfa98xx->dsp_lock);
#if !defined(TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE)
	/* Do not open/close tfa98xx: not required by tfa_calibrate */
	ret = tfa_calibrate(tfa98xx->handle);
#else
	ret = tfa98xx_open(tfa98xx->handle);
	if (ret) {
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_info("cannot open handle (%d)\n", ret);
		return -EBUSY;
	}

	/* OTC <0:always 1:once> */
	ret = tfa98xx_set_mtp(tfa98xx->handle,
		(otc_val << TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_POS)
		& TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK,
		TFA98XX_KEY2_PROTECTED_MTP0_MTPOTC_MSK);
	if (ret)
		pr_info("setting OTC failed (%d)\n", ret);
	/* MTPEX <reset to force to calibrate> */
	ret = tfa98xx_set_mtp(tfa98xx->handle, 0,
		TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK);
	if (ret)
		pr_info("resetting MTPEX failed (%d)\n", ret);

	/* EXT_TEMP */
#if defined(TFA_READ_BATTERY_TEMP)
	ret = tfa98xx_read_battery_temp(&temp_val);
	if (ret)
		pr_err("error in reading battery temp\n");
#endif
	tfa98xx_set_exttemp(tfa98xx->handle, (short)temp_val);

	/* reset stored calibration data */
	ret = tfa98xx_supported_speakers(handle, &spkr_count);
	if (ret == TFA98XX_ERROR_OK) {
		for (i = 0; i < spkr_count; i++)
			handles_local[tfa98xx->handle].mohm[i] = -1;
		handles_local[tfa98xx->handle].temp = temp_val;
	}

	/* run calibration */
	ret = tfa_tfadsp_calibrate(tfa98xx->handle);

	tfa98xx_close(tfa98xx->handle);
#endif /* TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE */
	mutex_unlock(&tfa98xx->dsp_lock);

#if !defined(TFA_FORCE_TO_WAIT_UNTIL_CALIBRATE)
	if (ret) {
		pr_info("[0x%x] Calibration start failed (%d), deferring...\n",
			tfa98xx->i2c->addr, ret);
		calib->triggered = true;
	} else {
		pr_info("[0x%x] Calibration started\n", tfa98xx->i2c->addr);
	}

	calib->wr_value = true;  /* request was triggered from here */
	calib->rd_valid = false; /* result not available */
	calib->rd_value = false; /* result not valid (default) */
#else
	if (ret) {
		pr_info("[0x%x] Calibration failed (%d), deferring...\n",
			tfa98xx->i2c->addr, ret);
		calib->triggered = true;
		tfa98xx->calibrate_done = 0;
		calib->wr_value = false; /* calibration over */
		calib->rd_valid = true; /* result available */
		calib->rd_value = false; /* result not valid */
	} else {
		pr_info("[0x%x] Calibration succeeded\n", tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 1;
		calib->wr_value = false; /* calibration over */
		calib->rd_valid = true; /* result available */
		calib->rd_value = true; /* result valid */
	}
#endif

	if (value != NULL)
		if ((calib->wr_value == false && calib->rd_valid == true)
			&& (TFA_GET_BF(tfa98xx->handle, MTPEX) == 1)) {
			*value = handles_local[tfa98xx->handle].mohm[0];
			if (*value == -1) {
				pr_info("%s: calibration data is not valid\n",
					__func__);
				*value = 0xffff;
				handles_local[tfa98xx->handle].temp = 0xffff;
				return -EINVAL;
			}
		}

	return ret;
}
EXPORT_SYMBOL(tfa_run_calibrate);

enum tfa98xx_error
tfa_read_calibrate(tfa98xx_handle_t handle, uint16_t *value)
{
	struct tfa98xx *tfa98xx = tfa98xx_devices[handle];
	struct tfa98xx_control *calib = &(handles_local[handle]
					  .dev_ops.controls.calib);
	enum tfa98xx_error ret;

	ret = wait_event_interruptible(tfa98xx->wq,
		calib->wr_value == false);
	if (ret == -ERESTARTSYS) {
		/* interrupted by signal */
		return ret;
	}

	if (!calib->rd_valid) {
		/* no calibration result available - skip */
		pr_err("no valid calibration data is available");
		return -EINVAL;
	}

	if (value != NULL)
		if (TFA_GET_BF(tfa98xx->handle, MTPEX) == 1) {
			*value = handles_local[tfa98xx->handle].mohm[0];
			if (*value == -1) {
				pr_info("%s: calibration data is not valid\n",
					__func__);
				return -EINVAL;
			}
		}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_read_calibrate);

enum tfa98xx_error
tfa_read_cal_temp(tfa98xx_handle_t handle, uint16_t *value)
{
	struct tfa98xx *tfa98xx = tfa98xx_devices[handle];
	struct tfa98xx_control *calib = &(handles_local[handle]
					  .dev_ops.controls.calib);
	enum tfa98xx_error ret;

	if (value == NULL)
		return TFA98XX_ERROR_FAIL;
	*value = 0xffff;

	ret = wait_event_interruptible(tfa98xx->wq,
		calib->wr_value == false);
	if (ret == -ERESTARTSYS) {
		/* interrupted by signal */
		return ret;
	}

	if (!calib->rd_valid) {
		/* no calibration result available - skip */
		pr_err("no valid calibration temp is available");
		return -EINVAL;
	}

	if (TFA_GET_BF(tfa98xx->handle, MTPEX) == 1) {
		*value = handles_local[tfa98xx->handle].temp;
		if (*value == 0xffff) {
			pr_info("%s: calibration data is not valid\n",
				__func__);
			return -EINVAL;
		}
	}

	return TFA98XX_ERROR_OK;
}
EXPORT_SYMBOL(tfa_read_cal_temp);

#if defined(TFA_READ_BATTERY_TEMP)
static enum tfa98xx_error tfa98xx_read_battery_temp(int *value)
{
	struct power_supply *psy;
	union power_supply_propval prop_read = {0};
	int ret = 0;

	/* get power supply of "battery" */
	/* value is preserved with default when error happens */
	psy = power_supply_get_by_name("battery");
	if (!psy) {
		pr_err("%s: failed to get power supply\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}
#if KERNEL_VERSION(4, 1, 0) > LINUX_VERSION_CODE
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &prop_read);
#else
	if (!psy->desc) {
		pr_err("%s: failed to get desc of power supply\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

	ret = psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP, &prop_read);
#endif
	if (!ret) {
		*value = (int)(prop_read.intval / 10); /* in degC */
		pr_info("%s: read battery temp (%d)\n", __func__, *value);
	} else {
		pr_err("%s: failed to get temp property\n", __func__);
		return TFA98XX_ERROR_FAIL;
	}

	return TFA98XX_ERROR_OK;
}
#endif /* TFA_READ_BATTERY_TEMP */

static ssize_t tfa98xx_dbgfs_version_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	char str[] = TFA98XX_VERSION "\n";
	int ret;

	ret = simple_read_from_buffer(user_buf, count, ppos, str, sizeof(str));

	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	switch (tfa98xx->dsp_init) {
	case TFA98XX_DSP_INIT_STOPPED:
		str = "Stopped\n";
		break;
	case TFA98XX_DSP_INIT_RECOVER:
		str = "Recover requested\n";
		break;
	case TFA98XX_DSP_INIT_FAIL:
		str = "Failed init\n";
		break;
	case TFA98XX_DSP_INIT_PENDING:
		str = "Pending init\n";
		break;
	case TFA98XX_DSP_INIT_DONE:
		str = "Init complete\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] dsp_state : %s\n", tfa98xx->i2c->addr, str);

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
	return ret;
}

static ssize_t tfa98xx_dbgfs_dsp_state_set(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	enum tfa_error ret;
	char buf[32];
	const char start_cmd[] = "start";
	const char stop_cmd[] = "stop";
	const char mon_start_cmd[] = "monitor start";
	const char mon_stop_cmd[] = "monitor stop";
	int buf_size;

	buf_size = min(count, (size_t)(sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	buf[buf_size] = 0;

	/* Compare strings, excluding the trailing \0 */
	if (!strncmp(buf, start_cmd, sizeof(start_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp start...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start
			(tfa98xx, tfa98xx_profile, tfa98xx_vsteps);
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_start complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, stop_cmd, sizeof(stop_cmd) - 1)) {
		pr_info("[0x%x] Manual triggering of dsp stop...\n",
			tfa98xx->i2c->addr);
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa_stop();
		mutex_unlock(&tfa98xx->dsp_lock);
		pr_debug("[0x%x] tfa_stop complete: %d\n",
			tfa98xx->i2c->addr, ret);
	} else if (!strncmp(buf, mon_start_cmd, sizeof(mon_start_cmd) - 1)) {
		pr_info("[0x%x] Manual start of monitor thread...\n",
			tfa98xx->i2c->addr);
		queue_delayed_work(tfa98xx->tfa98xx_wq,
					&tfa98xx->monitor_work, HZ);
	} else if (!strncmp(buf, mon_stop_cmd, sizeof(mon_stop_cmd) - 1)) {
		pr_info("[0x%x] Manual stop of monitor thread...\n",
			tfa98xx->i2c->addr);
		cancel_delayed_work_sync(&tfa98xx->monitor_work);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t tfa98xx_dbgfs_fw_state_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char *str;

	switch (tfa98xx->dsp_fw_state) {
	case TFA98XX_DSP_FW_NONE:
		str = "None\n";
		break;
	case TFA98XX_DSP_FW_PENDING:
		str = "Pending\n";
		break;
	case TFA98XX_DSP_FW_FAIL:
		str = "Fail\n";
		break;
	case TFA98XX_DSP_FW_OK:
		str = "Ok\n";
		break;
	default:
		str = "Invalid\n";
	}

	pr_debug("[0x%x] fw_state : %s", tfa98xx->i2c->addr, str);

	return simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));
}

static ssize_t tfa98xx_dbgfs_accounting_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	char str[255];
	int ret;
	int n = 0;

	n += snprintf(&str[n], sizeof(str)-1-n, "Wait4Src\t= %d\n",
		tfa98xx->count_wait_for_source_state);
	n += snprintf(&str[n], sizeof(str)-1-n, "NOCLK\t\t= %d\n",
		tfa98xx->count_noclk);

	str[n+1] = '\0'; /* in case str is not large enough */

	ret = simple_read_from_buffer(user_buf, count, ppos, str, n+1);

	return ret;
}

/* ++ RPC message fops */
static ssize_t tfa98xx_dbgfs_rpc_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum tfa98xx_error error;

	if (tfa98xx->handle < 0) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa98xx->handle, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	ret = simple_read_from_buffer(user_buf, count, ppos, buffer, count);

	kfree(buffer);

	return ret;
}

static ssize_t tfa98xx_dbgfs_rpc_send(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	struct tfa_file_dsc *msg_file;
	enum tfa98xx_error error;
	int ret = 0;

	if (tfa98xx->handle < 0) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	/* msg_file.name is not used */
	msg_file = kmalloc(count + sizeof(struct tfa_file_dsc), GFP_KERNEL);
	if (msg_file == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}
	msg_file->size = (uint32_t)count;

	if (copy_from_user(msg_file->data, user_buf, count)) {
		kfree(msg_file);
		return -EFAULT;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	if ((msg_file->data[0] == 'M') && (msg_file->data[1] == 'G')) {
		error = tfa_cont_write_file(tfa98xx->handle, msg_file, 0, 0);
		/* int vstep_idx, int vstep_msg_idx both 0 */
		if (error)
			pr_debug("[0x%x] tfa_cont_write_file error: %d\n",
				tfa98xx->i2c->addr, error);
	} else {
		error = dsp_msg
			(tfa98xx->handle, msg_file->size, msg_file->data);
		if (error)
			pr_debug("[0x%x] dsp_msg error: %d\n",
				tfa98xx->i2c->addr, error);
	}
	mutex_unlock(&tfa98xx->dsp_lock);

	kfree(msg_file);

	return count;
}
/* -- RPC */

/* ++ DSP message fops */
static ssize_t tfa98xx_dbgfs_dsp_read(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum tfa98xx_error error;

	if (tfa98xx->handle < 0) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg_read(tfa98xx->handle, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg_read error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	/* ret = simple_read_from_buffer
	 * (user_buf, count, ppos, buffer, count);
	 */
	ret = copy_to_user(user_buf, buffer, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer to user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}

static ssize_t tfa98xx_dbgfs_dsp_write(struct file *file,
	const char __user *user_buf,
	size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	uint8_t *buffer;
	enum tfa98xx_error error;

	if (tfa98xx->handle < 0) {
		pr_debug("[0x%x] dsp is not available\n", tfa98xx->i2c->addr);
		return -ENODEV;
	}

	if (count == 0)
		return 0;

	buffer = kmalloc(count, GFP_KERNEL);
	if (buffer == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return  ret;
	}

	ret = copy_from_user(buffer, user_buf, count);
	if (ret) {
		pr_debug("[0x%x] cannot copy buffer from user: %d\n",
			tfa98xx->i2c->addr, ret);
		kfree(buffer);
		return -EFAULT;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	error = dsp_msg(tfa98xx->handle, count, buffer);
	mutex_unlock(&tfa98xx->dsp_lock);
	if (error) {
		pr_debug("[0x%x] dsp_msg error: %d\n",
			tfa98xx->i2c->addr, error);
		kfree(buffer);
		return -EFAULT;
	}

	kfree(buffer);

	return count;
}
/* -- DSP */

static ssize_t tfa98xx_dbgfs_spkr_damaged_get(struct file *file,
	char __user *user_buf, size_t count, loff_t *ppos)
{
	struct i2c_client *i2c = file->private_data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int ret = 0;
	char *str;

	str = kmalloc(count, GFP_KERNEL);
	if (str == NULL) {
		ret = -ENOMEM;
		pr_debug("[0x%x] can not allocate memory\n",
			tfa98xx->i2c->addr);
		return ret;
	}

	scnprintf(str, PAGE_SIZE, "%s\n",
		(handles_local[tfa98xx->handle].spkr_damaged == 1)
		? "damaged" : "ready");

	ret = simple_read_from_buffer(user_buf, count, ppos, str, strlen(str));

	kfree(str);

	return ret;
}

static int tfa98xx_dbgfs_pga_gain_get(void *data, u64 *val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int err;
	unsigned int value;

	/* *val = TFA_GET_BF(tfa98xx->handle, SAAMGAIN); */
	err = regmap_read(tfa98xx->regmap, TFA98XX_CTRL_SAAM_PGA, &value);
	*val = (u64)((value & TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_MSK)
		>> TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_POS);
	return 0;
}

static int tfa98xx_dbgfs_pga_gain_set(void *data, u64 val)
{
	struct i2c_client *i2c = (struct i2c_client *)data;
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);
	int err;
	unsigned int value;

	value = val & 0xffff;
	if (value > 7)
		return -EINVAL;
/*	TFA_SET_BF(tfa98xx->handle, SAAMGAIN, value);*/
	err = regmap_update_bits(tfa98xx->regmap, TFA98XX_CTRL_SAAM_PGA,
		TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_MSK,
		value << TFA98XX_CTRL_SAAM_PGA_SAAMGAIN_POS);
	return err;
}

/* Direct registers access - provide register address in hex */
#define TFA98XX_DEBUGFS_REG_SET(__reg)					\
static int tfa98xx_dbgfs_reg_##__reg##_set(void *data, u64 val)		\
{									\
	struct i2c_client *i2c = (struct i2c_client *)data;		\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);		\
	unsigned int ret, value;					\
									\
	ret = regmap_write(tfa98xx->regmap, 0x##__reg, (val & 0xffff));	\
	value = val & 0xffff;						\
	return 0;							\
}									\
static int tfa98xx_dbgfs_reg_##__reg##_get(void *data, u64 *val)	\
{									\
	struct i2c_client *i2c = (struct i2c_client *)data;		\
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);		\
	unsigned int value;						\
	int ret;							\
									\
	ret = regmap_read(tfa98xx->regmap, 0x##__reg, &value);		\
	*val = value;							\
	return 0;							\
}									\
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_reg_##__reg##_fops, \
			tfa98xx_dbgfs_reg_##__reg##_get, \
			tfa98xx_dbgfs_reg_##__reg##_set, "0x%llx\n")

#define VAL(str) #str
#define TOSTRING(str) VAL(str)

#define TFA98XX_DEBUGFS_REG_CREATE_FILE(__reg, __name)			\
	debugfs_create_file(TOSTRING(__reg) "-" TOSTRING(__name),	\
			    0666, dbg_reg_dir,		\
			    i2c, &tfa98xx_dbgfs_reg_##__reg##_fops)

TFA98XX_DEBUGFS_REG_SET(00);
TFA98XX_DEBUGFS_REG_SET(01);
TFA98XX_DEBUGFS_REG_SET(02);
TFA98XX_DEBUGFS_REG_SET(03);
TFA98XX_DEBUGFS_REG_SET(04);
TFA98XX_DEBUGFS_REG_SET(05);
TFA98XX_DEBUGFS_REG_SET(06);
TFA98XX_DEBUGFS_REG_SET(07);
TFA98XX_DEBUGFS_REG_SET(08);
TFA98XX_DEBUGFS_REG_SET(09);
TFA98XX_DEBUGFS_REG_SET(0A);
TFA98XX_DEBUGFS_REG_SET(0B);
TFA98XX_DEBUGFS_REG_SET(0F);
TFA98XX_DEBUGFS_REG_SET(10);
TFA98XX_DEBUGFS_REG_SET(11);
TFA98XX_DEBUGFS_REG_SET(12);
TFA98XX_DEBUGFS_REG_SET(13);
TFA98XX_DEBUGFS_REG_SET(22);
TFA98XX_DEBUGFS_REG_SET(25);

DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_otc_fops,
			tfa98xx_dbgfs_otc_get,
			tfa98xx_dbgfs_otc_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_mtpex_fops,
			tfa98xx_dbgfs_mtpex_get,
			tfa98xx_dbgfs_mtpex_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_calib_temp_fops,
			tfa98xx_dbgfs_temp_get,
			tfa98xx_dbgfs_temp_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(tfa98xx_dbgfs_pga_gain_fops,
			tfa98xx_dbgfs_pga_gain_get,
			tfa98xx_dbgfs_pga_gain_set, "%llu\n");

static const struct file_operations tfa98xx_dbgfs_calib_start_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_start_get,
	.write = tfa98xx_dbgfs_start_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_r_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_r_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_version_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_version_read,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_state_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_state_get,
	.write = tfa98xx_dbgfs_dsp_state_set,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_fw_state_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_fw_state_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_accounting_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_accounting_get,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_rpc_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_rpc_read,
	.write = tfa98xx_dbgfs_rpc_send,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_dsp_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_dsp_read,
	.write = tfa98xx_dbgfs_dsp_write,
	.llseek = default_llseek,
};

static const struct file_operations tfa98xx_dbgfs_spkr_damaged_fops = {
	.open = simple_open,
	.read = tfa98xx_dbgfs_spkr_damaged_get,
	.llseek = default_llseek,
};

static void tfa98xx_debug_init(struct tfa98xx *tfa98xx, struct i2c_client *i2c)
{
	char name[50];
	struct dentry *dbg_reg_dir;

	scnprintf(name, MAX_CONTROL_NAME, "%s-%x", i2c->name, i2c->addr);
	tfa98xx->dbg_dir = debugfs_create_dir(name, NULL);
	debugfs_create_file("OTC", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_calib_otc_fops);
	debugfs_create_file("MTPEX", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_calib_mtpex_fops);
	debugfs_create_file("TEMP", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_calib_temp_fops);
	debugfs_create_file("calibrate", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_calib_start_fops);
	debugfs_create_file("R", 0444,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_r_fops);
	debugfs_create_file("version", 0444,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_version_fops);
	debugfs_create_file("dsp-state", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_dsp_state_fops);
	debugfs_create_file("fw-state", 0664,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_fw_state_fops);
	debugfs_create_file("accounting", 0444,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_accounting_fops);
	debugfs_create_file("rpc", 0444,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_rpc_fops);
	debugfs_create_file("dsp", 0644,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_dsp_fops);
	debugfs_create_file("spkr-state", 0444,
		tfa98xx->dbg_dir, i2c, &tfa98xx_dbgfs_spkr_damaged_fops);

	/* Direct registers access */
	if (tfa98xx->flags & TFA98XX_FLAG_TFA9890_FAM_DEV) {
		dbg_reg_dir = debugfs_create_dir("regs", tfa98xx->dbg_dir);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(00, STATUS);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(01, BATTERYVOLTAGE);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(02, TEMPERATURE);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(03, REVISIONNUMBER);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(04, I2SREG);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(05, BAT_PROT);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(06, AUDIO_CTR);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(07, DCDCBOOST);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(08, SPKR_CALIBRATION);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(09, SYS_CTRL);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(0A, I2S_SEL_REG);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(0B, HIDDEN_MTP_KEY2);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(0F, INTERRUPT_REG);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(10, PDM_CTRL);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(11, PDM_OUT_CTRL);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(12, PDM_DS4_R);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(13, PDM_DS4_L);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(22, CTRL_SAAM_PGA);
		TFA98XX_DEBUGFS_REG_CREATE_FILE(25, MISC_CTRL);
	}

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		dev_dbg(tfa98xx->dev, "Adding pga_gain debug interface\n");
		debugfs_create_file("pga_gain", 0444, tfa98xx->dbg_dir,
						tfa98xx->i2c,
						&tfa98xx_dbgfs_pga_gain_fops);
	}
}

static void tfa98xx_debug_remove(struct tfa98xx *tfa98xx)
{
	debugfs_remove_recursive(tfa98xx->dbg_dir);
}
#endif

static int tfa98xx_get_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int mixer_profile = kcontrol->private_value;
	int profile, vstep;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}
	vstep = tfa98xx_prof_vsteps[profile];

	ucontrol->value.integer.value[0] =
				tfa_cont_get_max_vstep(tfa98xx->handle, profile)
				- vstep - 1;
	return 0;
}

static int tfa98xx_set_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int mixer_profile = kcontrol->private_value;
	int value = ucontrol->value.integer.value[0];
	int profile, vstep, vsteps;
	int new_vstep, err = 0;

	if (no_start != 0)
		return 0;

	profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);
	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}
	vstep = tfa98xx_prof_vsteps[profile];
	vsteps = tfa_cont_get_max_vstep(tfa98xx->handle, profile);

	if (vstep == vsteps - value - 1)
		return 0;

	new_vstep = vsteps - value - 1;

	if (new_vstep < 0)
		new_vstep = 0;

	tfa98xx_prof_vsteps[profile] = new_vstep;

#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
	if (profile == tfa98xx_profile) {
#endif
		/* this is the active profile, program the new vstep */
		tfa98xx_vsteps[0] = new_vstep;
		tfa98xx_vsteps[1] = new_vstep;

		/* wait until when DSP is ready for initialization */
		if (tfa98xx->pstream != 0 || tfa98xx->samstream != 0) {
			mutex_lock(&tfa98xx->dsp_lock);
			tfa98xx_open(tfa98xx->handle);
			tfa98xx_close(tfa98xx->handle);

			err = tfa98xx_tfa_start
				(tfa98xx, profile, tfa98xx_vsteps);
			if (err)
				pr_err("Write vstep error: %d\n", err);
			else
				pr_debug("Successfully changed vstep index!\n");

			mutex_unlock(&tfa98xx->dsp_lock);
		} else {
			pr_info("%s: tfa_start is suspended when only cstream is on\n",
				__func__);
		}
#ifndef TFA98XX_ALSA_CTRL_PROF_CHG_ON_VOL
	}
#endif

	pr_debug("vstep:%d, (control value: %d) - profile %d\n",
		 new_vstep, value, profile);
	return (err == 0);
}

static int tfa98xx_info_vstep(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int mixer_profile = kcontrol->private_value;
	int profile = get_profile_id_for_sr(mixer_profile, tfa98xx->rate);

	if (profile < 0) {
		pr_err("%s: invalid profile %d (mixer_profile=%d, rate=%d)\n",
			__func__, profile, mixer_profile, tfa98xx->rate);
		return -EINVAL;
	}

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1; /* TODO handles_local[dev_idx].spkr_count */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = max
		(0, tfa_cont_get_max_vstep(tfa98xx->handle, profile) - 1);
	pr_debug("vsteps count: %d [prof=%d]\n",
		tfa_cont_get_max_vstep(tfa98xx->handle, profile), profile);

	return 0;
}

static int tfa98xx_get_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = tfa98xx_mixer_profile;
	return 0;
}

static int tfa98xx_set_profile(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	int profile_count = tfa98xx_mixer_profiles;
	int profile = tfa98xx_mixer_profile;
	int new_profile = ucontrol->value.integer.value[0];
	int err;
	int prof_idx;

	if (no_start != 0)
		return 0;

	if (new_profile == profile)
		return 0;

	if (new_profile >= profile_count)
		return 0;

	/* get the container profile for the requested sample rate */
	prof_idx = get_profile_id_for_sr(new_profile, tfa98xx->rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: sample rate [%d] not supported for this mixer profile [%d].\n",
			tfa98xx->rate, new_profile);
		return 0;
	}
	pr_debug("selected container profile [%d]\n", prof_idx);

	/* update mixer profile */
	tfa98xx_mixer_profile = new_profile;

	/* update 'real' profile (container profile) */
	tfa98xx_profile = prof_idx;
	tfa98xx_vsteps[0] = tfa98xx_prof_vsteps[prof_idx];
	tfa98xx_vsteps[1] = tfa98xx_prof_vsteps[prof_idx];

	/*
	 * Don't call tfa_start() on TFA1 if there is no clock.
	 * For TFA2 is able to load the profile without clock.
	 */

	/* wait until when DSP is ready for initialization */
	if (tfa98xx->pstream != 0 || tfa98xx->samstream != 0) {
		mutex_lock(&tfa98xx->dsp_lock);
		tfa98xx_open(tfa98xx->handle);
		tfa98xx_close(tfa98xx->handle);

		/* Also re-enables the interrupts */
		err = tfa98xx_tfa_start(tfa98xx, prof_idx, tfa98xx_vsteps);
		if (err) {
			pr_info("Write profile error: %d\n", err);
		} else {
			pr_debug("Changed to profile %d (vstep = %d)\n",
				prof_idx, tfa98xx_vsteps[0]);
		}

		mutex_unlock(&tfa98xx->dsp_lock);
	} else {
		pr_info("%s: tfa_start is suspended when only cstream is on\n",
			__func__);
		return 1;
	}

	/* Flag DSP as invalidated as the profile change may invalidate the
	 * current DSP configuration. That way, further stream start can
	 * trigger a tfa_start.
	 */
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_INVALIDATED;

	return 1;
}

static struct snd_kcontrol_new *tfa98xx_controls;

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

/* return the profile name accociated with id from the profile list */
static int get_profile_from_list(char *buf, int id)
{
	struct tfa98xx_baseprofile *bprof;

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
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (strncmp(bprof->basename, profile, len) == 0)
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
	struct tfa98xx_baseprofile *bprof;

	list_for_each_entry(bprof, &profile_list, list) {
		if (id == bprof->item_id) {
			idx = tfa98xx_get_fssel(rate);
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
static int is_calibration_profile(char *profile)
{
	if (strnstr(profile, ".cal", strlen(profile)) != NULL)
		return 1;
	return 0;
}

/*
 * adds the (container)profile index of the samplerate found in
 * the (container)profile to a fixed samplerate table in the (mixer)profile
 */
static int add_sr_to_profile(struct tfa98xx *tfa98xx,
	char *basename, int len, int profile)
{
	struct tfa98xx_baseprofile *bprof;
	int idx = 0;
	unsigned int sr = 0;

	list_for_each_entry(bprof, &profile_list, list) {
		if (strncmp(bprof->basename, basename, len) == 0) {
			/* add supported samplerate for this profile */
			sr = tfa98xx_get_profile_sr(tfa98xx->handle, profile);
			if (!sr) {
				pr_err("unable to identify supported sample rate for %s\n",
					bprof->basename);
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
			idx = tfa98xx_get_fssel(sr);
			if (idx < 0 || idx >= TFA98XX_NUM_RATES) {
				pr_err("invalid index for samplerate %d\n",
					idx);
				return TFA_ERROR;
			}

			/* enter the (container)profile for this samplerate
			 * at the corresponding index
			 */
			bprof->sr_rate_sup[idx] = profile;

			pr_debug("added profile:samplerate = [%d:%d] for mixer profile: %s\n",
				profile, sr, bprof->basename);
		}
	}

	return 0;
}

static int tfa98xx_info_profile(struct snd_kcontrol *kcontrol,
			 struct snd_ctl_elem_info *uinfo)
{
	char profile_name[MAX_CONTROL_NAME] = {0};
	int count = tfa98xx_mixer_profiles, err = -1;

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = count;

	if (uinfo->value.enumerated.item >= count)
		uinfo->value.enumerated.item = count - 1;

	err = get_profile_from_list(profile_name,
		uinfo->value.enumerated.item);
	if (err != 0)
		return -EINVAL;

	strlcpy(uinfo->value.enumerated.name,
		profile_name, MAX_CONTROL_NAME);

	return 0;
}

static int tfa98xx_get_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int tfa98xx_set_stop_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int ready = 0;

	pr_debug("%ld\n", ucontrol->value.integer.value[0]);

	tfa98xx_open(tfa98xx->handle);
	tfa98xx_close(tfa98xx->handle);

	if ((ucontrol->value.integer.value[0] != 0) && ready) {
		cancel_delayed_work_sync(&tfa98xx->monitor_work);

		cancel_delayed_work_sync(&tfa98xx->init_work);
		if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
			return 0;
		mutex_lock(&tfa98xx->dsp_lock);
		tfa_stop();
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
		mutex_unlock(&tfa98xx->dsp_lock);
	}

	ucontrol->value.integer.value[0] = 0;
	return 1;
}

static int tfa98xx_info_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xffff; /* 16 bit value */

	return 0;
}

static int tfa98xx_set_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	enum tfa98xx_error err;
	int status = 0;

	tfa98xx->cal_data = (uint16_t)ucontrol->value.integer.value[0];

	tfa98xx_open(tfa98xx->handle);
	tfa98xx_dsp_system_stable(tfa98xx->handle, &status);
	if (status)
		err = tfa_mtp_set_calibration
			(tfa98xx->handle, tfa98xx->cal_data);
	else
		err = TFA98XX_ERROR_NO_CLOCK;
	tfa98xx_close(tfa98xx->handle);

	tfa98xx->set_mtp_cal = (err != TFA98XX_ERROR_OK);
	if (tfa98xx->set_mtp_cal == false) {
		pr_info("Calibration value (%d) set in mtp\n",
			tfa98xx->cal_data);
	}

	return 1;
}

static int tfa98xx_get_cal_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	uint16_t cal_data;

	tfa98xx_open(tfa98xx->handle);
	tfa_mtp_get_calibration(tfa98xx->handle, &cal_data);
	tfa98xx_close(tfa98xx->handle);

	ucontrol->value.integer.value[0] = cal_data;
	return 0;
}

static int tfa98xx_set_saam_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int saam_select = ucontrol->value.integer.value[0];

	dev_dbg(&tfa98xx->i2c->dev, "%s: state: %d\n", __func__, saam_select);

	pr_info("%s: trigger tfa amp for SaaM (%s)\n", __func__,
		saam_select ? "start" : "stop");
	/* saam_select = 1: mute = 0 to enable SaaM
	 * saam_select = 2: mute = 0 to enable SaaM and playback, concurrently
	 * saam_select = 0: mute = 1 to disable SaaM
	 */
	tfa98xx->samstream = saam_select;
	tfa98xx_set_saam_use_case(saam_select);

	_tfa98xx_mute(tfa98xx, saam_select ? 0 : 1, SNDRV_PCM_STREAM_SAAM);

	return 0;
}

static int tfa98xx_get_saam_ctl(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tfa98xx->cstream;
	return 0;
}

static int tfa98xx_create_controls(struct tfa98xx *tfa98xx)
{
	int prof, nprof, mix_index = 0;
	int nr_controls = 0, id = 0;
	char *name;
	struct tfa98xx_baseprofile *bprofile;

	/* Create the following controls:
	 *  - enum control to select the active profile
	 *  - one volume control for each profile hosting a vstep
	 *  - Stop control on TFA1 devices
	 */

	nr_controls = 1;	 /* Profile control */
	if (tfa98xx_dev_family(tfa98xx->handle) == 1)
		nr_controls += 1; /* Stop control */

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL)
		nr_controls += 1; /* calibration */

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE)
		nr_controls += 1; /* SaaM */

	/* allocate the tfa98xx_controls base on the nr of profiles */
	nprof = tfa_cont_max_profile(tfa98xx->handle);

	for (prof = 0; prof < nprof; prof++) {
		if (tfa_cont_get_max_vstep(tfa98xx->handle, prof))
			nr_controls++; /* Playback Volume control */
	}

	tfa98xx_controls = devm_kzalloc(tfa98xx->codec->dev,
			nr_controls * sizeof(tfa98xx_controls[0]), GFP_KERNEL);
	if (!tfa98xx_controls)
		return -ENOMEM;

	/* Create a mixer item for selecting the active profile */
	name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	scnprintf(name, MAX_CONTROL_NAME, "%s Profile", tfa98xx->fw.name);
	tfa98xx_controls[mix_index].name = name;
	tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	tfa98xx_controls[mix_index].info = tfa98xx_info_profile;
	tfa98xx_controls[mix_index].get = tfa98xx_get_profile;
	tfa98xx_controls[mix_index].put = tfa98xx_set_profile;
	/* tfa98xx_controls[mix_index].private_value = profs; */
	/* save number of profiles */
	mix_index++;

	/* create mixer items for each profile that has volume */
	for (prof = 0; prof < nprof; prof++) {
		/* create an new empty profile */
		bprofile = devm_kzalloc
			(tfa98xx->codec->dev, sizeof(*bprofile), GFP_KERNEL);
		if (!bprofile)
			return -ENOMEM;

		bprofile->len = 0;
		bprofile->item_id = -1;
		INIT_LIST_HEAD(&bprofile->list);

		/* copy profile name into basename until the . */
		get_profile_basename(bprofile->basename,
			tfa_cont_profile_name(tfa98xx->handle, prof));
		bprofile->len = strlen(bprofile->basename);

		/*
		 * search the profile list for a profile with basename,
		 * if it is not found then add it to the list and
		 * add a new mixer control (if it has vsteps)
		 * also, if it is a calibration profile,
		 * do not add it to the list
		 */
		if (is_profile_in_list(bprofile->basename, bprofile->len) == 0
			&& is_calibration_profile
			(tfa_cont_profile_name(tfa98xx->handle, prof)) == 0) {
			/* the profile is not present, add it to the list */
			list_add(&bprofile->list, &profile_list);
			bprofile->item_id = id++;

			pr_debug("profile added [%d]: %s\n",
				bprofile->item_id, bprofile->basename);

			if (tfa_cont_get_max_vstep(tfa98xx->handle, prof)) {
				name = devm_kzalloc
					(tfa98xx->codec->dev, MAX_CONTROL_NAME,
					GFP_KERNEL);
				if (!name)
					return -ENOMEM;

				scnprintf(name, MAX_CONTROL_NAME,
					"%s %s Playback Volume",
				tfa98xx->fw.name, bprofile->basename);

				tfa98xx_controls[mix_index].name = name;
				tfa98xx_controls[mix_index].iface =
					SNDRV_CTL_ELEM_IFACE_MIXER;
				tfa98xx_controls[mix_index].info =
					tfa98xx_info_vstep;
				tfa98xx_controls[mix_index].get =
					tfa98xx_get_vstep;
				tfa98xx_controls[mix_index].put =
					tfa98xx_set_vstep;
				tfa98xx_controls[mix_index].private_value =
					bprofile->item_id;
				/* save profile index */
				mix_index++;
			}
		}

		/* look for the basename profile in the list of mixer profiles
		 * and add the container profile index
		 * to the supported samplerates of this mixer profile
		 */
		add_sr_to_profile(tfa98xx, bprofile->basename,
			bprofile->len, prof);
	}

	/* set the number of user selectable profiles in the mixer */
	tfa98xx_mixer_profiles = id;

	if (tfa98xx_dev_family(tfa98xx->handle) == 1) {
		/* Create a mixer item for stop control on TFA1 */
		name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME,
			GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME, "%s Stop", tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = snd_soc_info_bool_ext;
		tfa98xx_controls[mix_index].get = tfa98xx_get_stop_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_stop_ctl;
		mix_index++;
	}

	if (tfa98xx->flags & TFA98XX_FLAG_CALIBRATION_CTL) {
		name = devm_kzalloc(tfa98xx->codec->dev, MAX_CONTROL_NAME,
			GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME, "%s Calibration",
			tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = tfa98xx_info_cal_ctl;
		tfa98xx_controls[mix_index].get = tfa98xx_get_cal_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_cal_ctl;
		mix_index++;
	}

	/* Create a mixer item to enable amplifier for RaM / SaM */
	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		name = devm_kzalloc
			(tfa98xx->codec->dev, MAX_CONTROL_NAME, GFP_KERNEL);
		if (!name)
			return -ENOMEM;

		scnprintf(name, MAX_CONTROL_NAME, "%s SaaM", tfa98xx->fw.name);
		tfa98xx_controls[mix_index].name = name;
		tfa98xx_controls[mix_index].iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		tfa98xx_controls[mix_index].info = snd_soc_info_bool_ext;
		tfa98xx_controls[mix_index].get = tfa98xx_get_saam_ctl;
		tfa98xx_controls[mix_index].put = tfa98xx_set_saam_ctl;
		/* tfa98xx_controls[mix_index].private_value = profs; */
		/* save number of profiles */
		mix_index++;
	}

	return snd_soc_add_codec_controls(tfa98xx->codec,
		tfa98xx_controls, mix_index);
}

static void *tfa98xx_devm_kstrdup(struct device *dev, char *buf)
{
	char *str = devm_kzalloc(dev, strlen(buf) + 1, GFP_KERNEL);

	if (!str)
		return str;
	memcpy(str, buf, strlen(buf));

	return str;
}

static int tfa98xx_append_i2c_address(struct device *dev,
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
			snprintf(buf, 50, "%s-%x-%x", dai_drv[i].name,
				i2cbus, addr);
			dai_drv[i].name = tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].name=%s\n", i, dai_drv[i].name);

			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].playback.stream_name,
				i2cbus, addr);
			dai_drv[i].playback.stream_name =
				tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].playback.stream_name=%s\n",
				i, dai_drv[i].playback.stream_name);

			snprintf(buf, 50, "%s-%x-%x",
				dai_drv[i].capture.stream_name,
				i2cbus, addr);
			dai_drv[i].capture.stream_name =
				tfa98xx_devm_kstrdup(dev, buf);
			pr_info("dai_drv[%d].capture.stream_name=%s\n",
				i, dai_drv[i].capture.stream_name);
		}

	/* the idea behind this is convert:
	 * SND_SOC_DAPM_AIF_IN
	 *   ("AIF IN","AIF Playback",0,SND_SOC_NOPM,0,0),
	 * into:
	 * SND_SOC_DAPM_AIF_IN
	 *   ("AIF IN","AIF Playback-2-36",0,SND_SOC_NOPM,0,0),
	 */
	if (widgets && num_widgets > 0)
		for (i = 0; i < num_widgets; i++) {
			if (!widgets[i].sname)
				continue;
			if ((widgets[i].id == snd_soc_dapm_aif_in)
				|| (widgets[i].id == snd_soc_dapm_aif_out)) {
				snprintf(buf, 50, "%s-%x-%x",
					widgets[i].sname, i2cbus, addr);
				widgets[i].sname =
					tfa98xx_devm_kstrdup(dev, buf);
				pr_info("widgets[%d].sname=%s\n",
					i, widgets[i].sname);
			}
		}

	return 0;
}

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_common[] = {
	/* Stream widgets */
	SND_SOC_DAPM_AIF_IN("AIF IN", "AIF Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("AIF OUT", "AIF Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("OUTL"),
	SND_SOC_DAPM_INPUT("AEC Loopback"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_stereo[] = {
	SND_SOC_DAPM_OUTPUT("OUTR"),
};

static struct snd_soc_dapm_widget tfa98xx_dapm_widgets_saam[] = {
	SND_SOC_DAPM_INPUT("SAAM MIC"),
};

static struct snd_soc_dapm_widget tfa9888_dapm_inputs[] = {
	SND_SOC_DAPM_INPUT("DMIC1"),
	SND_SOC_DAPM_INPUT("DMIC2"),
	SND_SOC_DAPM_INPUT("DMIC3"),
	SND_SOC_DAPM_INPUT("DMIC4"),
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_common[] = {
	{ "OUTL", NULL, "AIF IN" },
	{ "AIF OUT", NULL, "AEC Loopback" },
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_saam[] = {
	{ "AIF OUT", NULL, "SAAM MIC" },
};

static const struct snd_soc_dapm_route tfa98xx_dapm_routes_stereo[] = {
	{ "OUTR", NULL, "AIF IN" },
};

static const struct snd_soc_dapm_route tfa9888_input_dapm_routes[] = {
	{ "AIF OUT", NULL, "DMIC1" },
	{ "AIF OUT", NULL, "DMIC2" },
	{ "AIF OUT", NULL, "DMIC3" },
	{ "AIF OUT", NULL, "DMIC4" },
};

static void tfa98xx_add_widgets(struct tfa98xx *tfa98xx)
{
#if KERNEL_VERSION(4, 2, 0) > LINUX_VERSION_CODE
	struct snd_soc_dapm_context *dapm
		= &tfa98xx->codec->dapm;
#else
	struct snd_soc_dapm_context *dapm
		= snd_soc_codec_get_dapm(tfa98xx->codec);
#endif
	struct snd_soc_dapm_widget *widgets;
	unsigned int num_dapm_widgets =
		ARRAY_SIZE(tfa98xx_dapm_widgets_common);

	widgets = devm_kzalloc(&tfa98xx->i2c->dev,
			sizeof(struct snd_soc_dapm_widget) *
				ARRAY_SIZE(tfa98xx_dapm_widgets_common),
			GFP_KERNEL);
	if (!widgets)
		return;
	memcpy(widgets, tfa98xx_dapm_widgets_common,
			sizeof(struct snd_soc_dapm_widget) *
				ARRAY_SIZE(tfa98xx_dapm_widgets_common));

	tfa98xx_append_i2c_address(&tfa98xx->i2c->dev,
				tfa98xx->i2c,
				widgets,
				num_dapm_widgets,
				NULL,
				0);

	snd_soc_dapm_new_controls(dapm, widgets,
		ARRAY_SIZE(tfa98xx_dapm_widgets_common));
	snd_soc_dapm_add_routes(dapm, tfa98xx_dapm_routes_common,
		ARRAY_SIZE(tfa98xx_dapm_routes_common));

	if (tfa98xx->flags & TFA98XX_FLAG_STEREO_DEVICE) {
		snd_soc_dapm_new_controls
			(dapm, tfa98xx_dapm_widgets_stereo,
			ARRAY_SIZE(tfa98xx_dapm_widgets_stereo));
		snd_soc_dapm_add_routes
			(dapm, tfa98xx_dapm_routes_stereo,
			ARRAY_SIZE(tfa98xx_dapm_routes_stereo));
	}

	if (tfa98xx->flags & TFA98XX_FLAG_MULTI_MIC_INPUTS) {
		snd_soc_dapm_new_controls
			(dapm, tfa9888_dapm_inputs,
			ARRAY_SIZE(tfa9888_dapm_inputs));
		snd_soc_dapm_add_routes
			(dapm, tfa9888_input_dapm_routes,
			ARRAY_SIZE(tfa9888_input_dapm_routes));
	}

	if (tfa98xx->flags & TFA98XX_FLAG_SAAM_AVAILABLE) {
		snd_soc_dapm_new_controls
			(dapm, tfa98xx_dapm_widgets_saam,
			ARRAY_SIZE(tfa98xx_dapm_widgets_saam));
		snd_soc_dapm_add_routes
			(dapm, tfa98xx_dapm_routes_saam,
			ARRAY_SIZE(tfa98xx_dapm_routes_saam));
	}
}

/* Match tfa98xx device structure with a valid DSP handle */
/* TODO can be removed once we pass the device struct in stead of handles
 *	The check in tfa98xx_register_dsp() is implicitly done
 *	in tfa_probe() /tfa98xx_cnt_slave2idx(_)
 */
static int tfa98xx_register_dsp(struct tfa98xx *tfa98xx)
{
	int i, handle = -1;
	u8 slave;

	for (i = 0; i < tfa98xx_cnt_max_device(); i++) {
		if (tfa_cont_get_slave(i, &slave) != TFA98XX_ERROR_OK)
			goto reg_err;
		pr_debug("%s: i=%d - dev = 0x%x\n", __func__, i, slave);
		if (slave == tfa98xx->i2c->addr) {
			handle = i;
			break;
		}
	}
	if (handle != -1) {
		tfa98xx_devices[handle] = tfa98xx;
		dev_info(&tfa98xx->i2c->dev,
			"Registered DSP instance with handle %d\n",
			handle);
		tfa98xx_registered_handles++;
		return handle;
	}
reg_err:
	dev_err(&tfa98xx->i2c->dev,
		"Unable to match I2C address 0x%x with a container device\n",
		tfa98xx->i2c->addr);
	return -EINVAL;
}

static int tfa98xx_unregister_dsp(struct tfa98xx *tfa98xx)
{
	if (tfa98xx->handle < 0)
		return -EINVAL;

	tfa98xx_registered_handles--;

	tfa98xx_devices[tfa98xx->handle] = NULL;
	dev_info(&tfa98xx->i2c->dev,
		"Un-registered DSP instance with handle %d\n",
		tfa98xx->handle);
	return 0;
}


/* I2C wrapper functions */
enum tfa98xx_error tfa98xx_write_register16(tfa98xx_handle_t handle,
	unsigned char subaddress,
	unsigned short value)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa98xx_devices[handle]) {
		tfa98xx = tfa98xx_devices[handle];
		if (!tfa98xx || !tfa98xx->regmap) {
			pr_err("No tfa98xx regmap available\n");
			return TFA98XX_ERROR_BAD_PARAMETER;
		}
retry:
		ret = regmap_write(tfa98xx->regmap, subaddress, value);
		if (ret < 0) {
			pr_warn("i2c error, retries left: %d\n", retries);
			if (retries) {
				retries--;
				msleep(I2C_RETRY_DELAY);
				goto retry;
			}
			return TFA98XX_ERROR_FAIL;
		}
		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx->i2c->dev,
				"WR reg=0x%02x, val=0x%04x %s\n",
				subaddress, value, ret < 0 ? "Error!!" : "");

		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("\tWR     reg=0x%02x, val=0x%04x %s\n",
				subaddress, value, ret < 0 ? "Error!!" : "");
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}
	return error;
}

enum tfa98xx_error tfa98xx_read_register16(tfa98xx_handle_t handle,
					unsigned char subaddress,
					unsigned short *val)
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	unsigned int value;
	int retries = I2C_RETRIES;
	int ret;

	if (tfa98xx_devices[handle]) {
		tfa98xx = tfa98xx_devices[handle];
		if (!tfa98xx || !tfa98xx->regmap) {
			pr_err("No tfa98xx regmap available\n");
			return TFA98XX_ERROR_BAD_PARAMETER;
		}
retry:
		ret = regmap_read(tfa98xx->regmap, subaddress, &value);
		if (ret < 0) {
			pr_warn("i2c error at subaddress 0x%x, retries left: %d\n",
				subaddress, retries);
			if (retries) {
				retries--;
				msleep(I2C_RETRY_DELAY);
				goto retry;
			}
			return TFA98XX_ERROR_FAIL;
		}
		*val = value & 0xffff;

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx->i2c->dev,
				"RD   reg=0x%02x, val=0x%04x %s\n",
				subaddress, *val, ret < 0 ? "Error!!" : "");
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("\tRD     reg=0x%02x, val=0x%04x %s\n",
				subaddress, *val, ret < 0 ? "Error!!" : "");
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}
	return error;
}

enum tfa98xx_error tfa98xx_read_data(tfa98xx_handle_t handle,
				unsigned char reg,
				int len, unsigned char value[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	struct i2c_client *tfa98xx_client;
	int err;
	int tries = 0;
	struct i2c_msg msgs[] = {
		{
			.flags = 0,
			.len = 1,
			.buf = &reg,
		}, {
			.flags = I2C_M_RD,
			.len = len,
			.buf = value,
		},
	};

	if (tfa98xx_devices[handle] && tfa98xx_devices[handle]->i2c) {
		tfa98xx = tfa98xx_devices[handle];
		tfa98xx_client = tfa98xx->i2c;
		msgs[0].addr = tfa98xx_client->addr;
		msgs[1].addr = tfa98xx_client->addr;

		do {
			err = i2c_transfer(tfa98xx_client->adapter, msgs,
				ARRAY_SIZE(msgs));
			if (err != ARRAY_SIZE(msgs))
				msleep_interruptible(I2C_RETRY_DELAY);
		} while ((err != ARRAY_SIZE(msgs)) && (++tries < I2C_RETRIES));

		if (err != ARRAY_SIZE(msgs)) {
			dev_err(&tfa98xx_client->dev,
				"read transfer error%d\n", err);
			error = TFA98XX_ERROR_FAIL;
		}

		if (tfa98xx_kmsg_regs)
			dev_dbg(&tfa98xx_client->dev,
				"RD-DAT reg=0x%02x, len=%d\n", reg, len);
		if (tfa98xx_ftrace_regs)
			tfa98xx_trace_printk
				("\t\tRD-DAT reg=0x%02x, len=%d\n", reg, len);
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}
	return error;
}

enum tfa98xx_error tfa98xx_write_raw(tfa98xx_handle_t handle,
				int len,
				const unsigned char data[])
{
	enum tfa98xx_error error = TFA98XX_ERROR_OK;
	struct tfa98xx *tfa98xx;
	int ret;
	int retries = I2C_RETRIES;

	if (tfa98xx_devices[handle]) {
		tfa98xx = tfa98xx_devices[handle];
retry:
		ret = i2c_master_send(tfa98xx->i2c, data, len);
		if (ret < 0) {
			pr_warn("i2c error, retries left: %d\n", retries);
			if (retries) {
				retries--;
				msleep(I2C_RETRY_DELAY);
				goto retry;
			}
		}

		if (ret == len) {
			if (tfa98xx_kmsg_regs)
				dev_dbg(&tfa98xx->i2c->dev,
					"  WR-RAW len=%d\n", len);
			if (tfa98xx_ftrace_regs)
				tfa98xx_trace_printk
					("\t\tWR-RAW len=%d\n", len);
			return TFA98XX_ERROR_OK;
		}
		pr_err("WR-RAW (len=%d) Error I2C send size mismatch %d\n",
			len, ret);
		error = TFA98XX_ERROR_FAIL;
	} else {
		pr_err("No device available\n");
		error = TFA98XX_ERROR_FAIL;
	}
	return error;
}

/* Interrupts management */
static void tfa98xx_interrupt_enable_tfa2(struct tfa98xx *tfa98xx, bool enable)
{
#if defined(ENABLE_INTERRUPT_CONTROL)
	/* Only for 0x72 we need to enable NOCLK interrupts */
	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE)
		tfa_irq_ena(tfa98xx->handle, tfa9912_irq_stnoclk, enable);

	tfa_irq_ena(tfa98xx->handle, tfa9912_irq_stmwsrc, enable);
	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		tfa_irq_ena(tfa98xx->handle, 36, enable);
		/* FIXME: IELP0 does not excist for 9912 */
		tfa_irq_ena(tfa98xx->handle, tfa9912_irq_stclpl, enable);
	}
#endif
}

#if defined(USE_TFA9891)
/* Check if tap-detection can and shall be enabled.
 * Configure SPK interrupt accordingly or setup polling mode
 * Tap-detection shall be active if:
 *  - the service is enabled (tapdet_open), AND
 *  - the current profile is a tap-detection profile
 * On TFA1 familiy of devices, activating tap-detection means enabling the SPK
 * interrupt if available.
 * We also update the tapdet_enabled and tapdet_poll variables.
 */
static void tfa98xx_tapdet_check_update(struct tfa98xx *tfa98xx)
{
	unsigned int enable = false;

	/* Support tap-detection on TFA1 family of devices */
	if ((tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) == 0)
		return;

	if (tfa98xx->tapdet_open &&
		(tfa98xx->tapdet_profiles & (1 << tfa98xx_profile)))
		enable = true;

	if (!gpio_is_valid(tfa98xx->irq_gpio)) {
		/* interrupt not available, setup polling mode */
		tfa98xx->tapdet_poll = true;
		if (enable)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
						&tfa98xx->tapdet_work, HZ/10);
		else
			cancel_delayed_work_sync(&tfa98xx->tapdet_work);
		dev_dbg(tfa98xx->codec->dev,
			"Polling for tap-detection: %s (%d; 0x%x, %d)\n",
			enable ? "enabled" : "disabled",
			tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
			tfa98xx_profile);

	} else {
		dev_dbg(tfa98xx->codec->dev,
			"Interrupt for tap-detection: %s (%d; 0x%x, %d)\n",
				enable ? "enabled":"disabled",
				tfa98xx->tapdet_open, tfa98xx->tapdet_profiles,
				tfa98xx_profile);
		/* enabled interrupt */
		tfa_irq_ena(tfa98xx->handle, tfa9912_irq_sttapdet, enable);
	}

	/* check disabled => enabled transition to clear pending events */
	if (!tfa98xx->tapdet_enabled && enable) {
		/* clear pending event if any */
		tfa_irq_clear(tfa98xx->handle, tfa9912_irq_sttapdet);
	}

	if (!tfa98xx->tapdet_poll)
		tfa_irq_ena(tfa98xx->handle, tfa9912_irq_sttapdet, 1);
		/* enable again */
}
#endif

/* global enable / disable interrupts */
static void tfa98xx_interrupt_enable(struct tfa98xx *tfa98xx, bool enable)
{
	if (tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)
		return;

	if (tfa98xx_dev_family(tfa98xx->handle) == 2)
		tfa98xx_interrupt_enable_tfa2(tfa98xx, enable);
}

/* Firmware management
 * Downloaded once only at module init
 * FIXME: may need to review that (one per instance of codec device?)
 */
#if defined(USE_TFA9872)
static char *fw_name = "Tfa9872.cnt";
#endif

module_param(fw_name, charp, 0644);
MODULE_PARM_DESC(fw_name, "TFA98xx DSP firmware (container file) name.");

static struct tfa_container *container;

static void
tfa98xx_container_loaded(const struct firmware *cont,	void *context)
{
	struct tfa98xx *tfa98xx = context;
	enum tfa_error tfa_err;
	int container_size;
	int handle;
	int ret;
#if defined(TFA_DBGFS_CHECK_MTPEX)
	unsigned short value;
#endif
#if defined(TFADSP_DSP_BUFFER_POOL)
	int index = 0;
#endif

	mutex_lock(&probe_lock);

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;

	if (!cont) {
		pr_err("Failed to read %s\n", fw_name);
		mutex_unlock(&probe_lock);
		return;
	}

	pr_debug("loaded %s - size: %zu\n", fw_name,
					cont ? cont->size : 0);

	container = kzalloc(cont->size, GFP_KERNEL);
	if (!container) {
		release_firmware(cont);
		pr_err("Error allocating memory\n");
		mutex_unlock(&probe_lock);
		return;
	}

	container_size = cont->size;
	memcpy(container, cont->data, container_size);
	release_firmware(cont);

	pr_debug("%.2s%.2s\n", container->version, container->subversion);
	pr_debug("%.8s\n", container->customer);
	pr_debug("%.8s\n", container->application);
	pr_debug("%.8s\n", container->type);
	pr_debug("%d ndev\n", container->ndev);
	pr_debug("%d nprof\n", container->nprof);

	tfa_set_partial_update(partial_enable);
	dev_info(tfa98xx->dev, "%s partial update\n",
		 partial_enable ? "enable" : "disable");

	tfa_err = tfa_load_cnt(container, container_size);
	if (tfa_err != tfa_error_ok) {
		dev_err(tfa98xx->dev, "Cannot load container file, aborting\n");
		mutex_unlock(&probe_lock);
		return;
	}

	/* register codec with dsp */
	tfa98xx->handle = tfa98xx_register_dsp(tfa98xx);
	if (tfa98xx->handle < 0) {
		dev_err(tfa98xx->dev, "Cannot register with DSP, aborting\n");
		tfa98xx->handle = -1;
		mutex_unlock(&probe_lock);
		return;
	}

	if (tfa_probe(tfa98xx->i2c->addr << 1, &handle) != TFA98XX_ERROR_OK) {
		dev_err(tfa98xx->dev,
			"Failed to probe TFA98xx @ 0x%.2x\n",
			tfa98xx->i2c->addr);
		mutex_unlock(&probe_lock);
		return;
	}

	/* prefix is the application name from the cnt */
	tfa_cont_get_app_name(tfa98xx->fw.name);

	/* Override default profile if requested */
	if (strcmp(dflt_prof_name, "")) {
		unsigned int i;

		for (i = 0; i < tfa_cont_max_profile(tfa98xx->handle); i++) {
			if (strcmp(tfa_cont_profile_name(tfa98xx->handle, i),
							dflt_prof_name) == 0) {
				tfa98xx_profile = i;
				dev_info(tfa98xx->dev,
					"changing default profile to %s (%d)\n",
					dflt_prof_name, tfa98xx_profile);
				break;
			}
		}
		if (i >= tfa_cont_max_profile(tfa98xx->handle))
			dev_info(tfa98xx->dev,
				"Default profile override failed (%s profile not found)\n",
				dflt_prof_name);
	}

	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_OK;

#if defined(TFA_DBGFS_CHECK_MTPEX)
	value = snd_soc_read(tfa98xx->codec, TFA98XX_KEY2_PROTECTED_MTP0);

	if (value != -1) {
		tfa98xx->calibrate_done =
			(value & TFA98XX_KEY2_PROTECTED_MTP0_MTPEX_MSK) ? 1 : 0;
		pr_info("[0x%x] calibrate_done = MTPEX (%d) 0x%04x\n",
			tfa98xx->i2c->addr, tfa98xx->calibrate_done, value);
	} else {
		pr_info("[0x%x] error in reading MTPEX\n", tfa98xx->i2c->addr);
		tfa98xx->calibrate_done = 0;
	}
#else
	tfa98xx->calibrate_done = 0;
#endif

	pr_debug("Firmware init complete\n");

#if defined(TFADSP_DSP_BUFFER_POOL)
	/* allocate buffer_pool */
	if (tfa98xx->handle == 0) {
		pr_info("Allocate buffer_pool\n");
		for (index = 0; index < POOL_MAX_INDEX; index++)
			tfa_buffer_pool(index,
				buf_pool_size[index], POOL_ALLOC);
	}
#endif

	if (no_start != 0) {
		mutex_unlock(&probe_lock);
		return;
	}

	/* Only controls for master device */
	if (tfa98xx->handle == 0)
		tfa98xx_create_controls(tfa98xx);

	tfa98xx_inputdev_check_register(tfa98xx);

	if (tfa_is_cold(tfa98xx->handle) == 0) {
		pr_debug("Warning: device 0x%.2x is still warm\n",
			tfa98xx->i2c->addr);
		tfa_reset();
	}

	if (tfa98xx->flags & TFA98XX_FLAG_DSP_START_ON_MUTE) {
		tfa98xx_interrupt_enable(tfa98xx, true);
		mutex_unlock(&probe_lock);
		return;
	}

	if (tfa98xx->handle == 0) {
		mutex_lock(&tfa98xx->dsp_lock);
		ret = tfa98xx_tfa_start
			(tfa98xx, tfa98xx_profile, tfa98xx_vsteps);
		if (ret == TFA98XX_ERROR_OK)
			tfa98xx->dsp_init = TFA98XX_DSP_INIT_DONE;
		else if (ret == TFA98XX_ERROR_NOT_SUPPORTED)
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
		mutex_unlock(&tfa98xx->dsp_lock);
	}
	tfa98xx_interrupt_enable(tfa98xx, true);

	mutex_unlock(&probe_lock);
}

static int tfa98xx_load_container(struct tfa98xx *tfa98xx)
{
	mutex_lock(&probe_lock);
	tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_PENDING;
	mutex_unlock(&probe_lock);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
		fw_name, tfa98xx->dev, GFP_KERNEL,
		tfa98xx, tfa98xx_container_loaded);
}

#if defined(USE_TFA9891)
static void tfa98xx_tapdet(struct tfa98xx *tfa98xx)
{
	unsigned int tap_pattern;
	int btn;

	/* check tap pattern (BTN_0 is "error" wrong tap indication */
	tap_pattern = tfa_get_tap_pattern(tfa98xx->handle);
	switch (tap_pattern) {
	case 0xffffffff:
		pr_info("More than 4 taps detected! (flagTapPattern = -1)\n");
		btn = BTN_0;
		break;
	case 0xfffffffe:
	case 0xfe:
		pr_info("Illegal tap detected!\n");
		btn = BTN_0;
		break;
	case 0:
		pr_info("Unrecognized pattern! (flagTapPattern = 0)\n");
		btn = BTN_0;
		break;
	default:
		pr_info("Detected pattern: %d\n", tap_pattern);
		btn = BTN_0 + tap_pattern;
		break;
	}

	input_report_key(tfa98xx->input, btn, 1);
	input_report_key(tfa98xx->input, btn, 0);
	input_sync(tfa98xx->input);

	/* acknowledge event done by clearing interrupt */
}

static void tfa98xx_tapdet_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;

	/* TODO check is this is still needed for tap polling */
	tfa98xx = container_of(work, struct tfa98xx, tapdet_work.work);

	if (tfa_irq_get(tfa98xx->handle, tfa9912_irq_sttapdet))
		tfa98xx_tapdet(tfa98xx);

	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->tapdet_work, HZ/10);
}
#endif

static void tfa98xx_monitor(struct work_struct *work)
{
	struct tfa98xx *tfa98xx;
	u16 val;
#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
	int active_handle = -1;
#if defined(TFA_PROFILE_ON_DEVICE)
	int dev, devcount = tfa98xx_cnt_max_device();
#endif
#endif

	tfa98xx = container_of(work, struct tfa98xx, monitor_work.work);

	pr_info("%s: [%d] - profile = %d: %s\n", __func__,
		tfa98xx->handle, tfa98xx_profile,
		tfa_cont_profile_name(tfa98xx->handle, tfa98xx_profile));

#if defined(TFA_USE_DEVICE_SPECIFIC_CONTROL)
#if defined(TFA_PROFILE_ON_DEVICE)
	for (dev = 0; dev < devcount; dev++)
		if (tfa_cont_is_dev_specific_profile(dev,
			tfa98xx_profile) != 0)
			active_handle = dev; /* check activeness with profile */
#endif

	if (active_handle != -1)
		if (active_handle != tfa98xx->handle)
			goto tfa_monitor_exit;
#endif /* TFA_USE_DEVICE_SPECIFIC_CONTROL */

	/* Check for tap-detection - bypass monitor if it is active */
	if (!tfa98xx->input) {
		/*
		 * check IC status bits: cold start
		 * and DSP watch dog bit to re init
		 */
		val = snd_soc_read(tfa98xx->codec, TFA98XX_STATUS_FLAGS0);
		pr_debug("STATUS_FLAG0: 0x%04x\n", val);

		if (tfa98xx->pstream != 0) {
			if (!(TFA98XX_STATUS_FLAGS0_SWS & val))
				pr_err("ERROR: SWS\n");

			/* Check secondary errors */
			if (!(val & TFA98XX_STATUS_FLAGS0_CLKS) ||
				!(val & TFA98XX_STATUS_FLAGS0_UVDS) ||
				!(val & TFA98XX_STATUS_FLAGS0_OVDS) ||
				!(val & TFA98XX_STATUS_FLAGS0_OTDS) ||
				!(val & TFA98XX_STATUS_FLAGS0_PLLS) ||
				(!(tfa98xx->flags & TFA98XX_FLAG_TDM_DEVICE) &&
				 !(val & TFA98XX_STATUS_FLAGS0_VDDS)))
				pr_err("Misc errors detected: STATUS_FLAG0 = 0x%x\n",
					val);

			if (tfa98xx->flags & TFA98XX_FLAG_TDM_DEVICE) {
				val = snd_soc_read
					(tfa98xx->codec, TFA98XX_STATUS_FLAGS1);
				pr_debug("STATUS_FLAG1: 0x%04x\n", val);

				if (val & TFA98XX_STATUS_FLAGS1_TDMERR)
					pr_info("TDM status: 0x%x (ref. 0x1: synchronized)\n",
					  (val & TFA98XX_STATUS_FLAGS1_TDMSTAT)
					  >> TFA98XX_STATUS_FLAGS1_TDMSTAT_POS);
				if (val & TFA98XX_STATUS_FLAGS1_TDMLUTER)
					pr_info("TDM size is not configurable with nbck/nslots: 0x%x\n",
						val);
			}
		}

		/* further debugging */
		val = snd_soc_read(tfa98xx->codec, TFA98XX_SYS_CONTROL0);
		pr_debug("SYS_CONTROL0: 0x%04x\n", val);
		val = snd_soc_read(tfa98xx->codec, TFA98XX_SYS_CONTROL1);
		pr_debug("SYS_CONTROL1: 0x%04x\n", val);
		val = snd_soc_read(tfa98xx->codec, TFA98XX_SYS_CONTROL2);
		pr_debug("SYS_CONTROL2: 0x%04x\n", val);
		val = snd_soc_read(tfa98xx->codec, TFA98XX_CLOCK_CONTROL);
		pr_debug("CLOCK_CONTROL: 0x%04x\n", val);
		val = snd_soc_read(tfa98xx->codec, TFA98XX_STATUS_FLAGS4);
		pr_debug("STATUS_FLAG4: 0x%04x\n", val);
		val = snd_soc_read(tfa98xx->codec, TFA98XX_TDM_CONFIG0);
		pr_debug("TDM_CONFIG0: 0x%04x\n", val);
	}

tfa_monitor_exit:
	pr_info("%s: exit\n", __func__);
	/* reschedule */
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->monitor_work, 5*HZ);
}

static void tfa98xx_dsp_init(struct tfa98xx *tfa98xx)
{
	int ret;
	bool failed = false;
	bool reschedule = false;

	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK) {
		pr_debug("Skipping tfa_start (no FW: %d)\n",
			tfa98xx->dsp_fw_state);
		return;
	}

	if (tfa98xx->dsp_init == TFA98XX_DSP_INIT_DONE) {
		pr_debug("Stream already started, skipping DSP power-on\n");
		return;
	}

	mutex_lock(&tfa98xx->dsp_lock);
	pr_info("%s: ...\n", __func__);

	/* further debugging */
	tfa_verbose(1);
	tfa_cont_verbose(1);

	tfa98xx->dsp_init = TFA98XX_DSP_INIT_PENDING;

	if (tfa98xx->init_count < TF98XX_MAX_DSP_START_TRY_COUNT) {
		/* directly try to start DSP */
		ret = tfa98xx_tfa_start
			(tfa98xx, tfa98xx_profile, tfa98xx_vsteps);
		if (ret == TFA98XX_ERROR_NOT_SUPPORTED) {
			mutex_unlock(&probe_lock);
			tfa98xx->dsp_fw_state = TFA98XX_DSP_FW_FAIL;
			dev_err(&tfa98xx->i2c->dev, "Failed starting device\n");
			failed = true;
			mutex_unlock(&probe_lock);
		} else if (ret != TFA98XX_ERROR_OK) {
			/* It may fail as we may not have a valid clock at that
			 * time, so re-schedule and re-try later.
			 */
			dev_err(&tfa98xx->i2c->dev,
					"tfa_start failed! (err %d) - %d\n",
					ret, tfa98xx->init_count);
			reschedule = true;
		} else {
			/* Subsystem ready, tfa init complete */
			dev_dbg(&tfa98xx->i2c->dev,
						"tfa_start success (%d)\n",
						tfa98xx->init_count);
			/* cancel other pending init works */
			cancel_delayed_work(&tfa98xx->init_work);
			tfa98xx->init_count = 0;
			/*
			 * start monitor thread to check IC status bit
			 * periodically, and re-init IC to recover if
			 * needed.
			 */
			queue_delayed_work(tfa98xx->tfa98xx_wq,
						&tfa98xx->monitor_work,
						1*HZ);
		}
	} else {
		/* exceeded max number ot start tentatives, cancel start */
		dev_err(&tfa98xx->i2c->dev,
			"Failed starting device (%d)\n",
			tfa98xx->init_count);
			failed = true;
	}
	if (reschedule) {
		/* reschedule this init work for later */
		queue_delayed_work(tfa98xx->tfa98xx_wq,
			&tfa98xx->init_work,
			msecs_to_jiffies(5));
		tfa98xx->init_count++;
	}
	if (failed) {
		tfa98xx->dsp_init = TFA98XX_DSP_INIT_FAIL;
		/* cancel other pending init works */
		cancel_delayed_work(&tfa98xx->init_work);
		tfa98xx->init_count = 0;
	}
	mutex_unlock(&tfa98xx->dsp_lock);
}


static void tfa98xx_dsp_init_work(struct work_struct *work)
{
	struct tfa98xx *tfa98xx =
		container_of(work, struct tfa98xx, init_work.work);

	pr_debug("%s: enter with profile %d\n", __func__, tfa98xx_profile);
	/* Only do dsp init for master device */
	if (tfa98xx->handle != 0) {
		pr_debug("%s: no first handle\n", __func__);
		return;
	}

	tfa98xx_dsp_init(tfa98xx);
}

static void tfa98xx_interrupt(struct work_struct *work)
{
	int err;
	struct tfa98xx *tfa98xx =
		container_of(work, struct tfa98xx, interrupt_work.work);

	pr_info("\n");

#if defined(USE_TFA9891)
	if (tfa98xx->flags & TFA98XX_FLAG_TAPDET_AVAILABLE) {
		/* check for tap interrupt */
		if (tfa_irq_get(tfa98xx->handle, tfa9912_irq_sttapdet)) {
			tfa98xx_tapdet(tfa98xx);

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->handle, tfa9912_irq_sttapdet);
		}
	} /* TFA98XX_FLAG_TAPDET_AVAILABLE */
#endif

	if (tfa98xx->flags & TFA98XX_FLAG_REMOVE_PLOP_NOISE) {
		/* Remove sticky bit by reading it once */
		TFA_GET_BF(tfa98xx->handle, NOCLK);

		/* No clock detected */
		if (tfa_irq_get(tfa98xx->handle, tfa9912_irq_stnoclk)) {

			int no_clk = TFA_GET_BF(tfa98xx->handle, NOCLK);
			/* Detect for clock is lost! (clock is not stable) */
			if ((tfa98xx->handle == 0) && (no_clk == 1)) {
				/* Clock lost. Set I2CR to remove POP noise */
				pr_info("No clock detected. Resetting I2CR to avoid pop on 72!\n");
				err = tfa98xx_tfa_start
					(tfa98xx, tfa98xx_profile,
					tfa98xx_vsteps);
				if (err != TFA98XX_ERROR_OK)
					pr_err("Error loading i2c registers (tfa_start), err=%d\n",
						err);
				else
					pr_info("Setting i2c registers after I2CR successfully\n");

				/* This is only for SAAM on the 72.
				 * Since the NOCLK interrupt is only enabled
				 * for 72 this is the place
				 * However: Not tested yet! But also does
				 * not harm normal flow!
				 */
				if (strnstr(tfa_cont_profile_name
					(tfa98xx->handle, tfa98xx_profile),
					".saam", strlen(tfa_cont_profile_name
					(tfa98xx->handle, tfa98xx_profile)))) {
					pr_info("Powering down from a SAAM profile, workaround PLMA4766 used!\n");
				}
			}

			/* If clk is stable set polarity
			 * to check for LOW (no clock)
			 */
			tfa_irq_set_pol(tfa98xx->handle,
					tfa9912_irq_stnoclk, (no_clk == 0));

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->handle, tfa9912_irq_stnoclk);
		}
	} /* TFA98XX_FLAG_REMOVE_PLOP_NOISE */

	/* manager wait for source state */
	if (tfa_irq_get(tfa98xx->handle, tfa9912_irq_stmwsrc)) {
		int manwait1 = TFA_GET_BF(tfa98xx->handle, MANWAIT1);

		if (manwait1 > 0) {
			pr_info("entering wait for source state\n");
			tfa98xx->count_wait_for_source_state++;

			/* set AMPC and AMPE to make sure the amp is enabled */
			pr_info("setting AMPC and AMPE to 1 (default)\n");
		} else {
			/* Now we can switch profile with internal clock
			 * it is not required to call tfa_start
			 */
			pr_info("leaving wait for source state\n");
			if (tfa98xx->set_mtp_cal) {
				enum tfa98xx_error err;

				tfa98xx_open(tfa98xx->handle);
				err = tfa_mtp_set_calibration
					(tfa98xx->handle, tfa98xx->cal_data);
				tfa98xx_close(tfa98xx->handle);
				if (err != TFA98XX_ERROR_OK) {
					pr_err("Error, setting calibration value in mtp, err=%d\n",
						err);
				} else {
					tfa98xx->set_mtp_cal = false;
					pr_info("Calibration value (%d) set in mtp\n",
						tfa98xx->cal_data);
				}
			}
		}

		tfa_irq_set_pol(tfa98xx->handle, tfa9912_irq_stmwsrc,
				(manwait1 == 0));

		/* clear interrupt */
		tfa_irq_clear(tfa98xx->handle, tfa9912_irq_stmwsrc);
	}

	if (tfa98xx->flags & TFA98XX_FLAG_LP_MODES) {
		const int irq_stclp0 = 36;
		/* FIXME: this 72 interrupt does not excist for 9912 */

		if (tfa_irq_get(tfa98xx->handle, irq_stclp0)) {
			int lp0 = TFA_GET_BF(tfa98xx->handle, LP0);

			if (lp0 > 0)
				pr_info("lowpower mode 0 detected\n");
			else
				pr_info("lowpower mode 0 not detected\n");

			tfa_irq_set_pol(tfa98xx->handle, irq_stclp0,
					(lp0 == 0));

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->handle, irq_stclp0);
		}

		if (tfa_irq_get(tfa98xx->handle, tfa9912_irq_stclpl)) {
			int lp1 = TFA_GET_BF(tfa98xx->handle, LP1);

			if (lp1 > 0)
				pr_info("lowpower mode 1 detected\n");
			else
				pr_info("lowpower mode 1 not detected\n");

			tfa_irq_set_pol(tfa98xx->handle, tfa9912_irq_stclpl,
					(lp1 == 0));

			/* clear interrupt */
			tfa_irq_clear(tfa98xx->handle, tfa9912_irq_stclpl);
		}
	} /* TFA98XX_FLAG_LP_MODES */

	/* unmask interrupts masked in IRQ handler */
	 tfa_irq_unmask(tfa98xx->handle);
}

static int tfa98xx_startup(struct snd_pcm_substream *substream,
						struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int idx = 0;
#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	unsigned int sr;
	int len, prof, nprof = tfa_cont_max_profile(tfa98xx->handle);
#if defined(TFADSP_DSP_BUFFER_POOL)
	char basename[MAX_CONTROL_NAME] = {0};
#else
	char *basename;
#endif
#endif /* TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION */
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

#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
#if !defined(TFADSP_DSP_BUFFER_POOL)
	basename = kzalloc(MAX_CONTROL_NAME, GFP_KERNEL);
	if (!basename)
		return -ENOMEM;
#endif

	/* copy profile name into basename until the . */
	get_profile_basename
		(basename,
		tfa_cont_profile_name(tfa98xx->handle, tfa98xx_profile));
	len = strlen(basename);
#endif /* TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION */

	/* loop over all profiles and get the supported samples rate(s) from
	 * the profiles with the same basename
	 */
	tfa98xx->rate_constraint.list = &tfa98xx->rate_constraint_list[0];
	tfa98xx->rate_constraint.count = 0;

#if !defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	for (prof = 0; prof < nprof; prof++) {
		if (strncmp(basename,
			tfa_cont_profile_name(tfa98xx->handle, prof), len)
			== 0) {
			/* Check which sample rate is supported
			 * with current profile, and enforce this.
			 */
			sr = tfa98xx_get_profile_sr(tfa98xx->handle, prof);
			if (!sr)
				dev_info(codec->dev,
					"Unable to identify supported sample rate\n");

			if (tfa98xx->rate_constraint.count
				>= TFA98XX_NUM_RATES) {
				dev_err(codec->dev, "too many sample rates\n");
			} else {
				tfa98xx->rate_constraint_list[idx++] = sr;
				tfa98xx->rate_constraint.count += 1;
			}
		}
	}

#if !defined(TFADSP_DSP_BUFFER_POOL)
	kfree(basename);
#endif

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
		SNDRV_PCM_HW_PARAM_RATE,
		&tfa98xx->rate_constraint);
#else
	pr_info("%s: add all the rates in the list\n", __func__);
	for (idx = 0; idx < (int)ARRAY_SIZE(rate_to_fssel); idx++) {
		tfa98xx->rate_constraint_list[idx] = rate_to_fssel[idx].rate;
		tfa98xx->rate_constraint.count += 1;
	}

	pr_info("%s: skip setting constraint, assuming fixed format\n",
		__func__);

	return 0;
#endif /* TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION */
}

static int tfa98xx_set_dai_sysclk(struct snd_soc_dai *codec_dai,
	int clk_id, unsigned int freq, int dir)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec_dai->codec);

	tfa98xx->sysclk = freq;
	return 0;
}

static int tfa98xx_set_tdm_slot(struct snd_soc_dai *dai, unsigned int tx_mask,
	unsigned int rx_mask, int slots, int slot_width)
{
	pr_debug("\n");
	return 0;
}

static int tfa98xx_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(dai->codec);
	struct snd_soc_codec *codec = dai->codec;

	pr_debug("fmt=0x%x\n", fmt);

	/* Supported mode: regular I2S, slave, or PDM */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		if ((fmt & SND_SOC_DAIFMT_MASTER_MASK)
			!= SND_SOC_DAIFMT_CBS_CFS) {
			dev_err(codec->dev, "Invalid Codec master mode\n");
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(codec->dev, "Unsupported DAI format %d\n",
			fmt & SND_SOC_DAIFMT_FORMAT_MASK);
		return -EINVAL;
	}

	tfa98xx->audio_mode = fmt & SND_SOC_DAIFMT_FORMAT_MASK;

	return 0;
}

static int tfa98xx_get_fssel(unsigned int rate)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(rate_to_fssel); i++)
		if (rate_to_fssel[i].rate == rate)
			return rate_to_fssel[i].fssel;

	return -EINVAL;
}

static int tfa98xx_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	unsigned int rate;
	int prof_idx;

	/* Supported */
	rate = params_rate(params);
	pr_debug("Requested rate: %d, sample size: %d, physical size: %d\n",
			rate, snd_pcm_format_width(params_format(params)),
			snd_pcm_format_physical_width(params_format(params)));
#if defined(TFA_FULL_RATE_SUPPORT_WITH_POST_CONVERSION)
	pr_info("forced to change rate: %d to %d\n", rate, sr_converted);
	rate = sr_converted;
#endif

	if (no_start != 0)
		return 0;

	/* check if samplerate is supported for this mixer profile */
	prof_idx = get_profile_id_for_sr(tfa98xx_mixer_profile, rate);
	if (prof_idx < 0) {
		pr_err("tfa98xx: invalid sample rate %d.\n", rate);
		return -EINVAL;
	}
	pr_debug("mixer profile:container profile = [%d:%d]\n",
		tfa98xx_mixer_profile, prof_idx);

	/* update 'real' profile (container profile) */
	tfa98xx_profile = prof_idx;

	pr_info("%s: tfa98xx_profile %d\n", __func__, tfa98xx_profile);

	/* update to new rate */
	tfa98xx->rate = rate;

	return 0;
}

static int tfa98xx_mute(struct snd_soc_dai *dai, int mute, int stream)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);

	dev_dbg(&tfa98xx->i2c->dev,
		"%s: state: %d (stream = %d)\n", __func__, mute, stream);

	if (!(tfa98xx->flags & TFA98XX_FLAG_DSP_START_ON_MUTE))
		return 0;

	if (no_start) {
		pr_debug("no_start parameter set no tfa_start or tfa_stop, returning\n");
		return 0;
	}

	_tfa98xx_mute(tfa98xx, mute, stream);

	return 0;
}

static int _tfa98xx_mute(struct tfa98xx *tfa98xx, int mute, int stream)
{
	if (mute) {
		/* stop DSP only when both playback and capture streams
		 * are deactivated
		 */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (tfa98xx->pstream == 0) {
				pr_debug("mute:%d [pstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->pstream = 0;
		} else if (stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (tfa98xx->cstream == 0) {
				pr_debug("mute:%d [cstream duplicated]\n",
					mute);
				return 0;
			}
			tfa98xx->cstream = 0;
		}
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);
		tfa98xx_set_stream_state((tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));

#if defined(TFA_BLACKBOX_LOGGING)
		if ((handles_local[0].log_get_cb)
			&& (tfa98xx->pstream ^ tfa98xx->cstream)) {
			mutex_lock(&tfa98xx->dsp_lock);
			if (tfa98xx->pstream == 0) {
				/* force to set pstream to enable messaging */
				tfa98xx_set_stream_state
					(BIT_PSTREAM
					|((tfa98xx->cstream<<1) & BIT_CSTREAM)
					|((tfa98xx->samstream<<2)
					& BIT_SAMSTREAM));
			}

			handles_local[0].log_get_cb();

			/* restore to reset pstream */
			tfa98xx_set_stream_state
				((tfa98xx->pstream & BIT_PSTREAM)
				|((tfa98xx->cstream<<1) & BIT_CSTREAM)
				|((tfa98xx->samstream<<2)
				& BIT_SAMSTREAM));
			mutex_unlock(&tfa98xx->dsp_lock);
		}
#endif

		/* case: both p/cstream (either) and samstream are off
		 * if (!(tfa98xx->pstream == 0 || tfa98xx->cstream == 0)
		 *  || (tfa98xx->samstream != 0)) {
		 *  pr_info("mute is suspended until playback/saam are off\n");
		 *  return 0;
		 * }
		 */
		/* wait until both main streams (pstream / samstream) are off */
		if ((tfa98xx->pstream == 0)
			&& (tfa98xx->samstream == 0)) {
			pr_info("mute is triggered\n");
		} else {
			pr_info("mute is suspended when only cstream is off\n");
			return 0;
		}

		_tfa98xx_stop(tfa98xx);

	} else {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			tfa98xx->pstream = 1;
		else if (stream == SNDRV_PCM_STREAM_CAPTURE)
			tfa98xx->cstream = 1;
		pr_info("mute:%d [pstream %d, cstream %d, samstream %d]\n",
			mute,
			tfa98xx->pstream, tfa98xx->cstream, tfa98xx->samstream);
		tfa98xx_set_stream_state((tfa98xx->pstream & BIT_PSTREAM)
			|((tfa98xx->cstream<<1) & BIT_CSTREAM)
			|((tfa98xx->samstream<<2) & BIT_SAMSTREAM));

		/* case: either p/cstream (both) or samstream is on
		 * if ((tfa98xx->pstream != 0 && tfa98xx->pstream != 0)
		 *  || tfa98xx->samstream != 0) {
		 */
		/* wait until DSP is ready for initialization */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK
			|| stream == SNDRV_PCM_STREAM_SAAM) {
			pr_info("unmute is triggered\n");
		} else {
			pr_debug("unmute is suspended when only cstream is on\n");
			return 0;
		}

		pr_debug("%s: unmute with profile %d\n",
			__func__, tfa98xx_profile);

		/* Only do dsp init for master device */
		if (tfa98xx->handle != 0) {
			pr_err("%s: no first handle\n", __func__);
			return 0;
		}

		/* Start DSP */
		pr_info("%s: start tfa amp\n", __func__);
		if (tfa98xx->dsp_init != TFA98XX_DSP_INIT_PENDING)
			queue_delayed_work(tfa98xx->tfa98xx_wq,
				&tfa98xx->init_work, 0);
	}

	return 0;
}

static int _tfa98xx_stop(struct tfa98xx *tfa98xx)
{
	cancel_delayed_work_sync(&tfa98xx->monitor_work);

	cancel_delayed_work_sync(&tfa98xx->init_work);
	if (tfa98xx->dsp_fw_state != TFA98XX_DSP_FW_OK)
		return 0;
	mutex_lock(&tfa98xx->dsp_lock);
	pr_info("%s: stop tfa amp\n", __func__);
	tfa_stop();
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	mutex_unlock(&tfa98xx->dsp_lock);

	return 0;
}

static const struct snd_soc_dai_ops tfa98xx_dai_ops = {
	.startup = tfa98xx_startup,
	.set_fmt = tfa98xx_set_fmt,
	.set_sysclk = tfa98xx_set_dai_sysclk,
	.set_tdm_slot = tfa98xx_set_tdm_slot,
	.hw_params = tfa98xx_hw_params,
	.mute_stream = tfa98xx_mute,
};

static struct snd_soc_dai_driver tfa98xx_dai[] = {
	{
		.name = "tfa98xx-aif",
		.base = TFA98XX_TDM_CONFIG0 - 1,
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.capture = {
			.stream_name = "AIF Capture",
			.channels_min = 1,
			.channels_max = MAX_HANDLES,
			.rates = TFA98XX_RATES,
			.formats = TFA98XX_FORMATS,
		},
		.ops = &tfa98xx_dai_ops,
		.symmetric_rates = 1,
		.symmetric_channels = 1,
		.symmetric_samplebits = 1,
	},
};

static int tfa98xx_probe(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
	int ret;

	pr_debug("\n");

	/* setup work queue, will be used to initial DSP on first boot up */
	tfa98xx->tfa98xx_wq = create_singlethread_workqueue("tfa98xx");
	if (!tfa98xx->tfa98xx_wq)
		return -ENOMEM;

	INIT_DELAYED_WORK(&tfa98xx->init_work, tfa98xx_dsp_init_work);
	INIT_DELAYED_WORK(&tfa98xx->monitor_work, tfa98xx_monitor);
	INIT_DELAYED_WORK(&tfa98xx->interrupt_work, tfa98xx_interrupt);
#if defined(USE_TFA9891)
	INIT_DELAYED_WORK(&tfa98xx->tapdet_work, tfa98xx_tapdet_work);
#endif

	tfa98xx->codec = codec;

	ret = tfa98xx_load_container(tfa98xx);
	pr_debug("Container loading requested: %d\n", ret);

	tfa98xx_add_widgets(tfa98xx);

	dev_info(codec->dev, "tfa98xx codec registered (%s)\n",
		 tfa98xx->fw.name);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
	sec_audio_bootlog(6, "%s: tfa98xx codec registered (%s)\n",
			__func__, tfa98xx->fw.name);
#endif

	return ret;
}

static int tfa98xx_remove(struct snd_soc_codec *codec)
{
	struct tfa98xx *tfa98xx = snd_soc_codec_get_drvdata(codec);
#if defined(TFADSP_DSP_BUFFER_POOL)
	int index = 0;
#endif

	pr_debug("\n");
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
	sec_audio_bootlog(7, "%s\n", __func__);
#endif

	tfa98xx_interrupt_enable(tfa98xx, false);

	tfa98xx_inputdev_unregister(tfa98xx);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
	cancel_delayed_work_sync(&tfa98xx->init_work);
#if defined(USE_TFA9891)
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
#endif

	if (tfa98xx->tfa98xx_wq)
		destroy_workqueue(tfa98xx->tfa98xx_wq);

#if defined(TFADSP_DSP_BUFFER_POOL)
	/* deallocate buffer_pool */
	pr_info("Deallocate buffer_pool\n");
	for (index = 0; index < POOL_MAX_INDEX; index++)
		tfa_buffer_pool(index, 0, POOL_FREE);
#endif

	return 0;
}

struct regmap *tfa98xx_get_regmap(struct device *dev)
{
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	return tfa98xx->regmap;
}

static struct snd_soc_codec_driver soc_codec_dev_tfa98xx = {
	.probe =	tfa98xx_probe,
	.remove =	tfa98xx_remove,
	.get_regmap = tfa98xx_get_regmap,
};

static bool tfa98xx_writeable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_readable_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static bool tfa98xx_volatile_register(struct device *dev, unsigned int reg)
{
	/* enable read access for all registers */
	return 1;
}

static const struct regmap_config tfa98xx_regmap = {
	.reg_bits = 8,
	.val_bits = 16,

	.max_register = TFA98XX_MAX_REGISTER,
	.writeable_reg = tfa98xx_writeable_register,
	.readable_reg = tfa98xx_readable_register,
	.volatile_reg = tfa98xx_volatile_register,
	.cache_type = REGCACHE_NONE,
};

static void tfa98xx_irq_tfa2(struct tfa98xx *tfa98xx)
{
	pr_info("\n");

	/*
	 * mask interrupts
	 * will be unmasked after handling interrupts in workqueue
	 */
	tfa_irq_mask(tfa98xx->handle);
	queue_delayed_work(tfa98xx->tfa98xx_wq, &tfa98xx->interrupt_work, 0);
}


static irqreturn_t tfa98xx_irq(int irq, void *data)
{
	struct tfa98xx *tfa98xx = data;

	if (tfa98xx_dev_family(tfa98xx->handle) == 2)
		tfa98xx_irq_tfa2(tfa98xx);

	return IRQ_HANDLED;
}

static int tfa98xx_ext_reset(struct tfa98xx *tfa98xx)
{
	if (tfa98xx && gpio_is_valid(tfa98xx->reset_gpio)) {
		gpio_set_value_cansleep(tfa98xx->reset_gpio, 1);
		gpio_set_value_cansleep(tfa98xx->reset_gpio, 0);
	}
	return 0;
}

static int tfa98xx_parse_dt(struct device *dev,
	struct tfa98xx *tfa98xx, struct device_node *np)
{
	int ret;

	tfa98xx->reset_gpio = of_get_named_gpio(np, "nxp,reset-gpio", 0);
	if (tfa98xx->reset_gpio < 0)
		dev_dbg(dev,
			"No reset GPIO provided, will not HW reset device\n");

	tfa98xx->irq_gpio = of_get_named_gpio(np, "nxp,irq-gpio", 0);
	if (tfa98xx->irq_gpio < 0)
		dev_dbg(dev, "No IRQ GPIO provided.\n");

	ret = of_property_read_string(np, "nxp,firmware-name",
					(char const **)&fw_name);
	if (ret < 0)
		dev_dbg(dev, "firmware-name is %s (default)\n", fw_name);
	else
		dev_dbg(dev, "firmware-name is %s\n", fw_name);

	return 0;
}

static ssize_t tfa98xx_reg_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);

	if (count != 1) {
		pr_debug("invalid register address");
		return -EINVAL;
	}

	tfa98xx->reg = buf[0];

	return 1;
}

static ssize_t tfa98xx_rw_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	u8 *data;
	int ret = 0;
	int retries = I2C_RETRIES;

	data = kmalloc(count+1, GFP_KERNEL);
	if (data == NULL) {
		ret = -ENOMEM;
		pr_debug("can not allocate memory\n");
		return ret;
	}

	data[0] = tfa98xx->reg;
	memcpy(&data[1], buf, count);

retry:
	ret = i2c_master_send(tfa98xx->i2c, data, count+1);
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
	}

	kfree(data);
	return ret;
}

#if defined(TFA_BLACKBOX_LOGGING)
int tfa_log_register(configure_log_t tfa_log_configure,
	update_log_t tfa_log_update)
{
	handles_local[0].log_set_cb = tfa_log_configure;
	handles_local[1].log_set_cb = tfa_log_configure;

	handles_local[0].log_get_cb = tfa_log_update;
	handles_local[1].log_get_cb = tfa_log_update;

	return 0;
}
#endif

static ssize_t tfa98xx_rw_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buf, loff_t off, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct tfa98xx *tfa98xx = dev_get_drvdata(dev);
	struct i2c_msg msgs[] = {
		{
			.addr = tfa98xx->i2c->addr,
			.flags = 0,
			.len = 1,
			.buf = &tfa98xx->reg,
		},
		{
			.addr = tfa98xx->i2c->addr,
			.flags = I2C_M_RD,
			.len = count,
			.buf = buf,
		},
	};
	int ret;
	int retries = I2C_RETRIES;
retry:
	ret = i2c_transfer(tfa98xx->i2c->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_warn("i2c error, retries left: %d\n", retries);
		if (retries) {
			retries--;
			msleep(I2C_RETRY_DELAY);
			goto retry;
		}
		return ret;
	}
	/* ret contains the number of i2c messages send */
	return 1 + ((ret > 1) ? count : 0);
}

static struct bin_attribute dev_attr_rw = {
	.attr = {
		.name = "rw",
		.mode = 0600,
	},
	.size = 0,
	.read = tfa98xx_rw_read,
	.write = tfa98xx_rw_write,
};

static struct bin_attribute dev_attr_reg = {
	.attr = {
		.name = "reg",
		.mode = 0200,
	},
	.size = 0,
	.read = NULL,
	.write = tfa98xx_reg_write,
};

static int tfa98xx_i2c_probe(struct i2c_client *i2c,
	const struct i2c_device_id *id)
{
	struct snd_soc_dai_driver *dai;
	struct tfa98xx *tfa98xx;
	struct device_node *np = i2c->dev.of_node;
	int irq_flags;
	unsigned int reg;
	int ret;

	pr_info("%s: start probing\n", __func__);
	pr_info("addr=0x%x\n", i2c->addr);

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
		sec_audio_bootlog(3, "%s: check_functionality failed\n", __func__);
#endif
		return -EIO;
	}

	tfa98xx = devm_kzalloc(&i2c->dev, sizeof(struct tfa98xx),
		GFP_KERNEL);
	if (tfa98xx == NULL)
		return -ENOMEM;

	tfa98xx->dev = &i2c->dev;
	tfa98xx->i2c = i2c;
	tfa98xx->dsp_init = TFA98XX_DSP_INIT_STOPPED;
	tfa98xx->rate = 48000; /* init to default sample rate (48kHz) */
	tfa98xx->handle = -1;

	tfa98xx->regmap = devm_regmap_init_i2c(i2c, &tfa98xx_regmap);
	if (IS_ERR(tfa98xx->regmap)) {
		ret = PTR_ERR(tfa98xx->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			ret);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
		sec_audio_bootlog(3, "%s: Failed to allocate register map: %d\n",
				__func__, ret);
#endif
		goto err;
	}

	i2c_set_clientdata(i2c, tfa98xx);
	mutex_init(&tfa98xx->dsp_lock);
	init_waitqueue_head(&tfa98xx->wq);

	if (np) {
		ret = tfa98xx_parse_dt(&i2c->dev, tfa98xx, np);
		if (ret) {
			dev_err(&i2c->dev, "Failed to parse DT node\n");
			goto err;
		}
		if (no_start)
			tfa98xx->irq_gpio = -1;
	} else {
		tfa98xx->reset_gpio = -1;
		tfa98xx->irq_gpio = -1;
	}

	if (gpio_is_valid(tfa98xx->reset_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->reset_gpio,
			GPIOF_OUT_INIT_LOW, "TFA98XX_RST");
		if (ret)
			goto err;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, tfa98xx->irq_gpio,
			GPIOF_DIR_IN, "TFA98XX_INT");
		if (ret)
			goto err;
	}

	/* Power up! */
	tfa98xx_ext_reset(tfa98xx);

	if (no_start == 0) {
		ret = regmap_read(tfa98xx->regmap, 0x03, &reg);
		if (ret < 0) {
			dev_err(&i2c->dev,
				"Failed to read Revision register: %d\n", ret);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
			sec_audio_bootlog(3, "%s: Failed to read Revision register: %d\n",
					__func__, ret);
#endif
			return -EIO;
		}

		tfa98xx_log_revision = reg & 0xff;
		tfa98xx_log_subrevision = (reg >> 8) & 0xff;

		switch (reg & 0xff) {
		case 0x72: /* tfa9872 */
			pr_info("TFA9872 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_CALIBRATION_CTL;
			tfa98xx->flags |= TFA98XX_FLAG_REMOVE_PLOP_NOISE;
			tfa98xx->flags |= TFA98XX_FLAG_LP_MODES;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			break;
		case 0x88: /* tfa9888 */
			pr_info("TFA9888 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_STEREO_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x13: /* tfa9912 */
			pr_info("TFA9912 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_MULTI_MIC_INPUTS;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE;
			break;
		case 0x80: /* tfa9890 */
		case 0x81: /* tfa9890 */
			pr_info("TFA9890 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TFA9890_FAM_DEV;
			break;
		case 0x92: /* tfa9891 */
			pr_info("TFA9891 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_SAAM_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_TAPDET_AVAILABLE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TFA9890_FAM_DEV;
			break;
		case 0x12: /* tfa9895 */
			pr_info("TFA9895 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TFA9890_FAM_DEV;
			break;
		case 0x97:
			pr_info("TFA9897 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TFA9890_FAM_DEV;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		case 0x96:
			pr_info("TFA9896 detected\n");
			tfa98xx->flags |= TFA98XX_FLAG_DSP_START_ON_MUTE;
			tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
			tfa98xx->flags |= TFA98XX_FLAG_TFA9890_FAM_DEV;
			tfa98xx->flags |= TFA98XX_FLAG_TDM_DEVICE;
			break;
		default:
			pr_info("Unsupported device revision (0x%x)\n",
				reg & 0xff);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
			sec_audio_bootlog(3, "%s: Unsupported device revision (0x%x)\n",
					__func__, (reg & 0xff));
#endif
			return -EINVAL;
		}
	}

	/* Modify the stream names, by appending the i2c device address.
	 * This is used with multicodec, in order to discriminate devices.
	 * Stream names appear in the dai definition and in the stream.
	 * We create copies of original structures because each device will
	 * have its own instance of this structure, with its own address.
	 */
	dai = devm_kzalloc(&i2c->dev, sizeof(tfa98xx_dai), GFP_KERNEL);
	if (!dai)
		return -ENOMEM;
	memcpy(dai, tfa98xx_dai, sizeof(tfa98xx_dai));

	tfa98xx_append_i2c_address(&i2c->dev,
				i2c,
				NULL,
				0,
				dai,
				ARRAY_SIZE(tfa98xx_dai));

	ret = snd_soc_register_codec(&i2c->dev,
				&soc_codec_dev_tfa98xx, dai,
				ARRAY_SIZE(tfa98xx_dai));

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to register TFA98xx: %d\n", ret);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
		sec_audio_bootlog(3, "%s: Failed to register TFA98xx: %d\n",
							__func__, ret);
#endif
		goto err_off;
	}

	if (gpio_is_valid(tfa98xx->irq_gpio) &&
		!(tfa98xx->flags & TFA98XX_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(tfa98xx->irq_gpio),
					NULL, tfa98xx_irq, irq_flags,
					"tfa98xx", tfa98xx);
		if (ret != 0) {
			dev_err(&i2c->dev, "Failed to request IRQ %d: %d\n",
					gpio_to_irq(tfa98xx->irq_gpio), ret);
			goto err_off;
		}
	} else {
		dev_info(&i2c->dev, "Skipping IRQ registration\n");
		/* disable feature support if gpio was invalid */
		tfa98xx->flags |= TFA98XX_FLAG_SKIP_INTERRUPTS;
	}

#ifdef CONFIG_DEBUG_FS
	tfa98xx_debug_init(tfa98xx, i2c);
#endif
	/* Register the sysfs files for climax backdoor access */
	ret = device_create_bin_file(&i2c->dev, &dev_attr_rw);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");
	ret = device_create_bin_file(&i2c->dev, &dev_attr_reg);
	if (ret)
		dev_info(&i2c->dev, "error creating sysfs files\n");

		tfa98xx_log_i2c_devicenum = i2c->adapter->nr;
		tfa98xx_log_i2c_slaveaddress = i2c->addr;

	pr_info("%s: Probe completed successfully!\n", __func__);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
	sec_audio_bootlog(6, "%s: Probe completed successfully!\n", __func__);
#endif
	return 0;

err_off:
	tfa98xx_unregister_dsp(tfa98xx);
err:
	return ret;
}

static int tfa98xx_i2c_remove(struct i2c_client *i2c)
{
	struct tfa98xx *tfa98xx = i2c_get_clientdata(i2c);

	pr_debug("addr=0x%x\n", i2c->addr);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
	sec_audio_bootlog(6, "%s\n", __func__);
#endif

	tfa98xx_interrupt_enable(tfa98xx, false);

	cancel_delayed_work_sync(&tfa98xx->interrupt_work);
	cancel_delayed_work_sync(&tfa98xx->monitor_work);
	cancel_delayed_work_sync(&tfa98xx->init_work);
#if defined(USE_TFA9891)
	cancel_delayed_work_sync(&tfa98xx->tapdet_work);
#endif

	device_remove_bin_file(&i2c->dev, &dev_attr_reg);
	device_remove_bin_file(&i2c->dev, &dev_attr_rw);
#ifdef CONFIG_DEBUG_FS
	tfa98xx_debug_remove(tfa98xx);
#endif

	tfa98xx_unregister_dsp(tfa98xx);

	snd_soc_unregister_codec(&i2c->dev);

	if (gpio_is_valid(tfa98xx->irq_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->irq_gpio);
	if (gpio_is_valid(tfa98xx->reset_gpio))
		devm_gpio_free(&i2c->dev, tfa98xx->reset_gpio);

	return 0;
}

static const struct i2c_device_id tfa98xx_i2c_id[] = {
	{ "tfa98xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tfa98xx_i2c_id);

#ifdef CONFIG_OF
static const struct of_device_id tfa98xx_dt_match[] = {
	{ .compatible = "nxp,tfa98xx" },
	{ .compatible = "nxp,tfa9872" },
	{ .compatible = "nxp,tfa9888" },
	{ .compatible = "nxp,tfa9890" },
	{ .compatible = "nxp,tfa9891" },
	{ .compatible = "nxp,tfa9895" },
	{ .compatible = "nxp,tfa9896" },
	{ .compatible = "nxp,tfa9897" },
	{ .compatible = "nxp,tfa9912" },
	{ },
};
#endif

static struct i2c_driver tfa98xx_i2c_driver = {
	.driver = {
		.name = "tfa98xx",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(tfa98xx_dt_match),
	},
	.probe = tfa98xx_i2c_probe,
	.remove = tfa98xx_i2c_remove,
	.id_table = tfa98xx_i2c_id,
};

static int trace_level;
module_param(trace_level, int, 0444);
MODULE_PARM_DESC(trace_level, "TFA98xx debug trace level (0=off, bits:1=verbose,2=regdmesg,3=regftrace).");
static int __init tfa98xx_i2c_init(void)
{
	int ret = 0;

	pr_info("TFA98XX driver version %s\n", TFA98XX_VERSION);

	/* Enable debug traces */
	tfa_verbose(trace_level);
	tfa98xx_kmsg_regs = trace_level & 2;
	tfa98xx_ftrace_regs = trace_level & 4;

	ret = i2c_add_driver(&tfa98xx_i2c_driver);

	return ret;
}
module_init(tfa98xx_i2c_init);

static void __exit tfa98xx_i2c_exit(void)
{
	i2c_del_driver(&tfa98xx_i2c_driver);

	kfree(container);
}
module_exit(tfa98xx_i2c_exit);

MODULE_DESCRIPTION("ASoC TFA98XX driver");
MODULE_LICENSE("GPL");
