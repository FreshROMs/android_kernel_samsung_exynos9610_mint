/*
 *  jack_rt5665_sysfs_cb.c
 *  Copyright (c) Samsung Electronics
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/input.h>
#include <sound/soc.h>
#include <sound/samsung/sec_audio_sysfs.h>
#include "jack_rt5665_sysfs_cb.h"
#include "../codecs/rt5665.h"
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <sound/jack.h>

static struct snd_soc_codec *rt5665_codec;

static int get_jack_status(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	int status = rt5665->jack_type;
	int report = 0;

	if (status) {
	    report = 1;
	}

	dev_info(codec->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_key_status(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	int report = 0;

	report = rt5665->btn_det;

	dev_info(codec->dev, "%s: %d\n", __func__, report);

	return report;
}

static int get_mic_adc(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	int adc = 0;

	adc = rt5665->adc_val;

	dev_info(codec->dev, "%s: %d\n", __func__, adc);

	return adc;
}

static int force_antenna_enable(int enable)
{
	struct snd_soc_codec *codec = rt5665_codec;
#ifdef CONFIG_SEC_FACTORY
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	struct i2c_client *i2c = to_i2c_client(codec->dev);
#endif

#ifdef CONFIG_SEC_FACTORY
	dev_info(codec->dev, "%s: %d\n", __func__, enable);

	if (enable) {
		of_property_read_u32(i2c->dev.of_node, "realtek,sar-hs-open-gender",
				&rt5665->pdata.sar_hs_open_gender);
		rt5665->pdata.ext_ant_det_gpio = of_get_named_gpio(i2c->dev.of_node,
				"realtek,ext-ant-det-gpio", 0);

		cancel_delayed_work_sync(&rt5665->jack_detect_work);
		queue_delayed_work(system_wq, &rt5665->jack_detect_work,
					msecs_to_jiffies(0));

		/* add delay to enable irq after jack_detect_work called */
		msleep(500);

		if ((rt5665->pdata.sar_hs_open_gender) && (rt5665->pdata.ext_ant_det_gpio)) {
			enable_irq(gpio_to_irq(rt5665->pdata.ext_ant_det_gpio));
		}

		cancel_delayed_work_sync(&rt5665->jack_detect_open_gender_work);
		queue_delayed_work(system_wq, &rt5665->jack_detect_open_gender_work,
					msecs_to_jiffies(100));

	} else {
		rt5665->pdata.sar_hs_open_gender = 0;
		if (rt5665->pdata.ext_ant_det_gpio) {
			disable_irq(gpio_to_irq(rt5665->pdata.ext_ant_det_gpio));
		}

		cancel_delayed_work_sync(&rt5665->jack_detect_work);
		queue_delayed_work(system_wq, &rt5665->jack_detect_work,
					msecs_to_jiffies(0));

	}
#else
	dev_info(codec->dev, "%s: %d only supported for TFM\n", __func__, enable);
#endif
	return 0;
}

static int get_antenna_status(void)
{
	struct snd_soc_codec *codec = rt5665_codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);
	int status;
	int report = 0;

	msleep(1000);

	mutex_lock(&rt5665->open_gender_mutex);
	status = rt5665->jack_type;
	mutex_unlock(&rt5665->open_gender_mutex);

	if (status == SND_JACK_OPEN_GENDER)
		report = 0xA;
	else if (status == SND_JACK_HEADSET)
		report = 0x1;
	else
		report = 0;

	dev_info(codec->dev, "%s: %d\n", __func__, report);

	return report;
}

void register_rt5665_jack_cb(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "%s\n", __func__);

	rt5665_codec = codec;

	audio_register_jack_state_cb(get_jack_status);
	audio_register_key_state_cb(get_key_status);
	audio_register_mic_adc_cb(get_mic_adc);
	audio_register_force_enable_antenna_cb(force_antenna_enable);
	audio_register_antenna_state_cb(get_antenna_status);
}
EXPORT_SYMBOL_GPL(register_rt5665_jack_cb);