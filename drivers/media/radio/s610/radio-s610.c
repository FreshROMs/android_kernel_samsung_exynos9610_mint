/*
 * drivers/media/radio/s610/radio-s610.c -- V4L2 driver for S610 chips
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/vmalloc.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-device.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <soc/samsung/exynos-pmu.h>
#include <linux/mcu_ipc.h>

#ifdef CONFIG_SCSC_FM
#include <scsc/scsc_mx.h>
#endif

#include "fm_low_struc.h"
#include "radio-s610.h"

#include "../../../../sound/soc/samsung/abox/abox.h"

static int radio_region;
module_param(radio_region, int, 0);
MODULE_PARM_DESC(radio_region, "Region: 0=Europe/US, 1=Japan");

struct s610_radio;

/* global variable for radio structure */
struct s610_radio *gradio;

#define	FAC_VALUE	16000

static int s610_radio_g_volatile_ctrl(struct v4l2_ctrl *ctrl);
static int s610_radio_s_ctrl(struct v4l2_ctrl *ctrl);
static int s610_core_set_power_state(struct s610_radio *radio,
		u8 next_state);
int fm_set_mute_mode(struct s610_radio *radio, u8 mute_mode_toset);
static int fm_radio_runtime_resume(struct device *dev);
void enable_FM_mux_clk_aud(struct s610_radio *radio);
void disable_clk_gating(struct s610_radio *radio);

static int fm_clk_get(struct s610_radio *radio);
static int fm_clk_prepare(struct s610_radio *radio);
static int fm_clk_enable(struct s610_radio *radio);
static void fm_clk_unprepare(struct s610_radio *radio);
static void fm_clk_disable(struct s610_radio *radio);
static void fm_clk_put(struct s610_radio *radio);

signed int exynos_get_fm_open_status(void);
signed int shared_fm_open_cnt;

extern fm_conf_ini_values low_fm_conf_init;
u32 *fm_spur_init;
u32 *fm_spur_trf_init;
u32 *fm_dual_clk_init;
u32 vol_level_init[FM_RX_VOLUME_GAIN_STEP] = {
	0, 16, 23, 32, 45, 64, 90, 128, 181, 256, 362, 512, 724, 1024, 1447, 2047 };

static const struct v4l2_ctrl_ops s610_ctrl_ops = {
		.s_ctrl			= s610_radio_s_ctrl,
		.g_volatile_ctrl       = s610_radio_g_volatile_ctrl,

};

enum s610_ctrl_idx {
	S610_IDX_CH_SPACING  = 0x01,
	S610_IDX_CH_BAND    =  0x02,
	S610_IDX_SOFT_STEREO_BLEND = 0x03,
	S610_IDX_SOFT_STEREO_BLEND_COEFF = 0x04,
	S610_IDX_SOFT_MUTE_COEFF = 0x05,
	S610_IDX_RSSI_CURR	= 0x06,
	S610_IDX_SNR_CURR	= 0x07,
	S610_IDX_SEEK_CANCEL	= 0x08,
	S610_IDX_SEEK_MODE	= 0x09,
	S610_IDX_RDS_ON = 0x0A,
	S610_IDX_IF_COUNT1 = 0x0B,
	S610_IDX_IF_COUNT2 = 0x0C,
	S610_IDX_RSSI_TH	= 0x0D,
	S610_IDX_KERNEL_VER	= 0x0E,
	S610_IDX_SOFT_STEREO_BLEND_REF	= 0x0F
};

static struct v4l2_ctrl_config s610_ctrls[] = {
		/**
		 * S610 during its station seeking(or tuning) process uses several
		 * parameters to detrmine if "the station" is valid:
		 *
		 *	- Signal's RSSI(in dBuV) must be greater than
		 *	#V4L2_CID_S610_RSSI_THRESHOLD
		 */
		[S610_IDX_CH_SPACING] = {/*0x01*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_CH_SPACING,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Channel Spacing",
				.min	= 0,
				.max	= 2,
				.step	= 1,
		},
		[S610_IDX_CH_BAND] = { /*0x02*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_CH_BAND,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Channel Band",
				.min	= 0,
				.max	= 2,
				.step	= 1,
		},
		[S610_IDX_SOFT_STEREO_BLEND] = { /*0x03*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SOFT_STEREO_BLEND,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Soft Stereo Blend",
				.min	=     0,
				.max	= 1,
				.step	= 1,
		},
		[S610_IDX_SOFT_STEREO_BLEND_COEFF] = { /*0x04*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SOFT_STEREO_BLEND_COEFF,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Soft Stereo Blend COEFF",
				.min	=     0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_SOFT_MUTE_COEFF] = { /*0x05*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SOFT_MUTE_COEFF,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Soft Mute COEFF Set",
				.min	=     0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_RSSI_CURR] = { /*0x06*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_RSSI_CURR,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "RSSI Current",
				.min	=     0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_SNR_CURR] = { /*0x07*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SNR_CURR,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "SNR Current",
				.min	=     0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_SEEK_CANCEL] = { /*0x08*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SEEK_CANCEL,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Seek Cancel",
				.min	=     0,
				.max	= 1,
				.step	= 1,
		},
		[S610_IDX_SEEK_MODE] = { /*0x09*/
				.ops	= &s610_ctrl_ops,
				.id	= V4L2_CID_S610_SEEK_MODE,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Seek Mode",
				.min	=     0,
				.max	= 4,
				.step	= 1,
		},
		[S610_IDX_RDS_ON] = { /*0x0A*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_RDS_ON,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "RDS ON",
				.min	=	  0,
				.max	= 0x0F,
				.step	= 1,
		},
		[S610_IDX_IF_COUNT1] = { /*0x0B*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_IF_COUNT1,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "IF_COUNT1",
				.min	=	  0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_IF_COUNT2] = { /*0x0C*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_IF_COUNT2,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "IF_COUNT2",
				.min	=	  0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_RSSI_TH] = { /*0x0D*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_RSSI_TH,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "RSSI Th",
				.min	=	  0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_KERNEL_VER] = { /*0x0E*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_KERNEL_VER,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "KERNEL_VER",
				.min	=	  0,
				.max	= 0xffff,
				.step	= 1,
		},
		[S610_IDX_SOFT_STEREO_BLEND_REF] = { /*0x0F*/
				.ops	= &s610_ctrl_ops,
				.id = V4L2_CID_S610_SOFT_STEREO_BLEND_REF,
				.type	= V4L2_CTRL_TYPE_INTEGER,
				.name	= "Soft Stereo Blend Ref",
				.min	=	  0,
				.max	= 0xffff,
				.step	= 1,
		},
};

static const struct v4l2_frequency_band s610_bands[] = {
		[0] = {
				.type		= V4L2_TUNER_RADIO,
				.index		= S610_BAND_FM,
				.capability	= V4L2_TUNER_CAP_LOW
				| V4L2_TUNER_CAP_STEREO
				| V4L2_TUNER_CAP_RDS
				| V4L2_TUNER_CAP_RDS_BLOCK_IO
				| V4L2_TUNER_CAP_FREQ_BANDS,
				/* default region Eu/US */
				.rangelow	= 87500*FAC_VALUE,
				.rangehigh	= 108000*FAC_VALUE,
				.modulation	= V4L2_BAND_MODULATION_FM,
		},
		[1] = {
				.type		= V4L2_TUNER_RADIO,
				.index		= S610_BAND_FM,
				.capability	= V4L2_TUNER_CAP_LOW
				| V4L2_TUNER_CAP_STEREO
				| V4L2_TUNER_CAP_RDS
				| V4L2_TUNER_CAP_RDS_BLOCK_IO
				| V4L2_TUNER_CAP_FREQ_BANDS,
				/* Japan region */
				.rangelow	= 76000*FAC_VALUE,
				.rangehigh	= 90000*FAC_VALUE,
				.modulation	= V4L2_BAND_MODULATION_FM,
		},
		[2] = {
				.type		= V4L2_TUNER_RADIO,
				.index		= S610_BAND_FM,
				.capability = V4L2_TUNER_CAP_LOW
				| V4L2_TUNER_CAP_STEREO
				| V4L2_TUNER_CAP_RDS
				| V4L2_TUNER_CAP_RDS_BLOCK_IO
				| V4L2_TUNER_CAP_FREQ_BANDS,
				/* custom region */
				.rangelow	= 76000*FAC_VALUE,
				.rangehigh	= 108000*FAC_VALUE,
				.modulation = V4L2_BAND_MODULATION_FM,
		},
};

/* Region info */
static struct region_info region_configs[] = {
		/* Europe/US */
		{
			.chanl_space = FM_CHANNEL_SPACING_200KHZ * FM_FREQ_MUL,
			.bot_freq = 87500,	/* 87.5 MHz */
			.top_freq = 108000,	/* 108 MHz */
			.fm_band = 0,
		},
		/* Japan */
		{
			.chanl_space = FM_CHANNEL_SPACING_200KHZ * FM_FREQ_MUL,
			.bot_freq = 76000,	/* 76 MHz */
			.top_freq = 90000,	/* 90 MHz */
			.fm_band = 1,
		},
		/* Custom */
		{
			.chanl_space = FM_CHANNEL_SPACING_200KHZ * FM_FREQ_MUL,
			.bot_freq = 76000,	/* 76 MHz */
			.top_freq = 108000,	/* 108 MHz */
			.fm_band = 2,
		},
};

static inline bool s610_radio_freq_is_inside_of_the_band(u32 freq, int band)
{
	return freq >= region_configs[radio_region].bot_freq &&
			freq <= region_configs[radio_region].top_freq;
}

static inline struct s610_radio *
v4l2_dev_to_radio(struct v4l2_device *d)
{
	return container_of(d, struct s610_radio, v4l2dev);
}

static inline struct s610_radio *
v4l2_ctrl_handler_to_radio(struct v4l2_ctrl_handler *d)
{
	return container_of(d, struct s610_radio, ctrl_handler);
}

/*
 * s610_vidioc_querycap - query device capabilities
 */
static int s610_radio_querycap(struct file *file, void *priv,
		struct v4l2_capability *capability)
{
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	s610_core_lock(radio->core);

	strlcpy(capability->driver, radio->v4l2dev.name,
			sizeof(capability->driver));
	strlcpy(capability->card,   DRIVER_CARD, sizeof(capability->card));
	snprintf(capability->bus_info, sizeof(capability->bus_info),
			"platform:%s", radio->v4l2dev.name);

	capability->device_caps = V4L2_CAP_TUNER
			| V4L2_CAP_RADIO | V4L2_CAP_RDS_CAPTURE
			| V4L2_CAP_READWRITE | V4L2_CAP_HW_FREQ_SEEK;

	capability->capabilities = capability->device_caps
			| V4L2_CAP_DEVICE_CAPS;

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return 0;
}

static int s610_radio_enum_freq_bands(struct file *file, void *priv,
		struct v4l2_frequency_band *band)
{
	int ret;
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	if (band->tuner != 0)
		return -EINVAL;

	s610_core_lock(radio->core);

	if (band->index == S610_BAND_FM) {
		*band = s610_bands[radio_region];
		ret = 0;
	} else {
		ret = -EINVAL;
	}

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_g_tuner(struct file *file, void *priv,
		struct v4l2_tuner *tuner)
{
	int ret;
	struct s610_radio *radio = video_drvdata(file);
	u16 payload;

	FUNC_ENTRY(radio);

	if (tuner->index != 0)
		return -EINVAL;

	s610_core_lock(radio->core);

	tuner->type       = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_LOW
			| V4L2_TUNER_CAP_STEREO
			| V4L2_TUNER_CAP_HWSEEK_BOUNDED
			| V4L2_TUNER_CAP_HWSEEK_WRAP
			| V4L2_TUNER_CAP_HWSEEK_PROG_LIM;


	strlcpy(tuner->name, "FM", sizeof(tuner->name));

	tuner->rxsubchans = V4L2_TUNER_SUB_RDS;
	tuner->capability |= V4L2_TUNER_CAP_RDS
			| V4L2_TUNER_CAP_RDS_BLOCK_IO
			| V4L2_TUNER_CAP_FREQ_BANDS;

	/* Read register : MONO, Stereo mode */
	/*ret = low_get_most_mode(radio, &payload);*/
	payload = radio->low->fm_state.force_mono ?
			0 : MODE_MASK_MONO_STEREO;
	radio->audmode = payload;
	tuner->audmode = radio->audmode;
	tuner->afc = 1;
	tuner->rangelow = s610_bands[radio_region].rangelow;
	tuner->rangehigh = s610_bands[radio_region].rangehigh;

	ret = low_get_search_lvl(radio, &payload);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
			"Failed to read reg for SEARCH_LVL\n");
		ret = -EIO;
		tuner->signal = 0;
	} else {
		tuner->signal = 0;
		if (payload & 0x80)
			tuner->signal = 0xFF00;
		else
			tuner->signal = 0;

		tuner->signal |= (payload & 0xFF);
	}

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_s_tuner(struct file *file, void *priv,
		const struct v4l2_tuner *tuner)
{
	int ret;
	struct s610_radio *radio = video_drvdata(file);
	u16 payload;

	FUNC_ENTRY(radio);

	if (tuner->index != 0)
		return -EINVAL;

	s610_core_lock(radio->core);

	if (tuner->audmode == V4L2_TUNER_MODE_MONO ||
			tuner->audmode == V4L2_TUNER_MODE_STEREO)
		radio->audmode = tuner->audmode;
	else
		radio->audmode = V4L2_TUNER_MODE_STEREO;

	payload = radio->audmode;
	ret = low_set_most_mode(radio, payload);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
			"Failed to write reg for MOST MODE clear\n");
		ret = -EIO;
	}

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_pretune(struct s610_radio *radio)
{
	int ret = 0;
	u16 payload;

	FUNC_ENTRY(radio);

	/*ret = low_get_flag(radio, &payload);*/
	payload = fm_get_flags(radio);

	payload = 0;
	ret = low_set_mute_state(radio, payload);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
				"Failed to write reg for MUTE state clean\n");
		return -EIO;
	}

	payload = radio->low->fm_state.force_mono ?
			0 : MODE_MASK_MONO_STEREO;

	payload = 0;
	ret = low_set_most_blend(radio, payload);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
				"Failed to write reg for MOST blend clean\n");
		return -EIO;
	}

	payload = 0;
	fm_set_band(radio, payload);

	payload = 0;
	ret = low_set_demph_mode(radio, payload);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
			"Failed to write reg for FM_SUBADDR_DEMPH_MODE clean\n");
		return -EIO;
	}

	payload = 0;
	radio->low->fm_state.use_rbds =
		((payload & RDS_SYSTEM_MASK_RDS) != 0);
	radio->low->fm_state.save_eblks =
		((payload & RDS_SYSTEM_MASK_EBLK) == 0);

	radio->low->fm_state.rds_mem_thresh = RDS_MEM_MAX_THRESH;

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_g_frequency(struct file *file, void *priv,
		struct v4l2_frequency *f)
{
	struct s610_radio *radio = video_drvdata(file);
	u32 payload;

	FUNC_ENTRY(radio);

	if (f->tuner != 0 || f->type  != V4L2_TUNER_RADIO)
		return -EINVAL;

	s610_core_lock(radio->core);

	payload = radio->low->fm_state.freq;
	f->frequency = payload;
	f->frequency *= FAC_VALUE;

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	FDEBUG(radio,
		"%s():freq.tuner:%d, type:%d, freq:%d, real-freq:%d\n",
		__func__, f->tuner, f->type, f->frequency,
		f->frequency/FAC_VALUE);

	return 0;
}

int fm_set_frequency(struct s610_radio *radio, u32 freq)
{
	unsigned long timeleft;
	u32 curr_frq, intr_flag;
	u32 curr_frq_in_khz;
	int ret;
	u32 payload;
	u16 payload16;

	FUNC_ENTRY(radio);

	/* Calculate frequency with offset and set*/
	payload = real_to_api(freq);

	low_set_freq(radio, payload);

	/* Read flags - just to clear any pending interrupts if we had */
	payload = fm_get_flags(radio);

	/* Enable FR, BL interrupts */
	intr_flag = radio->irq_mask;
	radio->irq_mask = (FM_EVENT_TUNED | FM_EVENT_BD_LMT);

	low_get_search_lvl(radio, (u16 *) &payload16);
	if (!payload16) {
		payload16 = FM_DEFAULT_RSSI_THRESHOLD;
		low_set_search_lvl(radio, (u16) payload16);
	}

	if (radio->low->fm_config.search_conf.normal_ifca_m == 0)
		radio->low->fm_config.search_conf.normal_ifca_m =
			low_fm_conf_init.search_conf.normal_ifca_m;

	if (radio->low->fm_config.search_conf.weak_ifca_m == 0)
		radio->low->fm_config.search_conf.weak_ifca_m =
			low_fm_conf_init.search_conf.weak_ifca_m;

	FDEBUG(radio, "%s(): ifcount:W-%d N-%d\n",	__func__,
		radio->low->fm_config.search_conf.weak_ifca_m,
		radio->low->fm_config.search_conf.normal_ifca_m);

	if (!radio->low->fm_config.mute_coeffs_soft) {
		radio->low->fm_config.mute_coeffs_soft =
			low_fm_conf_init.mute_coeffs_soft;
	}

	if (!radio->low->fm_config.blend_coeffs_soft) {
		radio->low->fm_config.blend_coeffs_soft =
			low_fm_conf_init.blend_coeffs_soft;
	}

	if (!radio->low->fm_config.blend_coeffs_switch) {
		radio->low->fm_config.blend_coeffs_switch =
			low_fm_conf_init.blend_coeffs_switch;
	}

	if (!radio->low->fm_config.blend_coeffs_dis)
		radio->low->fm_config.blend_coeffs_dis =
		low_fm_conf_init.blend_coeffs_dis;

	fm_set_blend_mute(radio);

	init_completion(&radio->flags_set_fr_comp);

	/* Start tune */
	payload = FM_TUNER_PRESET_MODE;
	ret = low_set_tuner_mode(radio, payload);
	if (ret != 0) {
		ret = -EIO;
		goto exit;
	}

	timeleft = jiffies_to_msecs(FM_DRV_TURN_TIMEOUT);
	/* Wait for tune ended interrupt */
	timeleft = wait_for_completion_timeout(&radio->flags_set_fr_comp,
			FM_DRV_TURN_TIMEOUT);

	if (!timeleft) {
		dev_err(radio->v4l2dev.dev,
				"Timeout(%d sec),didn't get tune ended int\n",
				jiffies_to_msecs(FM_DRV_TURN_TIMEOUT) / 1000);
		ret = -ETIMEDOUT;
		goto exit;
	}

	/* Read freq back to confirm */
	curr_frq = radio->low->fm_state.freq;
	curr_frq_in_khz = curr_frq;
	if (curr_frq_in_khz != freq) {
		dev_err(radio->v4l2dev.dev,
				"Set Freq (%d) but requested freq (%d)\n",
				curr_frq_in_khz, freq);
		ret = -ENODATA;
		goto exit;
	}

	FDEBUG(radio,
			"%s():--> Set frequency: %d Read frequency:%d\n",
			__func__,  freq, curr_frq_in_khz);

	/* Update local cache  */
	radio->freq = curr_frq_in_khz;
exit:
	/* Re-enable default FM interrupts */
	radio->irq_mask = intr_flag;

	FDEBUG(radio, "wait_atomic: %d\n", radio->wait_atomic);

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_s_frequency(struct file *file, void *priv,
		const struct v4l2_frequency *f)
{
	int ret;
	u32 freq = f->frequency;
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	if (f->tuner != 0 ||
			f->type  != V4L2_TUNER_RADIO)
		return -EINVAL;

	if (!wake_lock_active(&radio->wakelock))
		wake_lock(&radio->wakelock);

	s610_core_lock(radio->core);
	FDEBUG(radio, "%s():freq:%d, real-freq:%d\n",
			__func__, f->frequency, f->frequency/FAC_VALUE);

	freq /= FAC_VALUE;

	freq = clamp(freq,
			region_configs[radio_region].bot_freq,
			region_configs[radio_region].top_freq);
	if (!s610_radio_freq_is_inside_of_the_band(freq,
			radio_region)) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = fm_set_frequency(radio, freq);

unlock:
	s610_core_unlock(radio->core);

	if (wake_lock_active(&radio->wakelock))
		wake_unlock(&radio->wakelock);

	FUNC_EXIT(radio);

	FDEBUG(radio,
			"%s():v4l2_frequency.tuner:%d,type:%d,frequency:%d\n",
			__func__, f->tuner, f->type, freq);

	return ret;
}

int fm_rx_seek(struct s610_radio *radio, u32 seek_upward,
		u32 wrap_around, u32 spacing, u32 freq_low, u32 freq_hi)
{
	u32 curr_frq, save_freq;
	u32 payload, int_reason, intr_flag, tune_mode;
	u32 space, upward;
	u16 payload16;
	unsigned long timeleft;
	int ret;
	int	bl_1st, bl_2nd, bl_3nd;

	FUNC_ENTRY(radio);

	radio->seek_freq = 0;
	radio->wrap_around = 0;
	bl_1st = 0;
	bl_2nd = 0;
	bl_3nd = 0;

	payload = fm_get_flags(radio);

	/* Set channel spacing */
	if (spacing > 0 && spacing <= 50)
		payload = 0; /*CHANNEL_SPACING_50KHZ*/
	else if (spacing > 50 && spacing <= 100)
		payload = 1; /*CHANNEL_SPACING_100KHZ*/
	else
		payload = 2; /*CHANNEL_SPACING_200KHZ;*/

	FDEBUG(radio, "%s(): init: spacing: %d\n", __func__, payload);

	radio->low->fm_tuner_state.freq_step =
			radio->low->fm_freq_steps[payload];
	radio->region.chanl_space = 50 * (1 << payload);

	/* Check the offset in order to be aligned to the channel spacing*/
	space = radio->region.chanl_space;

	/* Set search direction (0:Seek Up, 1:Seek Down) */
	payload = (seek_upward ? FM_SEARCH_DIRECTION_UP :
		FM_SEARCH_DIRECTION_DOWN);
	upward = payload;
	radio->low->fm_state.search_down =
			!!(payload & SEARCH_DIR_MASK);
	FDEBUG(radio, "%s(): direction: %d\n",	__func__, payload);

	save_freq = freq_low;
	radio->seek_freq = save_freq;
	tune_mode = FM_TUNER_AUTONOMOUS_SEARCH_MODE_NEXT;

	if (radio->low->fm_state.rssi_limit_search == 0)
		low_set_search_lvl(radio, (u16)FM_DEFAULT_RSSI_THRESHOLD);

	if (radio->low->fm_config.search_conf.normal_ifca_m == 0)
		radio->low->fm_config.search_conf.normal_ifca_m =
			low_fm_conf_init.search_conf.normal_ifca_m;

	if (radio->low->fm_config.search_conf.weak_ifca_m == 0)
		radio->low->fm_config.search_conf.weak_ifca_m =
			low_fm_conf_init.search_conf.weak_ifca_m;

		FDEBUG(radio, "%s(): ifcount:W-%d N-%d\n",	__func__,
		radio->low->fm_config.search_conf.weak_ifca_m,
		radio->low->fm_config.search_conf.normal_ifca_m);

again:
	curr_frq = freq_low;
	if ((freq_low == region_configs[radio_region].bot_freq)
		&& (upward == FM_SEARCH_DIRECTION_DOWN))
		curr_frq = region_configs[radio_region].top_freq;

	if ((freq_low == region_configs[radio_region].top_freq)
		&& (upward == FM_SEARCH_DIRECTION_UP))
		curr_frq = region_configs[radio_region].bot_freq;

	payload = curr_frq;
	low_set_freq(radio, payload);

	FDEBUG(radio, "%s(): curr_freq: %d, freq hi: %d\n",
		__func__, curr_frq, freq_hi);

	/* Enable FR, BL interrupts */
	intr_flag = radio->irq_mask;
	radio->irq_mask = (FM_EVENT_TUNED | FM_EVENT_BD_LMT);
	low_get_search_lvl(radio, (u16 *) &payload16);
	if (!payload16) {
		payload16 = FM_DEFAULT_RSSI_THRESHOLD;
		low_set_search_lvl(radio, (u16) payload16);
	}
	FDEBUG(radio, "%s(): SEARCH_LVL1: 0x%x\n",  __func__, payload16);

	if (!radio->low->fm_config.mute_coeffs_soft) {
		radio->low->fm_config.mute_coeffs_soft =
			low_fm_conf_init.mute_coeffs_soft;
	}

	if (!radio->low->fm_config.blend_coeffs_soft) {
		radio->low->fm_config.blend_coeffs_soft =
			low_fm_conf_init.blend_coeffs_soft;
	}

	if (!radio->low->fm_config.blend_coeffs_switch) {
		radio->low->fm_config.blend_coeffs_switch =
			low_fm_conf_init.blend_coeffs_switch;
	}

	if (!radio->low->fm_config.blend_coeffs_dis)
		radio->low->fm_config.blend_coeffs_dis =
		low_fm_conf_init.blend_coeffs_dis;

	fm_set_blend_mute(radio);

	reinit_completion(&radio->flags_seek_fr_comp);

	payload = tune_mode;
	FDEBUG(radio,
			"%s(): turn start mode: 0x%x\n",  __func__, payload);
	ret = low_set_tuner_mode(radio, (u16) payload);
	if (ret != 0)
		return -EIO;

	/* Wait for tune ended interrupt */
	timeleft =
		wait_for_completion_timeout(&radio->flags_seek_fr_comp,
		FM_DRV_SEEK_TIMEOUT);

	/* seek cancel status */
	if (radio->seek_status == FM_TUNER_STOP_SEARCH_MODE) {
		dev_info(radio->dev, ">>> rev seek cancel");
		return -ENODATA;
	}

	FDEBUG(radio,
		"FDEBUG > Seek done rev complete!! freq %d, irq_flag: 0x%x, bl:%d\n",
		radio->low->fm_state.freq, radio->irq_flag, bl_1st);

	int_reason = radio->low->fm_state.flags &
			(FM_EVENT_TUNED | FM_EVENT_BD_LMT);

	if ((save_freq == region_configs[radio_region].bot_freq)
		|| (save_freq == region_configs[radio_region].top_freq))
			bl_1st = 1;

	if ((save_freq == region_configs[radio_region].bot_freq)
		&& (upward == FM_SEARCH_DIRECTION_UP)) {
		bl_1st = 0;
		bl_2nd = 1;
		tune_mode = FM_TUNER_AUTONOMOUS_SEARCH_MODE;
	}
	if ((save_freq == region_configs[radio_region].top_freq)
		&& (upward == FM_SEARCH_DIRECTION_DOWN)) {
		bl_1st = 0;
		bl_2nd = 1;
		tune_mode = FM_TUNER_AUTONOMOUS_SEARCH_MODE;
	}

	if ((int_reason & FM_EVENT_BD_LMT) && (bl_1st == 0) && (bl_3nd == 0)) {
		if (wrap_around) {
			freq_low = radio->low->fm_state.freq;
			bl_1st = 1;
			if (bl_2nd)
				bl_3nd = 1;

			/* Re-enable default FM interrupts */
			radio->irq_mask = intr_flag;
			radio->wrap_around = 1;
			FDEBUG(radio, "> bl set %d, %d, save %d\n",
				radio->seek_freq, radio->wrap_around, save_freq);

			goto again;
		} else
			ret = -EINVAL;
	}

	/* Read freq to know where operation tune operation stopped */
	payload = radio->low->fm_state.freq;
	radio->freq = payload;

	dev_info(radio->v4l2dev.dev, "Seek freq %d\n", radio->freq);

	radio->seek_freq = 0;
	radio->wrap_around = 0;

	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_s_hw_freq_seek(struct file *file, void *priv,
		const struct v4l2_hw_freq_seek *seek)
{
	int ret = 0;
	struct s610_radio *radio = video_drvdata(file);
	u32 seek_low, seek_hi, seek_spacing;

	FUNC_ENTRY(radio);

	if (file->f_flags & O_NONBLOCK)
		return -EAGAIN;

	if (seek->tuner != 0 ||
			seek->type  != V4L2_TUNER_RADIO)
		return -EINVAL;

	if (seek->rangelow >= seek->rangehigh)
		ret = -EINVAL;

	seek_low = radio->low->fm_state.freq;
	if (seek->seek_upward)
		seek_hi = region_configs[radio_region].top_freq;
	else
		seek_hi = region_configs[radio_region].bot_freq;

	seek_spacing = seek->spacing / 1000;
	FDEBUG(radio, "%s(): get freq low: %d, freq hi: %d\n",
		__func__, seek_low, seek_hi);
	FDEBUG(radio,
			"%s(): upward:%d, warp: %d, spacing: %d\n",  __func__,
		seek->seek_upward, seek->wrap_around, seek->spacing);

	if (!wake_lock_active(&radio->wakelock))
		wake_lock(&radio->wakelock);

	ret = fm_rx_seek(radio, seek->seek_upward, seek->wrap_around,
			seek_spacing, seek_low, seek_hi);
	if (ret < 0)
		dev_err(radio->v4l2dev.dev, "RX seek failed - %d\n", ret);

	if (wake_lock_active(&radio->wakelock))
		wake_unlock(&radio->wakelock);

	FUNC_EXIT(radio);

	return ret;
}

/* Configures mute mode (Mute Off/On/Attenuate) */
int fm_set_mute_mode(struct s610_radio *radio, u8 mute_mode_toset)
{
	u16 payload = radio->mute_mode;
	int ret = 0;

	FUNC_ENTRY(radio);

	radio->mute_mode = mute_mode_toset;

	switch (radio->mute_mode) {
	case FM_MUTE_ON:
		payload = FM_RX_AC_MUTE_MODE;
		break;

	case FM_MUTE_OFF:
		payload = FM_RX_UNMUTE_MODE;
		break;
	}

	/* Write register : mute */
	ret = low_set_mute_state(radio, payload);
	if (ret != 0)
		ret = -EIO;

	FUNC_EXIT(radio);

	return ret;
}

/* Enable/Disable RDS */
int fm_set_rds_mode(struct s610_radio *radio, u8 rds_en_dis)
{
	int ret;
	u16 payload;
	int ii;
	u32 fifo_tmp;

	FUNC_ENTRY(radio);

	if (rds_en_dis != FM_RDS_ENABLE && rds_en_dis != FM_RDS_DISABLE) {
		dev_err(radio->v4l2dev.dev, "Invalid rds option\n");
		return -EINVAL;
	}

	if (rds_en_dis == FM_RDS_ENABLE) {
		if (!wake_lock_active(&radio->rdswakelock))
			wake_lock(&radio->rdswakelock);
		mdelay(100);
		atomic_set(&radio->is_rds_new, 0);
		radio->rds_cnt_mod = 0;
		radio->rds_n_count = 0;
		radio->rds_r_count = 0;
		radio->rds_read_cnt = 0;
		radio->rds_sync_loss_cnt = 0;

		init_waitqueue_head(&radio->core->rds_read_queue);

		payload = radio->core->power_mode;
		payload |= S610_POWER_ON_RDS;
		ret = s610_core_set_power_state(radio,
				payload);
		if (ret != 0) {
			dev_err(radio->v4l2dev.dev,
					"Failed to set for RDS power state\n");
			return -EIO;
		}

		/* Write register : RDS on */
		/* Turn on RDS and RDS circuit */
		fm_rds_on(radio);

		/* Write register : clear RDS cache */
		/* flush RDS buffer */
		payload = FM_RX_RDS_FLUSH_FIFO;
		ret = low_set_rds_cntr(radio, payload);
		if (ret != 0) {
			dev_err(radio->v4l2dev.dev,
					"Failed to write reg for default RDS_CNTR\n");
			return -EIO;
		}

		/* Write register : clear panding interrupt flags */
		/* Read flags - just to clear any pending interrupts. */
		payload = fm_get_flags(radio);

		/* Write register : set RDS memory depth */
		/* Set RDS FIFO threshold value */
		if (radio->rds_parser_enable)
			radio->low->fm_state.rds_mem_thresh = RDS_MEM_MAX_THRESH_PARSER;
		else
		radio->low->fm_state.rds_mem_thresh = RDS_MEM_MAX_THRESH;

		/* Write register : set RDS interrupt enable */
		/* Enable RDS interrupt */
		radio->irq_mask |= FM_EVENT_BUF_FUL;

		/* Update our local flag */
		radio->rds_flag = FM_RDS_ENABLE;

		/* RDS parser reset */
		if (radio->rds_parser_enable)
			fm_rds_parser_reset(&(radio->pi));

		/* FIFO clear */
		for (ii = 0; ii < 32; ii++)
			fifo_tmp = fmspeedy_get_reg_work(0xFFF3C0);

#ifdef	RDS_POLLING_ENABLE
		fm_rds_periodic_update((unsigned long) radio);
#endif	/*RDS_POLLING_ENABLE*/
	} else if (
		rds_en_dis == FM_RDS_DISABLE) {
		payload = radio->core->power_mode;
		payload &= ~S610_POWER_ON_RDS;
		ret = s610_core_set_power_state(radio,
				payload);
		if (ret != 0) {
			dev_err(radio->v4l2dev.dev,
					"Failed to set for RDS power state\n");
			return -EIO;
		}

		/* Write register : RDS off */
		/* Turn off RX RDS */
		fm_rds_off(radio);

		/* Update RDS local cache */
		radio->irq_mask &= ~(FM_EVENT_BUF_FUL);
		radio->rds_flag = FM_RDS_DISABLE;
#ifdef	RDS_POLLING_ENABLE
		fm_rds_periodic_cancel((unsigned long) radio);
#endif	/*RDS_POLLING_ENABLE*/

		/* Service pending read */
		wake_up_interruptible(&radio->core->rds_read_queue);

		if (wake_lock_active(&radio->rdswakelock))
			wake_unlock(&radio->rdswakelock);
	}

	FUNC_EXIT(radio);

	return ret;
}

int fm_set_deemphasis(struct s610_radio *radio, u16 vol_to_set)
{
	int ret = 0;
	u16 payload;

	payload = vol_to_set;
	/* Write register : deemphasis */
	ret = low_set_demph_mode(radio, payload);
	if (ret != 0)
		return -EIO;

	radio->deemphasis_mode = vol_to_set;

	return ret;
}

static int s610_radio_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	struct s610_radio *radio = v4l2_ctrl_handler_to_radio(ctrl->handler);
	u16 payload;
	int ret = 0;

	FUNC_ENTRY(radio);
	if (!radio)
		return -ENODEV;

	s610_core_lock(radio->core);

	switch (ctrl->id) {
	case V4L2_CID_S610_CH_SPACING:
		payload = radio->low->fm_tuner_state.freq_step / 100;
		FDEBUG(radio, "%s(), FREQ_STEP val:%d, ret : %d\n",
				__func__, payload,  ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_CH_BAND:
		payload = radio->low->fm_state.band;
		FDEBUG(radio, "%s(), BAND val:%d, ret : %d\n",
				__func__, payload, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND:
		payload = radio->low->fm_state.use_switched_blend ?
			MODE_MASK_BLEND : 0;
		FDEBUG(radio, "%s(), MOST_BLEND val:%d, ret : %d\n",
				__func__, ctrl->val, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND_COEFF:
		payload = radio->low->fm_config.blend_coeffs_soft;
		FDEBUG(radio, "%s(),BLEND_COEFF_SOFT val:%d, ret: %d\n",
				__func__, ctrl->val, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_SOFT_MUTE_COEFF:
		payload = radio->low->fm_config.mute_coeffs_soft;
		FDEBUG(radio, "%s(), MUTE_COEFF_SOFT val:%d, ret: %d\n",
				__func__, ctrl->val, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_RSSI_CURR:
		fm_update_rssi(radio);
		ctrl->val  = radio->low->fm_state.rssi;
		FDEBUG(radio, "%s(), RSSI_CURR val:%d, ret : %d\n",
				__func__, ctrl->val, ret);
		break;
	case V4L2_CID_S610_SNR_CURR:
		radio->low->fm_state.snr = fmspeedy_get_reg(0xFFF2C5);
		payload = radio->low->fm_state.snr;
		FDEBUG(radio, "%s(), SNR_CURR val:%d, ret : %d\n",
				__func__, payload, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_SEEK_CANCEL:
		ctrl->val = 0;
		break;
	case V4L2_CID_S610_SEEK_MODE:
		if (radio->seek_mode == FM_TUNER_AUTONOMOUS_SEARCH_MODE_NEXT)
			ctrl->val = 4;
		else
			ctrl->val = radio->seek_mode;

		FDEBUG(radio, "%s(), SEEK_MODE val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_RDS_ON:
		if (radio->low->fm_state.fm_pwr_on & S610_POWER_ON_RDS)
			ctrl->val = FM_RDS_ENABLE;
		else
			ctrl->val = FM_RDS_DISABLE;
		FDEBUG(radio, "%s(), RDS_ON:%d, ret: %d\n", __func__,
				ctrl->val, ret);
		break;
	case V4L2_CID_S610_IF_COUNT1:
		payload = radio->low->fm_config.search_conf.weak_ifca_m;
		FDEBUG(radio, "%s(), IF_CNT1 val:%d, ret : %d\n",
			__func__, ctrl->val, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_IF_COUNT2:
		payload = radio->low->fm_config.search_conf.normal_ifca_m;
		FDEBUG(radio, "%s(), IF_CNT2 val:%d, ret : %d\n",
			__func__, ctrl->val, ret);
		ctrl->val = payload;
		break;
	case V4L2_CID_S610_RSSI_TH:
		ctrl->val  = radio->low->fm_state.rssi;
		FDEBUG(radio, "%s(), RSSI_CURR val:%d, ret : %d\n",
				__func__, ctrl->val, ret);
		break;
	case V4L2_CID_S610_KERNEL_VER:
		ctrl->val |= FM_RADIO_RDS_PARSER_VER_CHECK;
		FDEBUG(radio, "%s(), KERNEL_VER val:%d, ret : %d\n",
				__func__, ctrl->val, ret);
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND_REF:
		ctrl->val = radio->rssi_ref_enable;
		FDEBUG(radio, "%s(), SOFT_STEREO_BLEND_REF val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;

	default:
		ret = -EINVAL;
		dev_err(radio->v4l2dev.dev,
				"g_volatile_ctrl Unknown IOCTL: %d\n",
				(int) ctrl->id);
		break;
	}

	s610_core_unlock(radio->core);
	FUNC_EXIT(radio);

	return ret;
}

static int s610_radio_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int ret = 0;
	u16 payload = 0;
	struct s610_radio *radio = v4l2_ctrl_handler_to_radio(ctrl->handler);

	FUNC_ENTRY(radio);

	s610_core_lock(radio->core);
	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:  /* set mute */
		ret = fm_set_mute_mode(radio, (u8)ctrl->val);
		FDEBUG(radio, "%s(), mute val:%d, ret : %d\n", __func__,
				ctrl->val, ret);
		if (ret != 0)
			ret = -EIO;
		break;
	case V4L2_CID_AUDIO_VOLUME:  /* set volume gain */
		fm_set_audio_gain(radio, (u8)ctrl->val);
		FDEBUG(radio, "%s(), volume val:%d, ret : %d\n", __func__,
				ctrl->val, ret);
		break;
	case V4L2_CID_TUNE_DEEMPHASIS:
		if (ctrl->val == 0)
			break;
		payload = (u16)ctrl->val;
		if (ctrl->val > 0)
			payload -= 1;
		ret = fm_set_deemphasis(radio, payload);
		FDEBUG(radio, "%s(), deemphasis val:%d, ret : %d, payload:%d\n", __func__,
				ctrl->val, ret, payload);
		if (ret != 0)
			ret = -EINVAL;
		break;
	case V4L2_CID_S610_CH_SPACING:
		radio->freq_step = ctrl->val;
		radio->low->fm_tuner_state.freq_step =
		radio->low->fm_freq_steps[ctrl->val];
		FDEBUG(radio, "%s(), FREQ_STEP val:%d, ret : %d\n",
			__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_CH_BAND:
		radio_region = ctrl->val;
		radio->radio_region = ctrl->val;
		payload = ctrl->val;
		fm_set_band(radio, payload);
		FDEBUG(radio, "%s(), BAND val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND:
		ret = low_set_most_blend(radio, ctrl->val);
		FDEBUG(radio, "%s(), MOST_BLEND val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		if (ret != 0)
			ret = -EIO;
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND_COEFF:
		radio->low->fm_config.blend_coeffs_soft = (u16)ctrl->val;
		fm_set_blend_mute(radio);
		FDEBUG(radio, "%s(), BLEND_COEFF_SOFT val:%d,ret: %d\n",
				__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_SOFT_MUTE_COEFF:
		radio->low->fm_config.mute_coeffs_soft = (u16)ctrl->val;
		fm_set_blend_mute(radio);
		FDEBUG(radio, "%s(), SOFT_MUTE_COEFF val:%d, ret: %d\n",
				__func__, ctrl->val, ret);
		break;
	case V4L2_CID_S610_RSSI_CURR:
		ctrl->val = 0;
		break;
	case V4L2_CID_S610_SEEK_CANCEL:
		if (ctrl->val) {
			payload = FM_TUNER_STOP_SEARCH_MODE;
			low_set_tuner_mode(radio, payload);
			FDEBUG(radio, "%s(), SEEK_CANCEL val:%d, ret: %d\n",
					__func__, ctrl->val,  ret);
			radio->seek_mode = FM_TUNER_STOP_SEARCH_MODE;
		}
		break;
	case V4L2_CID_S610_SEEK_MODE:
		if (ctrl->val == 4)
			radio->seek_mode = FM_TUNER_AUTONOMOUS_SEARCH_MODE_NEXT;
		else
			radio->seek_mode = ctrl->val;

		FDEBUG(radio, "%s(), SEEK_MODE val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_RDS_ON:
		payload = (u16)ctrl->val;
		if (payload & RDS_PARSER_ENABLE) {
			radio->rds_parser_enable = TRUE;
		} else {
			radio->rds_parser_enable = FALSE;
		}
		payload &= FM_RDS_ENABLE;
		ret = fm_set_rds_mode(radio, payload);
		FDEBUG(radio, "%s(), RDS_RECEPTION:%d, ret:%d parser:%d\n", __func__,
				payload, ret, radio->rds_parser_enable);
		if (ret != 0)
			ret = -EINVAL;
		break;
	case V4L2_CID_S610_SNR_CURR:
		ctrl->val = 0;
		break;
	case V4L2_CID_S610_IF_COUNT1:
		radio->low->fm_config.search_conf.weak_ifca_m =
				(u16)ctrl->val;
		FDEBUG(radio, "%s(), IF_CNT1  val:%d, ret : %d\n",
			__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_IF_COUNT2:
		radio->low->fm_config.search_conf.normal_ifca_m =
				(u16)ctrl->val;
		FDEBUG(radio, "%s(), IF_CNT2  val:%d, ret : %d\n",
			__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_RSSI_TH:
		low_set_search_lvl(radio, (u16)ctrl->val);
		FDEBUG(radio, "%s(), RSSI_TH val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;
	case V4L2_CID_S610_KERNEL_VER:
		ctrl->val = 0;
		break;
	case V4L2_CID_S610_SOFT_STEREO_BLEND_REF:
		payload = (u16)ctrl->val;
		if (payload & RSSI_REF_ENABLE)
			radio->rssi_ref_enable = TRUE;
		else
			radio->rssi_ref_enable = FALSE;

		FDEBUG(radio, "%s(), SOFT_STEREO_BLEND_REF val:%d, ret : %d\n",
				__func__, ctrl->val,  ret);
		break;

	default:
		ret = -EINVAL;
		dev_err(radio->v4l2dev.dev, "s_ctrl Unknown IOCTL: 0x%x\n",
				(int)ctrl->id);
		break;
	}
	s610_core_unlock(radio->core);
	FUNC_EXIT(radio);

	return ret;
}

#ifdef	CONFIG_VIDEO_ADV_DEBUG
static int s610_radio_g_register(struct file *file, void *fh,
		struct v4l2_dbg_register *reg)
{
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	s610_core_lock(radio->core);

	reg->size = 4;
	reg->val = (__u64)fmspeedy_get_reg((u32)reg->reg);
	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return 0;
}
static int s610_radio_s_register(struct file *file, void *fh,
		const struct v4l2_dbg_register *reg)
{
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	s610_core_lock(radio->core);
	fmspeedy_set_reg((u32)reg->reg, (u32)reg->val);
	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return 0;
}
#endif

int s610_core_set_power_state(struct s610_radio *radio,
		u8 next_state)
{
	int ret = 0;

	FUNC_ENTRY(radio);

	ret = low_set_power(radio, next_state);
	if (ret != 0) {
		dev_err(radio->v4l2dev.dev,
			"Failed to write reg for power on\n");
		ret = -EIO;
		goto ret_power;
	}
	radio->core->power_mode |= next_state;

ret_power:
	FUNC_EXIT(radio);
	return ret;
}

void  fm_prepare(struct s610_radio *radio)
{
	FUNC_ENTRY(radio);

	spin_lock_init(&radio->rds_buff_lock);

	/* Initial FM device variables  */
	/* Region info */
	radio->region = region_configs[radio_region];
	radio->mute_mode = FM_MUTE_OFF;
	radio->rf_depend_mute = FM_RX_RF_DEPENDENT_MUTE_OFF;
	radio->rds_flag = FM_RDS_DISABLE;
	radio->freq = FM_UNDEFINED_FREQ;
	radio->rds_mode = FM_RDS_SYSTEM_RDS;
	radio->af_mode = FM_RX_RDS_AF_SWITCH_MODE_OFF;
	radio->core->power_mode = S610_POWER_DOWN;
	radio->seek_weak_rssi = SCAN_WEAK_SIG_THRESHOLD;
	/* Reset RDS buffer cache if need */

	/* Initial wait queue for rds read */
	init_waitqueue_head(&radio->core->rds_read_queue);
	radio->idle_cnt_mod = 0;
	radio->rds_new_stat = 0;

	FUNC_EXIT(radio);

}

#ifdef USE_FM_LNA_ENABLE
static int set_eLNA_gpio(struct s610_radio *radio, int stat)
{
	if (radio->without_elna)
		return -EPERM;

	if (!gpio_is_valid(radio->elna_gpio)) {
		return -EINVAL;
	} else {
		dev_info(radio->v4l2dev.dev, "%s(%d)\n", __func__, stat);
		gpio_set_value(radio->elna_gpio, stat);
	}
	dev_info(radio->v4l2dev.dev, "Get elna_gpio(%d)\n",
			gpio_get_value(radio->elna_gpio));

	return 0;
}
#endif /* USE_FM_LNA_ENABLE */

static int s610_radio_fops_open(struct file *file)
{
	int ret;
	struct s610_radio *radio = video_drvdata(file);
	static bool run_once = true;

	FUNC_ENTRY(radio);

	shared_fm_open_cnt++;

	ret = v4l2_fh_open(file);
	if (ret)
		return ret;

	if (v4l2_fh_is_singular_file(file)) {

#ifdef CONFIG_SCSC_FM
		/* Start FM/WLBT LDO */
		ret = mx250_fm_request();
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"mx250_fm_request() failed with err %d\n", ret);
			return ret;
		}
#endif /* CONFIG_SCSC_FM */

#ifdef USE_FM_LNA_ENABLE
		if (radio->elna_gpio != -EINVAL) {
			dev_info(radio->v4l2dev.dev,
				"(%s):FM eLNA gpio: %d\n",
				__func__, radio->elna_gpio);
			if (gpio_request_one(radio->elna_gpio,
						GPIOF_OUT_INIT_LOW,
						"LNA_GPIO_EN")) {
				dev_info(radio->v4l2dev.dev,
					"Failed to request gpio for FM eLNA\n");
			}
			if (set_eLNA_gpio(radio, GPIO_HIGH)) {
				dev_info(radio->v4l2dev.dev,
					"Failed to set gpio HIGH for FM eLNA\n");
			} else
				dev_info(radio->v4l2dev.dev,
					"Set gpio HIGH for FM eLNA\n");
		}
#endif /* USE_FM_LNA_ENABLE */

		s610_core_lock(radio->core);
		atomic_set(&radio->is_doing, 0);
		atomic_set(&radio->is_rds_doing, 0);

#ifdef USE_AUDIO_PM
		if (radio->a_dev) {
			ret = pm_runtime_get_sync(radio->a_dev);
			if (ret < 0) {
				dev_err(radio->v4l2dev.dev,
					"audio get_sync not work: not suspend %d\n", ret);
				goto err_open;
			}

			abox_request_cpu_gear_sync(radio->a_dev,
				dev_get_drvdata(radio->a_dev), ABOX_CPU_GEAR_FM, ABOX_CPU_GEAR_MAX);
		}
#endif /* USE_AUDIO_PM */

		ret = pm_runtime_get_sync(radio->dev);
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"get_sync failed with err %d\n", ret);
			goto err_open;
		}

		fmspeedy_wakeup();

		if (run_once) {
			fm_ds_set(0);
			fm_aux_pll_off();

			run_once = false;
		}

		fm_get_version_number();

		/* Initail fm low structure */
		ret = init_low_struc(radio);
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"Failed to init reg for initial struc\n");
			goto err_open;
		}

		/* Booting fm */
		ret = fm_boot(radio);
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"Failed to set reg for FM boot\n");
			goto err_open;
		}

		fm_prepare(radio);

		ret = s610_core_set_power_state(radio,
				S610_POWER_ON_FM);
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"Failed to write reg for power on\n");
			goto err_open;
		}

		ret = s610_radio_pretune(radio);
		if (ret < 0) {
			dev_err(radio->v4l2dev.dev,
				"Failed to write reg for preturn\n");
			goto power_down;
		}

		radio->core->power_mode = S610_POWER_ON_FM;

		mdelay(100);

#ifndef RDS_POLLING_ENABLE
		/* Speedy master interrupt enable */
		fm_speedy_m_int_enable();
#else
		fm_speedy_m_int_disable();
#endif /*RDS_POLLING_ENABLE*/
		s610_core_unlock(radio->core);
		/*Must be done after s610_core_unlock to prevent a deadlock*/
		v4l2_ctrl_handler_setup(&radio->ctrl_handler);
	}

	FUNC_EXIT(radio);

	return ret;

power_down:
	s610_core_set_power_state(radio,
			S610_POWER_DOWN);

err_open:
	s610_core_unlock(radio->core);
	v4l2_fh_release(file);

	FUNC_EXIT(radio);

	return ret;

}

static int s610_radio_fops_release(struct file *file)
{
	int ret = 0;
	struct s610_radio *radio = video_drvdata(file);

	FUNC_ENTRY(radio);

	if (v4l2_fh_is_singular_file(file)) {
		s610_core_lock(radio->core);
		s610_core_set_power_state(radio, S610_POWER_DOWN);

		/* Speedy master interrupt disable */
		fm_speedy_m_int_disable();

		/* FM demod power off */
		fm_power_off();

		cancel_delayed_work_sync(&radio->dwork_sig2);
		cancel_delayed_work_sync(&radio->dwork_tune);

#ifdef	ENABLE_RDS_WORK_QUEUE
		cancel_work_sync(&radio->work);
#endif	/*ENABLE_RDS_WORK_QUEUE*/
#ifdef	IDLE_POLLING_ENABLE
		fm_idle_periodic_cancel((unsigned long) radio);
#endif	/*IDLE_POLLING_ENABLE*/
#ifdef	RDS_POLLING_ENABLE
		fm_rds_periodic_cancel((unsigned long) radio);
#endif	/*RDS_POLLING_ENABLE*/
/*#ifdef	ENABLE_IF_WORK_QUEUE
		cancel_work_sync(&radio->if_work);
#endif*/	/*ENABLE_IF_WORK_QUEUE*/

		pm_runtime_put_sync(radio->dev);
#ifdef USE_AUDIO_PM
		if (radio->a_dev) {
			abox_request_cpu_gear_sync(radio->a_dev,
				dev_get_drvdata(radio->a_dev), ABOX_CPU_GEAR_FM, ABOX_CPU_GEAR_MIN);

			pm_runtime_put_sync(radio->a_dev);
		}
#endif /* USE_AUDIO_PM */
		s610_core_unlock(radio->core);

#ifdef USE_FM_LNA_ENABLE
		if (radio->elna_gpio != -EINVAL) {
			if (set_eLNA_gpio(radio, GPIO_LOW))
				dev_info(radio->v4l2dev.dev,
					"Failed to set gpio LOW for FM eLNA\n");
			else
				dev_info(radio->v4l2dev.dev,
					"Set gpio LOW for FM eLNA\n");
			gpio_free(radio->elna_gpio);
		}
#endif /* USE_FM_LNA_ENABLE */

#ifdef CONFIG_SCSC_FM
		/* Stop FM/WLBT LDO */
		ret = mx250_fm_release();
		if (ret < 0)
			dev_err(radio->v4l2dev.dev,
				"mx250_fm_release() failed with err %d\n", ret);
#endif
	}

	if (wake_lock_active(&radio->wakelock))
		wake_unlock(&radio->wakelock);

	if (wake_lock_active(&radio->rdswakelock))
		wake_unlock(&radio->rdswakelock);

	ret = v4l2_fh_release(file);

	shared_fm_open_cnt--;

	return ret;
}

static ssize_t s610_radio_fops_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	int ret;
	size_t rsize, blocks;
	struct s610_radio *radio = video_drvdata(file);
	size_t rds_max_thresh;

	FUNC_ENTRY(radio);

	ret = wait_event_interruptible(radio->core->rds_read_queue,
			atomic_read(&radio->is_rds_new));
	if (ret < 0)
		return -EINTR;

	if (!atomic_read(&radio->is_rds_new)) {
		radio->rds_new_stat++;
		return -ERESTARTSYS;
	}

	if (s610_core_lock_interruptible(radio->core))
		return -ERESTARTSYS;

	if (radio->rds_parser_enable)
#ifdef USE_RINGBUFF_API
		rds_max_thresh = ringbuf_bytes_used(&radio->rds_rb);
#else
		rds_max_thresh = RDS_MEM_MAX_THRESH_PARSER;
#endif /* USE_RINGBUFF_API */
	else
	rds_max_thresh = RDS_MEM_MAX_THRESH;

	/* Turn on RDS mode */
	memset(radio->rds_buf, 0, 480);

	rsize = rds_max_thresh;
	rsize = min(rsize, count);

	blocks = rsize/FM_RDS_BLK_SIZE;

	ret = fm_read_rds_data(radio, radio->rds_buf,
		(int)rsize, (u16 *)&blocks);
	if (ret == 0) {
		dev_err(radio->v4l2dev.dev, ">>> Failed to read rds mode\n");
		goto read_unlock;
	}

	/* always transfer rds complete blocks */
	if (copy_to_user(buf, &radio->rds_buf, rsize))
		ret = -EFAULT;

	if (radio->rds_parser_enable) {
		RDSEBUG(radio, "RDS RD done:%08d:%08d fifo err:%08d type(%02X) len(%02X)",
			radio->rds_read_cnt, radio->rds_n_count, radio->rds_fifo_err_cnt,
			radio->rds_buf[0], radio->rds_buf[1]);
	} else {
	if ((radio->rds_buf[2]&0x07) != RDS_BLKTYPE_A) {
		radio->rds_reset_cnt++;
		if (radio->rds_reset_cnt > radio->low->fm_state.rds_unsync_blk_cnt) {
			fm_set_rds_mode(radio, FM_RDS_DISABLE);
			mdelay(10);
			fm_set_rds_mode(radio, FM_RDS_ENABLE);
			radio->rds_reset_cnt = 0;
			RDSEBUG(radio, "RDS reset! cause of block type invalid");
		}
		RDSEBUG(radio, "RDS block type invalid! %02d, %08d",
			(radio->rds_buf[2]&0x07), radio->rds_reset_cnt);
	}

	RDSEBUG(radio, "RDS RD done:%08d:%08d fifo err:%08d block type0:%02X,%02X",
		radio->rds_read_cnt, radio->rds_n_count, radio->rds_fifo_err_cnt,
		(radio->rds_buf[2]&0x11)>>3, radio->rds_buf[2]&0x07);
	}

	radio->rds_read_cnt++;
	ret = rsize;
read_unlock:
	atomic_set(&radio->is_rds_new, 0);
	atomic_set(&gradio->is_doing, 0);
	atomic_set(&gradio->is_rds_doing, 0);

	s610_core_unlock(radio->core);

	FUNC_EXIT(radio);

	return ret;
}

static unsigned int s610_radio_fops_poll(struct file *file,
		struct poll_table_struct *pts)
{
	struct s610_radio *radio = video_drvdata(file);
	unsigned long req_events = poll_requested_events(pts);
	unsigned int ret = v4l2_ctrl_poll(file, pts);

	FUNC_ENTRY(radio);

	if (req_events & (POLLIN | POLLRDNORM)) {
		poll_wait(file, &radio->core->rds_read_queue, pts);

		if (atomic_read(&radio->is_rds_new))
			ret = POLLIN | POLLRDNORM;
	}

	FDEBUG(radio, "POLL RET: 0x%x pwmode: %d freq: %d",
		ret,
		radio->core->power_mode,
		radio->freq);
	FUNC_EXIT(radio);

	return ret;
}

static const struct v4l2_file_operations s610_fops = {
		.owner			= THIS_MODULE,
		.read			= s610_radio_fops_read,
		.poll			= s610_radio_fops_poll,
		.unlocked_ioctl		= video_ioctl2,
		.open			= s610_radio_fops_open,
		.release		= s610_radio_fops_release,
};

static const struct v4l2_ioctl_ops S610_ioctl_ops = {
		.vidioc_querycap		= s610_radio_querycap,
		.vidioc_g_tuner			= s610_radio_g_tuner,
		.vidioc_s_tuner			= s610_radio_s_tuner,

		.vidioc_g_frequency		= s610_radio_g_frequency,
		.vidioc_s_frequency		= s610_radio_s_frequency,
		.vidioc_s_hw_freq_seek		= s610_radio_s_hw_freq_seek,
		.vidioc_enum_freq_bands		= s610_radio_enum_freq_bands,

		.vidioc_subscribe_event		= v4l2_ctrl_subscribe_event,
		.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,

#ifdef CONFIG_VIDEO_ADV_DEBUG
		.vidioc_g_register		= s610_radio_g_register,
		.vidioc_s_register		= s610_radio_s_register,
#endif
};

static const struct video_device s610_viddev_template = {
		.fops			= &s610_fops,
		.name			= DRIVER_NAME,
		.release		= video_device_release_empty,
};

static int s610_radio_add_new_custom(struct s610_radio *radio,
		enum s610_ctrl_idx idx, unsigned long flag)
{
	int ret;
	struct v4l2_ctrl *ctrl;

	ctrl = v4l2_ctrl_new_custom(&radio->ctrl_handler,
			&s610_ctrls[idx],
			NULL);

	ret = radio->ctrl_handler.error;
	if (ctrl == NULL && ret) {
		dev_err(radio->v4l2dev.dev,
				"Could not initialize '%s' control %d\n",
				s610_ctrls[idx].name, ret);
		return -EINTR;
	}
	if (flag && (ctrl != NULL))
		ctrl->flags |= flag;

	return ret;
}

#ifndef RDS_POLLING_ENABLE
static irqreturn_t s610_hw_irq_handle(int irq, void *devid)
{
	struct s610_radio *radio = devid;
	u32 int_stat;

	spin_lock(&radio->slock);

	int_stat = __raw_readl(radio->fmspeedy_base+FMSPDY_STAT);

	if (int_stat & FM_SLV_INT)
		fm_isr(radio);

	spin_unlock(&radio->slock);

	return IRQ_HANDLED;
}
#endif /* RDS_POLLING_ENABLE */

MODULE_ALIAS("platform:s610-radio");
static const struct of_device_id exynos_fm_of_match[] = {
		{
				.compatible = "samsung,exynos9610-fm",
		},
		{},
};
MODULE_DEVICE_TABLE(of, exynos_fm_of_match);

signed int exynos_get_fm_open_status(void)
{
	pr_info("%s(): shared fm open count: %d", __func__, shared_fm_open_cnt);
	return (shared_fm_open_cnt);
}

EXPORT_SYMBOL(exynos_get_fm_open_status);

/* bit 28:
  Global hardware automatic clock gating enable on CMU module : CMU
*/
#define ENABLE_AUTOMATIC_CLKGATING	28
void disable_clk_gating(struct s610_radio *radio)
{
	void __iomem *cmu_disaud_800;
	u32 cmu_disaud_800_val;

	FUNC_ENTRY(radio);

	cmu_disaud_800 = radio->disaud_cmu_base+0x800;
	
	/* Read register */
	cmu_disaud_800_val = read32(cmu_disaud_800);
	FDEBUG(radio,
		"cmu_disaud_800: 0x%08X,\n",
		cmu_disaud_800_val);
	
	if (GetBits(cmu_disaud_800, ENABLE_AUTOMATIC_CLKGATING, 1))
		SetBits(cmu_disaud_800, ENABLE_AUTOMATIC_CLKGATING, 1, 0);

	/* Read register */
	cmu_disaud_800_val = read32(cmu_disaud_800);
	FDEBUG(radio,
		"cmu_disaud_800: 0x%08X,\n",
		cmu_disaud_800_val);

	FUNC_EXIT(radio);
}

void enable_FM_mux_clk_aud(struct s610_radio *radio)
{
	void __iomem *cmu_disaud_100c;
	u32 cmu_disaud_100c_val;

	FUNC_ENTRY(radio);

	cmu_disaud_100c = radio->disaud_cmu_base+0x100C;

	/* Read register */
	cmu_disaud_100c_val = read32(cmu_disaud_100c);
	FDEBUG(radio,
		"befor: cmu_disaud_100c: 0x%08X,\n",
		cmu_disaud_100c_val);

	if (GetBits(cmu_disaud_100c, ENABLE_AUTOMATIC_CLKGATING, 1))
		SetBits(cmu_disaud_100c, ENABLE_AUTOMATIC_CLKGATING, 1, 0);

	/* Read register */
	cmu_disaud_100c_val = read32(cmu_disaud_100c);
	FDEBUG(radio,
		"after: cmu_disaud_100c: 0x%08X,\n",
		cmu_disaud_100c_val);

	FUNC_EXIT(radio);
}

#ifdef USE_AUDIO_PM
static struct device_node *exynos_audio_parse_dt(struct s610_radio *radio)
{
	struct platform_device *pdev = NULL;
	struct device_node *np = NULL;

	np = of_find_compatible_node(NULL, NULL, "samsung,abox");
	if (!np) {
		dev_err(radio->dev, "abox device is not available\n");
		return NULL;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		dev_err(radio->dev, "%s: failed to get audio platform_device\n", __func__);
		return NULL;
	}
	radio->a_dev = &pdev->dev;

	return np;
}
#endif /* USE_AUDIO_PM */

static int s610_radio_probe(struct platform_device *pdev)
{
	unsigned long ret;
	struct s610_radio *radio;
	struct v4l2_ctrl *ctrl;
	const struct of_device_id *match;
	struct resource *resource;
	struct device *dev = &pdev->dev;
	static atomic_t instance = ATOMIC_INIT(0);
	struct device_node *dnode;

	dnode = dev->of_node;

	dev_info(&pdev->dev, ">>> start FM Radio probe\n");

	radio = devm_kzalloc(&pdev->dev, sizeof(*radio), GFP_KERNEL);
	if (!radio)
		return -ENOMEM;

	radio->core = devm_kzalloc(&pdev->dev, sizeof(struct s610_core),
			GFP_KERNEL);
	if (!radio->core) {
		ret =  -ENOMEM;
		goto alloc_err0;
	}

	radio->low = devm_kzalloc(&pdev->dev, sizeof(struct s610_low),
			GFP_KERNEL);
	if (!radio->low) {
		ret =  -ENOMEM;
		goto alloc_err1;
	}

	radio->dev = dev;
	radio->pdev = pdev;
	gradio = radio;

	ret = fm_clk_get(radio);
	if (ret)
		goto alloc_err2;

	fm_clk_prepare(radio);
	if (ret)
		goto alloc_err2;

	fm_clk_enable(radio);
	if (ret) {
		fm_clk_unprepare(radio);
		goto alloc_err2;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	spin_lock_init(&radio->slock);
	spin_lock_init(&radio->rds_lock);

	/* Init flags FR BL init_completion */
	init_completion(&radio->flags_set_fr_comp);
	init_completion(&radio->flags_seek_fr_comp);

	v4l2_device_set_name(&radio->v4l2dev, DRIVER_NAME, &instance);

	ret = v4l2_device_register(&pdev->dev, &radio->v4l2dev);
	if (ret) {
		dev_err(&pdev->dev, "Cannot register v4l2_device.\n");
		goto alloc_err3;
	}

	match = of_match_node(exynos_fm_of_match, dev->of_node);
	if (!match) {
		dev_err(&pdev->dev, "failed to match node\n");
		ret =  -EINVAL;
		goto alloc_err4;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	radio->fmspeedy_base = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(radio->fmspeedy_base)) {
		dev_err(&pdev->dev,
			"Failed to request memory region\n");
		ret = -EBUSY;
		goto alloc_err4;
	}

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	radio->disaud_cmu_base = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(radio->disaud_cmu_base)) {
		dev_err(&pdev->dev,
			"Failed to request memory region\n");
		ret = -EBUSY;
		goto alloc_err4;
	}

#ifndef RDS_POLLING_ENABLE
	/*save to global variavle  fm speedy physical address */
	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!resource) {
		dev_err(&pdev->dev, "failed to get IRQ resource\n");
		ret = -ENOENT;
		goto alloc_err4;
	}

	ret = devm_request_irq(&pdev->dev,
			resource->start,
			s610_hw_irq_handle,
			0,
			pdev->name,
			radio);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		goto alloc_err4;
	}
	radio->irq = resource->start;
#endif /* RDS_POLLING_ENABLE */

#ifdef USE_FM_LNA_ENABLE
	radio->elna_gpio = of_get_named_gpio(dnode, "elna_gpio", 0);
	if (!gpio_is_valid(radio->elna_gpio)) {
		dev_err(dev, "(%s) elna_gpio invalid. Disable elna_gpio control\n",
				__func__);
		radio->elna_gpio = -EINVAL;
	} else {
		ret = gpio_request_one(radio->elna_gpio, GPIOF_OUT_INIT_LOW,
					"LNA_GPIO_EN");
		if (ret)
			gpio_set_value(radio->elna_gpio, GPIO_LOW);
		dev_info(dev, "(%s) Enable elna_gpio control :%d\n",
			__func__, gpio_get_value(radio->elna_gpio));
	}
#endif /* USE_FM_LNA_ENABLE */

#ifdef USE_AUDIO_PM
	if (!exynos_audio_parse_dt(radio)) {
		ret = -EINVAL;
		goto alloc_err4;
	}
#endif /* USE_AUDIO_PM */

	memcpy(&radio->videodev, &s610_viddev_template,
			sizeof(struct video_device));

	radio->videodev.v4l2_dev  = &radio->v4l2dev;
	radio->videodev.ioctl_ops = &S610_ioctl_ops;

	video_set_drvdata(&radio->videodev, radio);
	platform_set_drvdata(pdev, radio);

	radio->v4l2dev.ctrl_handler = &radio->ctrl_handler;

	v4l2_ctrl_handler_init(&radio->ctrl_handler,
			1 + ARRAY_SIZE(s610_ctrls));
	/* Set control */
	ret = s610_radio_add_new_custom(radio, S610_IDX_CH_SPACING,
			0); /*0x01*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_CH_BAND,
			0); /*0x02*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SOFT_STEREO_BLEND,
			0); /*0x03*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SOFT_STEREO_BLEND_COEFF,
			0); /*0x04*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SOFT_MUTE_COEFF,
			0); /*0x05*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_RSSI_CURR,
			V4L2_CTRL_FLAG_VOLATILE); /*0x06*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SNR_CURR,
			V4L2_CTRL_FLAG_VOLATILE); /*0x07*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SEEK_CANCEL,
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE); /*0x08*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SEEK_MODE,
			0); /*0x09*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_RDS_ON,
			0); /*0x0A*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_IF_COUNT1,
			0); /*0x0B*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_IF_COUNT2,
			0); /*0x0C*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_RSSI_TH,
			0); /*0x0D*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_KERNEL_VER,
			V4L2_CTRL_FLAG_VOLATILE); /*0x0E*/
	if (ret < 0)
		goto exit;
	ret = s610_radio_add_new_custom(radio, S610_IDX_SOFT_STEREO_BLEND_REF,
			V4L2_CTRL_FLAG_EXECUTE_ON_WRITE|V4L2_CTRL_FLAG_VOLATILE); /*0x0F*/
	if (ret < 0)
		goto exit;

	ctrl = v4l2_ctrl_new_std(&radio->ctrl_handler, &s610_ctrl_ops,
			V4L2_CID_AUDIO_MUTE, 0, 4, 1, 1);
	ret = radio->ctrl_handler.error;
	if (ctrl == NULL && ret) {
		dev_err(&pdev->dev,
			"Could not initialize V4L2_CID_AUDIO_MUTE control %d\n",
			(int)ret);
		goto exit;
	}

	ctrl = v4l2_ctrl_new_std(&radio->ctrl_handler, &s610_ctrl_ops,
			V4L2_CID_AUDIO_VOLUME, 0, 15, 1, 1);
	ret = radio->ctrl_handler.error;
	if (ctrl == NULL && ret) {
		dev_err(&pdev->dev,
			"Could not initialize V4L2_CID_AUDIO_VOLUME control %d\n",
			(int)ret);
		goto exit;
	}

	ctrl = v4l2_ctrl_new_std_menu(&radio->ctrl_handler, &s610_ctrl_ops,
			V4L2_CID_TUNE_DEEMPHASIS,
			V4L2_DEEMPHASIS_75_uS, 0, V4L2_PREEMPHASIS_75_uS);
	ret = radio->ctrl_handler.error;
	if (ctrl == NULL && ret) {
		dev_err(&pdev->dev,
			"Could not initialize V4L2_CID_TUNE_DEEMPHASIS control %d\n",
			(int)ret);
		goto exit;
	}

	/* register video device */
	ret = video_register_device(&radio->videodev, VFL_TYPE_RADIO, -1);
	if (ret < 0) {
		dev_err(&pdev->dev, "Could not register video device\n");
		goto exit;
	}

	mutex_init(&radio->lock);
	s610_core_lock_init(radio->core);

	/*init_waitqueue_head(&radio->core->rds_read_queue);*/

	INIT_DELAYED_WORK(&radio->dwork_sig2, s610_sig2_work);
	INIT_DELAYED_WORK(&radio->dwork_tune, s610_tune_work);
#ifdef	RDS_POLLING_ENABLE
	INIT_DELAYED_WORK(&radio->dwork_rds_poll, s610_rds_poll_work);
#endif	/*RDS_POLLING_ENABLE*/
#ifdef	IDLE_POLLING_ENABLE
	INIT_DELAYED_WORK(&radio->dwork_idle_poll, s610_idle_poll_work);
#endif	/*IDLE_POLLING_ENABLE*/

#ifdef	ENABLE_RDS_WORK_QUEUE
	INIT_WORK(&radio->work, s610_rds_work);
#endif	/*ENABLE_RDS_WORK_QUEUE*/

#ifndef	RDS_POLLING_ENABLE
#ifdef	ENABLE_IF_WORK_QUEUE
	INIT_WORK(&radio->if_work, s610_if_work);
#endif	/*ENABLE_IF_WORK_QUEUE*/
#endif	/* RDS_POLLING_ENABLE */

	/* all aux pll off for WIFI/BT */
	radio->rfchip_ver = S620_REV_0;

	ret = of_property_read_u32(dnode, "fm_iclk_aux", &radio->iclkaux);
	if (ret)
		radio->iclkaux = 1;
	dev_info(radio->dev, "iClk Aux: %d\n", radio->iclkaux);

	ret = of_property_read_u32(dnode, "num-tcon-freq", &radio->tc_on);
	if (ret) {
		radio->tc_on = 0;
		goto skip_tc_off;
	}

	fm_spur_init = devm_kzalloc(&pdev->dev, radio->tc_on * sizeof(*fm_spur_init),
					GFP_KERNEL);
	if (!fm_spur_init) {
		dev_err(radio->dev, "Mem alloc failed for TC ON freq values, TC off\n");
		radio->tc_on = 0;
		goto skip_tc_off;
	}

	if (of_property_read_u32_array(dnode, "val-tcon-freq", fm_spur_init, radio->tc_on)) {
		dev_err(radio->dev, "Getting val-tcon-freq values faild, TC off\n");
		radio->tc_on = 0;
		goto skip_tc_off;
	}
	dev_info(radio->dev, "number TC On Freq: %d\n", radio->tc_on);
skip_tc_off:

	ret = of_property_read_u32(dnode, "num-trfon-freq", &radio->trf_on);
	if (ret) {
		radio->trf_on = 0;
		goto skip_trf_off;
	}

	fm_spur_trf_init = devm_kzalloc(&pdev->dev, radio->trf_on * sizeof(*fm_spur_trf_init),
					GFP_KERNEL);
	if (!fm_spur_trf_init) {
		dev_err(radio->dev, "Mem alloc failed for TRF ON freq values, TRF off\n");
		radio->trf_on = 0;
		goto skip_trf_off;
	}

	if (of_property_read_u32_array(dnode, "val-trfon-freq", fm_spur_trf_init, radio->trf_on)) {
		dev_err(radio->dev, "Getting val-trfon-freq values faild, TRF off\n");
		radio->trf_on = 0;
		goto skip_trf_off;
	}
	dev_info(radio->dev, "number TRF On Freq: %d\n", radio->trf_on);
skip_trf_off:

	ret = of_property_read_u32(dnode, "num-dual-clkon-freq", &radio->dual_clk_on);
	if (ret) {
		radio->dual_clk_on = 0;
		goto skip_dual_clk_off;
	}

	fm_dual_clk_init = devm_kzalloc(&pdev->dev, radio->dual_clk_on * sizeof(*fm_dual_clk_init),
					GFP_KERNEL);
	if (!fm_dual_clk_init) {
		dev_err(radio->dev, "Mem alloc failed for DUAL CLK ON freq values, DUAL CLK off\n");
		radio->dual_clk_on = 0;
		goto skip_dual_clk_off;
	}

	if (of_property_read_u32_array(dnode, "val-dual-clkon-freq", fm_dual_clk_init, radio->dual_clk_on)) {
		dev_err(radio->dev, "Getting val-dual-clkon-freq values faild, DUAL CLK off\n");
		radio->dual_clk_on = 0;
		goto skip_dual_clk_off;
	}
	dev_info(radio->dev, "number DUAL CLK On Freq: %d\n", radio->dual_clk_on);
skip_dual_clk_off:

	radio->vol_num = FM_RX_VOLUME_GAIN_STEP;
	radio->vol_level_mod = vol_level_init;

	ret = of_property_read_u32(dnode, "num-volume-level", &radio->vol_num);
	if (ret) {
		goto skip_vol_sel;
	}

	radio->vol_level_tmp = devm_kzalloc(&pdev->dev, radio->vol_num * sizeof(u32),
					GFP_KERNEL);
	if (!radio->vol_level_tmp) {
		dev_err(radio->dev, "Mem alloc failed for Volume level values, Volume Level default setting\n");
		goto skip_vol_sel;
	}

	if (of_property_read_u32_array(dnode, "val-vol-level", radio->vol_level_tmp, radio->vol_num)) {
		dev_err(radio->dev, "Getting val-vol-level values faild, Volume Level default stting\n");
		kfree(radio->vol_level_tmp);
		radio->vol_num = FM_RX_VOLUME_GAIN_STEP;
		goto skip_vol_sel;
	}
	radio->vol_level_mod = radio->vol_level_tmp;

skip_vol_sel:
	dev_info(radio->dev, "volume select num: %d\n", radio->vol_num);

	ret = of_property_read_u32(dnode, "vol_3db_on", &radio->vol_3db_att);
	if (ret)
		radio->vol_3db_att = 0;
	dev_info(radio->dev, "volume -3dB: %d\n", radio->vol_3db_att);

	ret = of_property_read_u32(dnode, "rssi_est_on", &radio->rssi_est_on);
	if (ret)
		radio->rssi_est_on = 0;
	dev_info(radio->dev, "rssi_est_on: %d\n", radio->rssi_est_on);

	ret = of_property_read_u32(dnode, "sw_mute_weak", &radio->sw_mute_weak);
	if (ret)
		radio->sw_mute_weak = 0;
	dev_info(radio->dev, "sw_mute_weak: %d\n", radio->sw_mute_weak);

	ret = of_property_read_u32(dnode, "without_elna", &radio->without_elna);
	if (ret)
		radio->without_elna = 0;
	dev_info(radio->dev, "without eLNA: %d\n", radio->without_elna);

	ret = of_property_read_u16(dnode, "rssi_adjust", &radio->rssi_adjust);
	if (ret)
		radio->rssi_adjust= 0;
	dev_info(radio->dev, "rssi adjust: %d\n", radio->rssi_adjust);

	wake_lock_init(&radio->wakelock, WAKE_LOCK_SUSPEND, "fm_wake");
	wake_lock_init(&radio->rdswakelock, WAKE_LOCK_SUSPEND, "fm_rdswake");

	dev_info(&pdev->dev, "end FM probe.\n");
	return 0;

exit:
	v4l2_ctrl_handler_free(radio->videodev.ctrl_handler);

alloc_err4:
	v4l2_device_unregister(&radio->v4l2dev);

alloc_err3:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);

	fm_clk_disable(radio);
	fm_clk_unprepare(radio);

alloc_err2:
	kfree(radio->low);

alloc_err1:
	kfree(radio->core);

alloc_err0:
	kfree(radio);

	return ret;
}

static int s610_radio_remove(struct platform_device *pdev)
{
	struct s610_radio *radio = platform_get_drvdata(pdev);

	if (radio) {
		fm_clk_disable(radio);
		fm_clk_unprepare(radio);
		fm_clk_put(radio);

		pm_runtime_disable(&pdev->dev);
		pm_runtime_set_suspended(&pdev->dev);

		v4l2_ctrl_handler_free(radio->videodev.ctrl_handler);
		video_unregister_device(&radio->videodev);
		v4l2_device_unregister(&radio->v4l2dev);
		wake_lock_destroy(&radio->wakelock);
		wake_lock_destroy(&radio->rdswakelock);

		kfree(radio->vol_level_tmp);
		kfree(radio->low);
		kfree(radio->core);
		kfree(radio);
	}

	return 0;
}

static int fm_clk_get(struct s610_radio *radio)
{
	struct device *dev = radio->dev;
	struct clk *clk;
	int clk_count;
	int ret, i;

	clk_count = of_property_count_strings(dev->of_node, "clock-names");
	if (IS_ERR_VALUE((unsigned long)clk_count)) {
		dev_err(dev, "invalid clk list in %s node", dev->of_node->name);
		return -EINVAL;
	}

	radio->clk_ids = (const char **)devm_kmalloc(dev,
				(clk_count + 1) * sizeof(const char *),
				GFP_KERNEL);
	if (!radio->clk_ids) {
		dev_err(dev, "failed to alloc for clock ids");
		return -ENOMEM;
	}

	for (i = 0; i < clk_count; i++) {
		ret = of_property_read_string_index(dev->of_node, "clock-names",
								i, &radio->clk_ids[i]);
		if (ret) {
			dev_err(dev, "failed to read clocks name %d from %s node\n",
					i, dev->of_node->name);
			return ret;
		}
	}
	radio->clk_ids[clk_count] = NULL;

	radio->clocks = (struct clk **) devm_kmalloc(dev,
			clk_count * sizeof(struct clk *), GFP_KERNEL);
	if (!radio->clocks) {
		dev_err(dev, "%s: couldn't alloc\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; radio->clk_ids[i] != NULL; i++) {
		clk = devm_clk_get(dev, radio->clk_ids[i]);
		if (IS_ERR_OR_NULL(clk))
			goto err;

		radio->clocks[i] = clk;
	}
	radio->clocks[i] = NULL;

	return 0;

err:
	dev_err(dev, "couldn't get %s clock\n", radio->clk_ids[i]);
	return -EINVAL;
}

static int fm_clk_prepare(struct s610_radio *radio)
{
	int i;
	int ret;

	for (i = 0; radio->clocks[i] != NULL; i++) {
		ret = clk_prepare(radio->clocks[i]);
		if (ret)
			goto err;
	}

	return 0;

err:
	dev_err(radio->dev, "couldn't prepare clock[%d]\n", i);

	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_unprepare(radio->clocks[i]);

	return ret;
}

static int fm_clk_enable(struct s610_radio *radio)
{
	int i;
	int ret;

	for (i = 0; radio->clocks[i] != NULL; i++) {
		ret = clk_enable(radio->clocks[i]);
		if (ret)
			goto err;

		dev_info(radio->dev, "clock %s: %lu\n", radio->clk_ids[i], clk_get_rate(radio->clocks[i]));

		if (!strcmp(radio->clk_ids[i], "clk_aud_fm")) {
			if (clk_get_rate(radio->clocks[i]) != 40000) {
				ret = clk_set_rate(radio->clocks[i], 40000);
				if (IS_ERR_VALUE((unsigned long)ret))
					dev_info(radio->dev,
					"setting clock clk_aud_fm to 40KHz is failed: %lu\n", ret);
				dev_info(radio->dev, "FM clock clk_aud_fm: %lu\n",
					clk_get_rate(radio->clocks[i]));
			}
		}
	}

	return 0;

err:
	dev_err(radio->dev, "couldn't enable clock[%d]\n", i);

	/* roll back */
	for (i = i - 1; i >= 0; i--)
		clk_disable(radio->clocks[i]);

	return ret;
}

static void fm_clk_unprepare(struct s610_radio *radio)
{
	int i;

	for (i = 0; radio->clocks[i] != NULL; i++)
		clk_unprepare(radio->clocks[i]);
}

static void fm_clk_disable(struct s610_radio *radio)
{
	int i;

	for (i = 0; radio->clocks[i] != NULL; i++)
		clk_disable(radio->clocks[i]);
}

static void fm_clk_put(struct s610_radio *radio)
{
	int i;

	for (i = 0; radio->clocks[i] != NULL; i++)
		clk_put(radio->clocks[i]);

}

#ifdef CONFIG_PM
static int fm_radio_runtime_suspend(struct device *dev)
{
	struct s610_radio *radio = dev_get_drvdata(dev);

	FUNC_ENTRY(radio);

	fm_clk_disable(radio);

	return 0;
}

static int fm_radio_runtime_resume(struct device *dev)
{
	struct s610_radio *radio = dev_get_drvdata(dev);

	FUNC_ENTRY(radio);

	fm_clk_enable(radio);

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int fm_radio_suspend(struct device *dev)
{
//	struct s610_radio *radio = dev_get_drvdata(dev);

//	FUNC_ENTRY(radio);

	return 0;
}

static int fm_radio_resume(struct device *dev)
{
//	struct s610_radio *radio = dev_get_drvdata(dev);

//	FUNC_ENTRY(radio);

	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops fm_radio_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(fm_radio_suspend, fm_radio_resume)
	SET_RUNTIME_PM_OPS(fm_radio_runtime_suspend,
			fm_radio_runtime_resume, NULL)
};

#define DEV_PM_OPS	(&fm_radio_dev_pm_ops)

static struct platform_driver s610_radio_driver = {
		.driver		= {
				.name	= DRIVER_NAME,
				.owner	= THIS_MODULE,
				.of_match_table = exynos_fm_of_match,
				.pm	= DEV_PM_OPS,
		},
		.probe		= s610_radio_probe,
		.remove		= s610_radio_remove,
};
static int __init init_s610_radio(void)
{
	platform_driver_register(&s610_radio_driver);

	return 0;
}

static void __exit exit_s610_radio(void)
{
	platform_driver_unregister(&s610_radio_driver);
}

late_initcall(init_s610_radio);
module_exit(exit_s610_radio);
MODULE_AUTHOR("Youngjoon Chung, <young11@samsung.com>");
MODULE_DESCRIPTION("Driver for S610 FM Radio in Exynos9610");
MODULE_LICENSE("GPL");
