/*
 *  sound/soc/codecs/rt5510.c
 *  Driver to Richtek RT5510 SPKAMP IC
 *
 *  Copyright (C) 2018 Richtek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "rt5510.h"

struct codec_reg_val {
	u32 addr;
	u32 mask;
	u32 data;
};

static const struct codec_reg_val e4_reg_inits[] = {
	{ RT5510_REG_WDT_CTRL, 0x80, 0x00 },
	{ RT5510_REG_SPS_CTRL, 0x01, 0x00 },
	{ RT5510_REG_AUDIO_IN2_SEL, 0x1c, 0x04 },
	{ RT5510_REG_RESV11, 0x0c, 0x00 },
	{ RT5510_REG_RESV31, 0x03, 0x03 },
	{ RT5510_REG_RESV40, 0x01, 0x00 },
	{ RT5510_REG_RESV0, 0x44, 0x04 },
	{ RT5510_REG_RESV19, 0xff, 0x82 },
	{ RT5510_REG_RESV17, 0x7777, 0x7273 },
	{ RT5510_REG_RESV16, 0x07, 0x03 },
	{ RT5510_REG_DRE_CORASE, 0xe0, 0x20 },
	{ RT5510_REG_ADDA_CLOCK, 0xff, 0x70 },
	{ RT5510_REG_RESV21, 0xff, 0x20 },
	{ RT5510_REG_DRE_THDMODE, 0xff, 0x40 },
	{ RT5510_REG_RESV23, 0xffff, 0x17f8 },
	{ RT5510_REG_PWM_CTRL, 0xff, 0x15 },
	{ RT5510_REG_ADC_USB_MODE, 0xff, 0x00 },
	{ RT5510_REG_PROTECTION_CFG, 0xff, 0x1d },
	{ RT5510_REG_HPF1_COEF, 0xffffffff, 0x7fdb7ffe },
	{ RT5510_REG_HPF2_COEF, 0xffffffff, 0x7fdb7ffe },
	{ RT5510_REG_SIG_GAIN, 0xff, 0x58 },
	{ RT5510_REG_RESV6, 0xff, 0xce },
	{ RT5510_REG_SIGMAX, 0xffff, 0x7fff },
	{ RT5510_REG_DA_GAIN, 0xffff, 0x0116 },
	{ RT5510_REG_TDM_CFG3, 0x1800, 0x0800 },
	{ RT5510_REG_DRE_CTRL, 0x1f, 0x07 },
};

static unsigned int rt5510_codec_io_read(struct snd_soc_codec *codec,
					 unsigned int reg)
{
#ifdef CONFIG_RT_REGMAP
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT5510_GET_ADDR(reg), reg_size = RT5510_GET_SIZE(reg);
	struct rt_reg_data rrd = {0};
	int ret = 0;

	dev_dbg(codec->dev,
		"%s: reg=0x%02x, size %d\n", __func__, reg_addr, reg_size);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not invalid reg size %d\n", reg_size);
		return -ENOTSUPP;
	}
	ret = rt_regmap_reg_read(chip->regmap, &rrd, reg_addr);
	return ret < 0 ? ret : rrd.rt_data.data_u32;
#else
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT5510_GET_ADDR(reg), reg_size = RT5510_GET_SIZE(reg);
	u8 data[4] = {0};
	u32 reg_data = 0;
	int i, ret = 0;

	dev_dbg(codec->dev,
		"%s: reg=0x%02x, size %d\n", __func__, reg_addr, reg_size);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not invalid reg size %d\n", reg_size);
		return -ENOTSUPP;
	}
	ret = i2c_smbus_read_i2c_block_data(chip->i2c, reg_addr,
					    reg_size, data);
	if (ret < 0)
		return ret;
	for (i = 0; i < reg_size; i++) {
		reg_data <<= 8;
		reg_data |= data[i];
	}
	return reg_data;
#endif /* CONFIG_RT_REGMAP */
}

static int rt5510_codec_io_write(struct snd_soc_codec *codec,
				 unsigned int reg, unsigned int data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT5510_GET_ADDR(reg), reg_size = RT5510_GET_SIZE(reg);
	struct rt_reg_data rrd = {0};

	dev_dbg(codec->dev, "%s: reg=0x%02x, size %d, data=0x%08x\n",
		__func__, reg_addr, reg_size, data);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not invalid reg size %d\n", reg_size);
		return -ENOTSUPP;
	}
	return rt_regmap_reg_write(chip->regmap, &rrd, reg_addr, data);
#else
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	u8 reg_addr = RT5510_GET_ADDR(reg), reg_size = RT5510_GET_SIZE(reg);
	u8 reg_data[4] = {0};
	int i;

	dev_dbg(codec->dev, "%s: reg=0x%02x, size %d, data=0x%08x\n",
		__func__, reg_addr, reg_size, data);
	if (reg_size > 4 || reg_size == 0) {
		dev_err(codec->dev, "not invalid reg size %d\n", reg_size);
		return -ENOTSUPP;
	}
	for (i = 0; i < reg_size; i++)
		reg_data[reg_size - i - 1] = (data >> (8 * i)) & 0xff;
	return i2c_smbus_write_i2c_block_data(chip->i2c, reg_addr,
					      reg_size, reg_data);
#endif /* CONFIG_RT_REGMAP */
}

static inline int rt5510_chip_power_on(struct snd_soc_codec *codec, int onoff)
{
	struct rt5510_chip *ri = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s: onoff = %d\n", __func__, onoff);
	mutex_lock(&ri->var_lock);
	if (onoff) {
		if (ri->pwr_cnt++ == 0) {
			ret = snd_soc_update_bits(codec,
						  RT5510_REG_SYSTEM_CTRL,
						  0x01, 0x00);
		}
	} else {
		if (--ri->pwr_cnt == 0) {
			ret = snd_soc_update_bits(codec,
						  RT5510_REG_SYSTEM_CTRL,
						  0x01, 0xff);
		}
		if (ri->pwr_cnt < 0) {
			dev_warn(ri->dev, "not paired on/off\n");
			ri->pwr_cnt = 0;
		}
	}
	mutex_unlock(&ri->var_lock);
	return ret;
}

static int rt5510_codec_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret = 0;

	if (dapm->bias_level == level) {
		dev_warn(codec->dev, "%s: repeat level change\n", __func__);
		goto level_change_skip;
	}
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;
		dev_dbg(codec->dev, "exit low power mode\n");
		ret = rt5510_chip_power_on(codec, 1);
		if (ret < 0)
			dev_err(codec->dev, "power on fail\n");
		break;
	case SND_SOC_BIAS_OFF:
		dev_dbg(codec->dev, "enter low power mode\n");
		dev_info(codec->dev, "last amp irq status = 0x%02x\n",
			 snd_soc_read(codec, RT5510_REG_IRQ_STATUS1));
		ret = rt5510_chip_power_on(codec, 0);
		if (ret < 0)
			dev_err(codec->dev, "power off fail\n");
		break;
	default:
		return -EINVAL;
	}
	dapm->bias_level = level;
	dev_dbg(codec->dev, "c bias_level = %d\n", level);
level_change_skip:
	return 0;
}

static int rt5510_codec_init_setting(struct snd_soc_codec *codec)
{
	const struct codec_reg_val *init_table;
	int i, len, ret = 0;

		init_table = e4_reg_inits;
		len = ARRAY_SIZE(e4_reg_inits);

	for (i = 0; i < len; i++) {
		ret = snd_soc_update_bits(codec, init_table[i].addr,
					init_table[i].mask, init_table[i].data);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int rt5510_codec_register_param_device(struct rt5510_chip *chip)
{
	chip->param_dev =  platform_device_register_data(chip->dev,
				"rt5510-param", chip->dev_cnt, NULL, 0);
	if (!chip->param_dev)
		return -EINVAL;
	return 0;
}

static void rt5510_codec_unregister_param_device(struct rt5510_chip *chip)
{
	platform_device_unregister(chip->param_dev);
}

int rt5510_codec_trigger_param_write(struct rt5510_chip *chip,
				     void *param, int size)
{
	struct snd_soc_codec *codec = chip->codec;
	u8 *data = (u8 *)param;
	int i = 0, ret = 0;

	dev_dbg(codec->dev, "%s: ++\n", __func__);
	mutex_lock(&chip->var_lock);
	ret = chip->pwr_cnt;
	mutex_unlock(&chip->var_lock);
	if (ret) {
		dev_err(codec->dev, "pwr is not at off state\n");
		return -EINVAL;
	}
	ret = rt5510_chip_power_on(codec, 1);
	if (ret < 0)
		dev_err(codec->dev, "%s: power on fail\n", __func__);
	dev_info(codec->dev, "writing proprietary param\n");
	while (i < size) {
		dev_dbg(codec->dev, "[%02x] [%02x] -> [%02x]\n",
			data[i], data[i + 1], *(data + i + 2));
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_write(chip->regmap, data[i],
					    data[i + 1], data + i + 2);
#else
		ret = i2c_smbus_write_i2c_block_data(chip->i2c, data[i],
						     data[i + 1], data + i + 2);
#endif /* CONFIG_RT_REGMAP */
		if (ret <  0) {
			dev_err(codec->dev,
				"reg[0x%02x] write fail\n", data[i]);
			break;
		}
		i += (data[i + 1] + 2);
	}
	ret = rt5510_chip_power_on(codec, 0);
	if (ret < 0)
		dev_err(codec->dev, "%s: power off fail\n", __func__);
	dev_dbg(codec->dev, "%s: --\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(rt5510_codec_trigger_param_write);

static int rt5510_codec_probe(struct snd_soc_codec *codec)
{
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_info(codec->dev, "%s++\n", __func__);
	ret = rt5510_codec_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
	if (ret < 0) {
		dev_err(codec->dev, "config bias standby fail\n");
		return ret;
	}
	ret = rt5510_codec_init_setting(codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s: write init setting fail\n", __func__);
		return ret;
	}
	ret = rt5510_codec_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if (ret < 0) {
		dev_err(codec->dev, "config bias off fail\n");
		return ret;
	}
	ret = rt5510_codec_register_param_device(chip);
	if (ret < 0) {
		dev_err(codec->dev, "create param device fail\n");
		return ret;
	}
#ifdef CONFIG_SND_RICHTEK_SPM
	/* internal use for calibration */
	ret = snd_soc_read(codec, RT5510_REG_CALI_T0);
	if (ret < 0)
		return ret;
	chip->richtek_spm.t0 = (u32) ((ret & 0x07) + 20);
	ret = devm_richtek_spm_classdev_register(codec->dev,
						 &chip->richtek_spm);
	if (ret < 0) {
		dev_err(codec->dev, "register richtek spm classdev fail\n");
		return ret;
	}
#endif /* CONFIG_SND_RICHTEK_SPM */
	chip->codec = codec;
	dev_info(codec->dev, "%s--\n", __func__);
	return 0;
}

static int rt5510_codec_remove(struct snd_soc_codec *codec)
{
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s++\n", __func__);
	chip->codec = NULL;
	rt5510_codec_unregister_param_device(chip);
	dev_dbg(codec->dev, "%s--\n", __func__);
	return 0;
}

static int rt5510_codec_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(1000, 1100);
		break;
	}
	return 0;
}

static int rt5510_codec_classd_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev, "%s: before classd turn on\n", __func__);
		/* config to adaptive mode */
		ret = snd_soc_update_bits(codec,
					  RT5510_REG_BST_CTRL, 0x03, 0x03);
		if (ret < 0) {
			dev_err(codec->dev, "config mode adaptive fail\n");
			return ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* voltage sensing enable */
		ret = snd_soc_update_bits(codec, RT5510_REG_RESV7, 0x04, 0x04);
		if (ret < 0) {
			dev_err(codec->dev, "enable voltage sensing fail\n");
			return ret;
		}
#ifdef CONFIG_SND_RICHTEK_SPM
		ret = richtek_spm_classdev_trigger_ampon(&chip->richtek_spm);
		if (ret < 0)
			dev_err(codec->dev, "trigger spm device ampon fail\n");
#endif /* CONFIG_SND_RICHTEK_SPM */
		dev_info(codec->dev, "rt5510 amp on\n");
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_info(codec->dev, "rt5510 amp off\n");
#ifdef CONFIG_SND_RICHTEK_SPM
		ret = richtek_spm_classdev_trigger_ampoff(&chip->richtek_spm);
		if (ret < 0)
			dev_err(codec->dev, "trigger spm device ampoff fail\n");
#endif /* CONFIG_SND_RICHTEK_SPM */
		/* voltage sensing disable */
		ret = snd_soc_update_bits(codec, RT5510_REG_RESV7, 0x04, 0x00);
		if (ret < 0) {
			dev_err(codec->dev, "disable voltage sensing fail\n");
			return ret;
		}
		/* pop-noise improvement 1 */
		ret = snd_soc_update_bits(codec, RT5510_REG_RESV10, 0x10, 0x10);
		if (ret < 0) {
			dev_err(codec->dev, "pop-noise improvement 1 fail\n");
			return ret;
		}
		/* PWM phase shift 0ns */
		if ((chip->chip_rev & 0xff) >= 0xe2) {
			ret = snd_soc_update_bits(codec, RT5510_REG_RESV23,
						  0xffff, 0x07f8);
			if (ret < 0) {
				dev_err(codec->dev, "pwm phase shift 1 fail\n");
				return ret;
			}
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, "%s: after classd turn off\n", __func__);
		/* PWN phase shift 20ns */
		if ((chip->chip_rev & 0xff) >= 0xe2) {
			ret = snd_soc_update_bits(codec, RT5510_REG_RESV23,
						  0xffff, 0x17f8);
			if (ret < 0) {
				dev_err(codec->dev, "pwn phase shift 2 fail\n");
				return ret;
			}
		}
		/* pop-noise improvement 2 */
		ret = snd_soc_update_bits(codec, RT5510_REG_RESV10, 0x10, 0x00);
		if (ret < 0) {
			dev_err(codec->dev, "pop-noise improvement 2 fail\n");
			return ret;
		}
		/* config to off mode */
		ret = snd_soc_update_bits(codec,
					  RT5510_REG_BST_CTRL, 0x03, 0x00);
		if (ret < 0) {
			dev_err(codec->dev, "config mode off fail\n");
			return ret;
		}
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget rt5510_codec_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("DAC", NULL, RT5510_REG_PLL_CFG1,
			   0, 1, rt5510_codec_dac_event, SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_ADC("VI ADC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV_E("ClassD", RT5510_REG_SYSTEM_CTRL, 2, 0,
			       NULL, 0, rt5510_codec_classd_event,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SPK("SPK", NULL),
};

static const struct snd_soc_dapm_route rt5510_codec_dapm_routes[] = {
	{ "DAC", NULL, "aif_playback"},
	{ "PGA", NULL, "DAC"},
	{ "ClassD", NULL, "PGA"},
	{ "SPK", NULL, "ClassD"},
	{ "VI ADC", NULL, "ClassD"},
	{ "aif_capture", NULL, "VI ADC"},
};

static int rt5510_codec_put_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int ret, put_ret = 0;

	ret = rt5510_chip_power_on(codec, 1);
	if (ret < 0)
		dev_err(codec->dev, "%s: pwr on fail\n", __func__);
	put_ret = snd_soc_put_volsw(kcontrol, ucontrol);
	if (ret < 0)
		return ret;
	ret = rt5510_chip_power_on(codec, 0);
	if (ret < 0)
		dev_err(codec->dev, "%s: pwr off fail\n", __func__);
	return put_ret;
}

static int rt5510_codec_get_volsw(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct rt5510_chip *chip = snd_soc_codec_get_drvdata(codec);
	int ret = -EINVAL;

	if (!strcmp(kcontrol->id.name, "Chip_Rev")) {
		ucontrol->value.integer.value[0] = chip->chip_rev & 0x0f;
		ret = 0;
	}
	return ret;
}

static const DECLARE_TLV_DB_SCALE(vol_ctl_tlv, -1155, 5, 0);

static const struct snd_kcontrol_new rt5510_codec_snd_controls[] = {
	SOC_SINGLE_EXT_TLV("Volume_Ctrl", RT5510_REG_VOL_CTRL, 0, 255,
			   1, snd_soc_get_volsw, rt5510_codec_put_volsw,
			   vol_ctl_tlv),
	SOC_SINGLE_EXT("WDT_Enable", RT5510_REG_WDT_CTRL, 7, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("Hard_Clip_Enable", RT5510_REG_HCLIP_CTRL, 8, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("Clip_Enable", RT5510_REG_SPS_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("BoostMode", RT5510_REG_BST_CTRL, 0, 3, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("DRE_Enable", RT5510_REG_DRE_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("DC_Protect_Enable", RT5510_REG_DC_PROTECT_CTRL, 3, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("I2SLRS", RT5510_REG_DATAO_SEL, 6, 3, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("I2SDOLS", RT5510_REG_DATAO_SEL, 3, 7, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("I2SDORS", RT5510_REG_DATAO_SEL, 0, 7, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	/* for debug purpose */
	SOC_SINGLE_EXT("HPF_AUD_IN_EN", RT5510_REG_HPF_CTRL, 0, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("AUD_LOOP_BACK", RT5510_REG_PATH_BYPASS, 4, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("Mute_Enable", RT5510_REG_SYSTEM_CTRL, 1, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("CS_Comp_Disable", RT5510_REG_PATH_BYPASS, 2, 1, 0,
		       snd_soc_get_volsw, rt5510_codec_put_volsw),
	SOC_SINGLE_EXT("T0_SEL", RT5510_REG_CALI_T0, 0, 7, 0,
		       snd_soc_get_volsw, NULL),
	SOC_SINGLE_EXT("Chip_Rev", SND_SOC_NOPM, 0, 16, 0,
		       rt5510_codec_get_volsw, NULL),
};

static const struct snd_soc_codec_driver rt5510_codec_driver = {
	.probe = rt5510_codec_probe,
	.remove = rt5510_codec_remove,

	.read = rt5510_codec_io_read,
	.write = rt5510_codec_io_write,

	.component_driver = {
		.controls = rt5510_codec_snd_controls,
		.num_controls = ARRAY_SIZE(rt5510_codec_snd_controls),
		.dapm_widgets = rt5510_codec_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(rt5510_codec_dapm_widgets),
		.dapm_routes = rt5510_codec_dapm_routes,
		.num_dapm_routes = ARRAY_SIZE(rt5510_codec_dapm_routes),
	},

	.set_bias_level = rt5510_codec_set_bias_level,
	.idle_bias_off = true,
};

static int rt5510_codec_aif_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(dai->codec);
	int ret = 0;

	dev_dbg(dai->dev, "%s\n", __func__);
	if (dapm->bias_level == SND_SOC_BIAS_OFF)
		ret = rt5510_codec_set_bias_level(dai->codec,
						  SND_SOC_BIAS_STANDBY);
	return ret;
}

static void rt5510_codec_aif_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
}

static int rt5510_codec_aif_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	dev_dbg(dai->dev, "%s\n", __func__);
	return 0;
}

static int rt5510_codec_aif_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params, struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
				     snd_soc_component_get_dapm(dai->component);
	int word_len = params_physical_width(hw_params);
	int aud_bit = params_width(hw_params);
	u16 reg_data = 0;
	int ret = 0;

	dev_dbg(dai->dev, "%s: ++\n", __func__);
	dev_dbg(dai->dev, "format: 0x%08x\n", params_format(hw_params));
	dev_dbg(dai->dev, "rate: 0x%08x\n", params_rate(hw_params));
	dev_dbg(dai->dev, "word_len: %d, aud_bit: %d\n", word_len, aud_bit);
	if (word_len > 32 || word_len < 16) {
		dev_err(dai->dev, "not supported word length\n");
		return -ENOTSUPP;
	}
	switch (aud_bit) {
	case 16:
		reg_data = 3;
		break;
	case 18:
		reg_data = 2;
		break;
	case 20:
		reg_data = 1;
		break;
	case 24:
	case 32:
		reg_data = 0;
		break;
	default:
		return -ENOTSUPP;
	}
	ret = snd_soc_update_bits(dai->codec,
				  RT5510_REG_SERIAL_CFG1, 0xc0, reg_data << 6);
	if (ret < 0) {
		dev_err(dai->dev, "config aud bit fail\n");
		return ret;
	}
	ret = snd_soc_update_bits(dai->codec,
				  RT5510_REG_TDM_CFG3, 0x3f0, word_len << 4);
	if (ret < 0) {
		dev_err(dai->dev, "config word len fail\n");
		return ret;
	}
	dev_dbg(dai->dev, "%s: --\n", __func__);
	return snd_soc_dapm_enable_pin(dapm, "SPK");
}

static int rt5510_codec_aif_hw_free(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_context *dapm =
				     snd_soc_component_get_dapm(dai->component);
	int ret;

	dev_dbg(dai->dev, "%s\n", __func__);
	ret = snd_soc_dapm_disable_pin(dapm, "SPK");
	if (ret < 0)
		return ret;
	return snd_soc_dapm_sync(dapm);
}

static int rt5510_codec_aif_trigger(struct snd_pcm_substream *substream,
	int cmd, struct snd_soc_dai *dai)
{
	int capture = (substream->stream == SNDRV_PCM_STREAM_CAPTURE);

	dev_dbg(dai->dev, "%s: cmd = %d\n", __func__, cmd);
	dev_dbg(dai->dev, "%s: %c\n", __func__, capture ? 'c' : 'p');
	return 0;
}

static const struct snd_soc_dai_ops rt5510_codec_aif_ops = {
	.startup = rt5510_codec_aif_startup,
	.shutdown = rt5510_codec_aif_shutdown,
	.prepare = rt5510_codec_aif_prepare,
	.hw_params = rt5510_codec_aif_hw_params,
	.hw_free = rt5510_codec_aif_hw_free,
	.trigger = rt5510_codec_aif_trigger,
};

#define STUB_RATES	SNDRV_PCM_RATE_8000_192000
#define STUB_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | \
			SNDRV_PCM_FMTBIT_U16_LE | \
			SNDRV_PCM_FMTBIT_S24_LE | \
			SNDRV_PCM_FMTBIT_U24_LE | \
			SNDRV_PCM_FMTBIT_S32_LE | \
			SNDRV_PCM_FMTBIT_U32_LE)

static struct snd_soc_dai_driver rt5510_codec_dai = {
	.name = "rt5510-aif",
	.playback = {
		.stream_name	= "aif_playback",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture = {
		.stream_name	= "aif_capture",
		.channels_min	= 1,
		.channels_max	= 2,
		.rates = STUB_RATES,
		.formats = STUB_FORMATS,
	},
	/* dai properties */
	.symmetric_rates = 1,
	.symmetric_channels = 1,
	.symmetric_samplebits = 1,
	/* dai operations */
	.ops = &rt5510_codec_aif_ops,
};

static inline int rt5510_chip_id_check(struct i2c_client *i2c)
{
	u8 reg_addr = RT5510_GET_ADDR(RT5510_REG_DEVID);
	u8 id[2] = {0};
	int ret = 0;

	i2c_smbus_write_byte_data(i2c, 0x03, 0x00);
	ret = i2c_smbus_read_i2c_block_data(i2c, reg_addr, 2, id);
	if (ret < 0)
		return ret;
	ret = (id[0] << 8) + id[1];
	ret &= 0x0ff0;
	if (ret != 0x00e0 && ret != 0x01e0)
		return -ENODEV;
	i2c_smbus_write_byte_data(i2c, 0x03, 0x01);
	return 0;
}

static inline int _rt5510_chip_sw_reset(struct rt5510_chip *chip)
{
	u8 reg_addr = RT5510_GET_ADDR(RT5510_REG_SYSTEM_CTRL);

	i2c_smbus_write_byte_data(chip->i2c, reg_addr, 0x80);
	msleep(30);
	return 0;
}

static inline int _rt5510_chip_power_on(struct rt5510_chip *chip, int onoff)
{
	u8 reg_addr = RT5510_GET_ADDR(RT5510_REG_SYSTEM_CTRL), reg_data = 0;
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chip->i2c, reg_addr);
	if (ret < 0)
		return ret;
	reg_data = (u8)ret;
	if (onoff)
		reg_data &= (~0x01);
	else
		reg_data |= 0x01;
	return i2c_smbus_write_byte_data(chip->i2c, reg_addr, reg_data);
}

static inline int _rt5510_read_chip_revision(struct rt5510_chip *chip)
{
	u8 reg_addr = RT5510_GET_ADDR(RT5510_REG_DEVID);
	u8 reg_data[2] = {0};
	int ret = 0;

	ret = i2c_smbus_read_i2c_block_data(chip->i2c, reg_addr, 2, reg_data);
	if (ret < 0) {
		dev_err(chip->dev, "get chip revision fail\n");
		return ret;
	}
	chip->chip_rev = (reg_data[0] << 8) | reg_data[1];
	return 0;
}

int rt5510_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct rt5510_chip *chip = NULL;
	static int dev_cnt;
	int ret = 0;

	dev_info(&client->dev, "%s++\n", __func__);
	ret = rt5510_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "chip id check fail\n");
		return ret;
	}
	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->i2c = client;
	chip->dev = &client->dev;
	chip->dev_cnt = dev_cnt++;
	mutex_init(&chip->var_lock);
	i2c_set_clientdata(client, chip);

	/* chip power on */
	ret = _rt5510_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 1 fail\n");
		goto probe_fail;
	}
	/* chip reset first */
	ret = _rt5510_chip_sw_reset(chip);
	if (ret < 0) {
		dev_err(chip->dev, "chip reset fail\n");
		goto probe_fail;
	}
	/* chip power on */
	ret = _rt5510_chip_power_on(chip, 1);
	if (ret < 0) {
		dev_err(chip->dev, "chip power on 2 fail\n");
		goto probe_fail;
	}
	ret = _rt5510_read_chip_revision(chip);
	if (ret < 0) {
		dev_err(chip->dev, "read chip revsion fail\n");
		goto probe_fail;
	}
	ret = _rt5510_chip_power_on(chip, 0);
	if (ret < 0) {
		dev_err(chip->dev, "chip power off fail\n");
		goto probe_fail;
	}
	ret = rt5510_regmap_register(chip);
	if (ret < 0) {
		dev_err(chip->dev, "regmap register fail\n");
		goto probe_fail;
	}
	dev_info(chip->dev, "%s--\n", __func__);
	return snd_soc_register_codec(chip->dev, &rt5510_codec_driver,
				      &rt5510_codec_dai, 1);
probe_fail:
	mutex_destroy(&chip->var_lock);
	return ret;
}

int rt5510_i2c_remove(struct i2c_client *client)
{
	struct rt5510_chip *chip = i2c_get_clientdata(client);

	dev_dbg(chip->dev, "%s++\n", __func__);
	snd_soc_unregister_codec(chip->dev);
	rt5510_regmap_unregister(chip);
	mutex_destroy(&chip->var_lock);
	dev_dbg(chip->dev, "%s--\n", __func__);
	return 0;
}

static int __maybe_unused rt5510_i2c_suspend(struct device *dev)
{
	struct rt5510_chip *chip = dev_get_drvdata(dev);

	dev_dbg(chip->dev, "%s\n", __func__);
	return 0;
}

static int __maybe_unused rt5510_i2c_resume(struct device *dev)
{
	struct rt5510_chip *chip = dev_get_drvdata(dev);

	dev_dbg(chip->dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt5510_pm_ops,
			 rt5510_i2c_suspend, rt5510_i2c_resume);

static const struct of_device_id __maybe_unused rt5510_of_id[] = {
	{ .compatible = "richtek,rt5510", },
	{},
};
MODULE_DEVICE_TABLE(of, rt5510_of_id);

static const struct i2c_device_id rt5510_i2c_id[] = {
	{ "rt5510", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, rt5510_i2c_id);

static struct i2c_driver rt5510_i2c_driver = {
	.driver = {
		.name = "rt5510",
		.owner = THIS_MODULE,
		.pm = &rt5510_pm_ops,
		.of_match_table = of_match_ptr(rt5510_of_id),
	},
	.probe = rt5510_i2c_probe,
	.remove = rt5510_i2c_remove,
	.id_table = rt5510_i2c_id,
};
module_i2c_driver(rt5510_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT5510 SPKAMP Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.6_S");
