/*
 * Exynos9610-RT5665 Audio Machine driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#define CHANGE_DEV_PRINT

#include <linux/input-event-codes.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <soc/samsung/exynos-pmu.h>
#include <sound/samsung/abox.h>
#include "../codecs/rt5665.h"
#include <sound/jack.h>
#include <sound/samsung/sec_audio_debug.h>

#include "jack_rt5665_sysfs_cb.h"

#define CODEC_SAMPLE_RATE_48KHZ		48000
#define CODEC_PLL_48KHZ				24576000
#define CODEC_SAMPLE_RATE_192KHZ	192000
#define CODEC_PLL_192KHZ			49152000

#define RT5665_MCLK_FREQ	26000000
#define RT5665_DAI_ID		16
#define RT5665_DAI_OFFSET	13
#define RT5665_CODEC_MAX	22
#define RT5665_AUX_MAX		2

#if defined(CONFIG_SND_SOC_RT5510)
#define RT5510_DAI_ID		18
#endif

#define CLK_SRC_SCLK	0
#define CLK_SRC_LRCLK	1
#define CLK_SRC_PDM		2
#define CLK_SRC_SELF	3
#define CLK_SRC_MCLK	4
#define CLK_SRC_SWIRE	5

#define CLK_SRC_DAI		0
#define CLK_SRC_CODEC	1

#define RDMA_COUNT                      8
#define WDMA_COUNT                      5

#define UAIF_START			(RDMA_COUNT + WDMA_COUNT)
#define UAIF_COUNT			4
#define SIFS_START			(RDMA_COUNT + WDMA_COUNT + UAIF_COUNT + 2)
#define SIFS_COUNT			3

#define DSIF_COUNT			1
#define SPDY_START			(RDMA_COUNT + WDMA_COUNT + UAIF_COUNT + DSIF_COUNT)

static struct snd_soc_card exynos9610_audio;
static struct snd_soc_codec_conf codec_conf[RT5665_CODEC_MAX];
static struct snd_soc_aux_dev aux_dev[RT5665_AUX_MAX];
static struct clk *xclkout;

struct rt5665_drvdata {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct snd_soc_jack rt5665_headset;
	int aifrate;
	bool use_external_jd;

	int abox_vss_state;
	unsigned int pcm_state;
	struct wake_lock wake_lock;
	unsigned int wake_lock_switch;

	int uaifp_count;
	int uaifc_count;
};

static struct rt5665_drvdata exynos9610_drvdata;
#if defined(CONFIG_SND_SOC_TFA9894)
extern void exynos9610_set_xclkout0_13(void);
#endif

static struct snd_soc_pcm_runtime *exynos9610_get_rtd(struct snd_soc_card *card,
	int id)
{
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_pcm_runtime *rtd = NULL;

	for (dai_link = card->dai_link;
			dai_link - card->dai_link < card->num_links;
			dai_link++) {
			if (id == dai_link->id) {
			rtd = snd_soc_get_pcm_runtime(card, dai_link->name);
			break;
		}
	}

	if (!rtd)
		rtd = snd_soc_get_pcm_runtime(card, card->dai_link[id].name);

	return rtd;
}

static int exynos9610_rt5665_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret, codec_pll_in, codec_pll_out;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	drvdata->pcm_state |= (1 << dai_link->id);

	if ((drvdata->uaifp_count == 0) && (drvdata->uaifc_count == 0)) {
		dev_info(card->dev, "%s: BCLK Enable\n", __func__);
		snd_soc_dai_set_tristate(rtd->cpu_dai, 0);
	}

	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (drvdata->uaifp_count == 0)) {
		drvdata->uaifp_count++;
	} else if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (drvdata->uaifc_count == 0)) {
		drvdata->uaifc_count++;
	} else {
		dev_err(card->dev, "%s: cnt check stream %d, uaifp_cnt %d, uaifc_cnt %d\n",
			__func__, substream->stream, drvdata->uaifp_count, drvdata->uaifc_count);
	}
	dev_info(card->dev, "%s: uaifp_cnt %d, uaifc_cnt %d\n", __func__,
			drvdata->uaifp_count, drvdata->uaifc_count);

	dev_info(card->dev, "%s: %s-%d %dch, %dHz, %dbit, %dbytes (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_width(params), params_buffer_bytes(params),
			drvdata->pcm_state);

	if (params_rate(params) == CODEC_SAMPLE_RATE_192KHZ)
		codec_pll_in = params_rate(params) * params_width(params) * 2;
	else
		codec_pll_in = params_rate(params) * params_width(params) * 2;
	codec_pll_out = params_rate(params) * 512;

	ret = snd_soc_dai_set_pll(codec_dai, 0, RT5665_PLL1_S_BCLK1, codec_pll_in, codec_pll_out);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai RT5665_PLL1_S_BCLK1 not set\n");
		return ret;
	}
	ret = snd_soc_dai_set_sysclk(codec_dai, RT5665_SCLK_S_PLL1, codec_pll_out, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "codec_dai RT5665_PLL1_S_PLL1 not set\n");
		return ret;
	}

	return ret;
}

static int exynos9610_rt5665_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;

	drvdata->pcm_state &= (~(1 << dai_link->id));

	dev_info(card->dev, "%s: %s-%d (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			drvdata->pcm_state);

	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			&& (drvdata->uaifp_count > 0)) {
		drvdata->uaifp_count--;
	} else if ((substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			&& (drvdata->uaifc_count > 0)) {
		drvdata->uaifc_count--;
	} else {
		dev_err(card->dev, "%s: cnt check stream %d, uaifp_cnt %d, uaifc_cnt %d\n",
			__func__, substream->stream, drvdata->uaifp_count, drvdata->uaifc_count);
	}

	if ((drvdata->uaifp_count == 0) && (drvdata->uaifc_count == 0)) {
		dev_info(card->dev, "%s: BCLK Disable\n", __func__);
		snd_soc_dai_set_tristate(rtd->cpu_dai, 1);
	}

	dev_info(card->dev, "%s: uaifp_cnt %d, uaifc_cnt %d\n", __func__,
			drvdata->uaifp_count, drvdata->uaifc_count);

	return 0;
}

static struct snd_soc_ops uaif0_ops = {
	.hw_params = exynos9610_rt5665_hw_params,
	.hw_free = exynos9610_rt5665_hw_free,
};

static int vogue_dai_ops_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;

	drvdata->pcm_state |= (1 << dai_link->id);

	dev_info(card->dev,
		"%s: %s-%d: 0x%x: hw_params: %dch, %dHz, %dbytes, %dbit (pcm_state: 0x%07x)\n",
		__func__, rtd->dai_link->name, substream->stream, dai_link->id,
		params_channels(params), params_rate(params),
		params_buffer_bytes(params), params_width(params),
		drvdata->pcm_state);

	return ret;
}

static int vogue_dai_ops_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;

	drvdata->pcm_state &= (~(1 << dai_link->id));

	dev_info(card->dev, "%s: %s-%d (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			drvdata->pcm_state);

	return ret;
}

static const struct snd_soc_ops uaif_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static const struct snd_soc_ops rdma_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static const struct snd_soc_ops wdma_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx_slot[] = {0, 1};

	/* bclk ratio 64 for DSD64, 128 for DSD128 */
	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	/* channel map 0 1 if left is first, 1 0 if right is first */
	snd_soc_dai_set_channel_map(cpu_dai, 2, tx_slot, 0, NULL);
	return 0;
}

static const struct snd_soc_ops dsif_ops = {
	.hw_params = dsif_hw_params,
};

static int exynos9610_rt5665_codec_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret;
	struct rt5665_drvdata *ctx = snd_soc_card_get_drvdata(rtd->card);
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_jack *jack;

	dev_info(codec->dev, "%s\n", __func__);

	ret = snd_soc_card_jack_new(&exynos9610_audio, "Headset Jack",
								SND_JACK_HEADSET, &ctx->rt5665_headset, NULL, 0);
	if (ret) {
		dev_err(rtd->dev, "Headset Jack creation failed %d\n", ret);
		return ret;
	}

	jack = &ctx->rt5665_headset;
	snd_jack_set_key(jack->jack, SND_JACK_BTN_0, KEY_MEDIA);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_1, KEY_VOICECOMMAND);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_2, KEY_VOLUMEUP);
	snd_jack_set_key(jack->jack, SND_JACK_BTN_3, KEY_VOLUMEDOWN);
	rt5665_set_jack_detect(codec, &ctx->rt5665_headset);

	return ret;
}

static int exynos9610_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_dai *aif1_dai;
	struct snd_soc_codec *codec;
	struct snd_soc_component *cpu;
	struct snd_soc_dapm_context *dapm;
	char name[SZ_32];
	const char *prefix;
	int i;
	struct rt5665_drvdata *drvdata = snd_soc_card_get_drvdata(card);
#if defined(CONFIG_SND_SOC_RT5510)
	struct snd_soc_codec *spk_amp;
	struct snd_soc_dai *aif2_dai;
#endif

	dev_info(card->dev, "%s\n", __func__);

	aif1_dai = exynos9610_get_rtd(card, 0)->cpu_dai;
	cpu = aif1_dai->component;

	aif1_dai = exynos9610_get_rtd(card, RT5665_DAI_ID)->codec_dai;
	codec = aif1_dai->codec;

#if defined(CONFIG_SND_SOC_RT5510)
	aif2_dai = exynos9610_get_rtd(card, RT5510_DAI_ID)->codec_dai;
	spk_amp = aif2_dai->codec;
#endif
	drvdata->codec = codec;

	/* close codec device immediately when pcm is closed */
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "MAINMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SUBMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPEAKER");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1_1 Capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

#if defined(CONFIG_SND_SOC_RT5510)
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(spk_amp), "aif_playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(spk_amp), "aif_capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(spk_amp));
#endif

	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA0 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA2 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA3 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA4 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA5 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA6 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX RDMA7 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA0 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA1 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA2 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA3 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_component_get_dapm(cpu), "ABOX WDMA4 Capture");
	snd_soc_dapm_sync(snd_soc_component_get_dapm(cpu));

	for (i = 0; i < UAIF_COUNT; i++) {
		aif1_dai = exynos9610_get_rtd(card, UAIF_START + i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s UAIF%d Capture", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snprintf(name, sizeof(name), "%s UAIF%d Playback", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	for (i = 0; i < SIFS_COUNT; i++) {
		aif1_dai = exynos9610_get_rtd(card, SIFS_START + i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s SIFS%d Capture", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snprintf(name, sizeof(name), "%s SIFS%d Playback", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	aif1_dai = exynos9610_get_rtd(card, SPDY_START)->cpu_dai;
	cpu = aif1_dai->component;
	dapm = snd_soc_component_get_dapm(cpu);
	prefix = dapm->component->name_prefix;
	snprintf(name, sizeof(name), "%s SPDY Capture", prefix);
	snd_soc_dapm_ignore_suspend(dapm, name);
	snd_soc_dapm_sync(dapm);

	wake_lock_init(&drvdata->wake_lock, WAKE_LOCK_SUSPEND, "vogue-sound");
	drvdata->wake_lock_switch = 0;

	register_rt5665_jack_cb(codec);

	register_debug_mixer(card);

	return 0;
}

static int vogue_headsetmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;
	struct snd_soc_codec *codec = drvdata->codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	dev_info(card->dev, "%s ev: %d, jack_type %d\n", __func__, event, rt5665->jack_type);

	return 0;
}

static int vogue_mainmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_submic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_receiver(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_headphone(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;
	struct snd_soc_codec *codec = drvdata->codec;
	struct rt5665_priv *rt5665 = snd_soc_codec_get_drvdata(codec);

	dev_info(card->dev, "%s ev: %d, jack_type %d\n", __func__, event, rt5665->jack_type);

	return 0;
}

static int vogue_speaker(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_bt_sco_tx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_bt_sco_rx(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_fm_radio_in(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_get_sound_wakelock(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;

	ucontrol->value.integer.value[0] = drvdata->wake_lock_switch;

	return 0;
}

static int vogue_set_sound_wakelock(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;
	unsigned int val;

	val = (unsigned int)ucontrol->value.integer.value[0];
	drvdata->wake_lock_switch = val;

	dev_info(drvdata->dev, "%s: %d\n", __func__, drvdata->wake_lock_switch);

	if (drvdata->wake_lock_switch)
		wake_lock(&drvdata->wake_lock);
	else
		wake_unlock(&drvdata->wake_lock);

	return 0;
}

static const char * const abox_vss_state_enum_texts[] = {
	"NORMAL",
	"INIT FAIL",
	"SUSPENDED",
	"PCM OPEN FAIL",
	"ABOX STUCK",
};

SOC_ENUM_SINGLE_DECL(abox_vss_state_enum, SND_SOC_NOPM, 0,
				abox_vss_state_enum_texts);

int abox_vss_state_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;

	ucontrol->value.enumerated.item[0] = drvdata->abox_vss_state;

	return 0;
}

int abox_vss_state_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct rt5665_drvdata *drvdata = &exynos9610_drvdata;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	const unsigned int item = ucontrol->value.enumerated.item[0];

	if (item >= e->items) {
		dev_err(drvdata->dev, "%s item %d overflow\n", __func__, item);
		return -EINVAL;
	}

	drvdata->abox_vss_state = item;
	dev_info(drvdata->dev, "VSS STATE: %s\n", abox_vss_state_enum_texts[item]);

	return 0;
}

static const struct snd_kcontrol_new exynos9610_audio_controls[] = {
	SOC_DAPM_PIN_SWITCH("HEADPHONE"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
	SOC_DAPM_PIN_SWITCH("MAINMIC"),
	SOC_DAPM_PIN_SWITCH("SUBMIC"),
	SOC_DAPM_PIN_SWITCH("HEADSETMIC"),
	SOC_ENUM_EXT("ABOX VSS State", abox_vss_state_enum, abox_vss_state_get, abox_vss_state_put),
	SOC_SINGLE_BOOL_EXT("Sound Wakelock", 0,
			vogue_get_sound_wakelock, vogue_set_sound_wakelock),
};

const struct snd_soc_dapm_widget exynos9610_audio_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("HEADSETMIC", vogue_headsetmic),
	SND_SOC_DAPM_MIC("MAINMIC", vogue_mainmic),
	SND_SOC_DAPM_MIC("SUBMIC", vogue_submic),
	SND_SOC_DAPM_SPK("RECEIVER", vogue_receiver),
	SND_SOC_DAPM_HP("HEADPHONE", vogue_headphone),
	SND_SOC_DAPM_SPK("SPEAKER", vogue_speaker),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", vogue_bt_sco_tx),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", vogue_bt_sco_rx),
	SND_SOC_DAPM_MIC("FM IN", vogue_fm_radio_in),
};

static struct snd_soc_dai_link exynos9610_dai[] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.platform_name = "14a51000.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 0,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.platform_name = "14a51100.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.platform_name = "14a51200.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 2,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.platform_name = "14a51300.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 3,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.platform_name = "14a51400.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 4,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.platform_name = "14a51500.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 5,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.platform_name = "14a51600.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 6,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.platform_name = "14a51700.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 7,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.platform_name = "14a52000.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 8,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.platform_name = "14a52100.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 9,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.platform_name = "14a52200.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 10,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.platform_name = "14a52300.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 11,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.platform_name = "14a52400.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 12,
	},
#if IS_ENABLED(SND_SOC_SAMSUNG_DISPLAYPORT)
	{
		.name = "DP Audio",
		.stream_name = "DP Audio",
		.cpu_dai_name = "audio_cpu_dummy",
		.platform_name = "dp_dma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif0_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.init = exynos9610_rt5665_codec_init,
		.id = 16,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.platform_name = "snd-soc-dummy",
#if !defined(CONFIG_SND_SOC_TFA9894)
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
#endif
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.id = 17,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.id = 18,
	},
	{
		.name = "UAIF4",
		.stream_name = "UAIF4",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.id = 19,
	},
	{
		.name = "DSIF",
		.stream_name = "DSIF",
		.cpu_dai_name = "DSIF",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "SPDY",
		.stream_name = "SPDY",
		.cpu_dai_name = "SPDY",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_capture = 1,
		.id = 20,
	},
	{
		.name = "SIFS0",
		.stream_name = "SIFS0",
		.cpu_dai_name = "SIFS0",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS1",
		.stream_name = "SIFS1",
		.cpu_dai_name = "SIFS1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS2",
		.stream_name = "SIFS2",
		.cpu_dai_name = "SIFS2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

static int read_platform(struct device_node *np, const char * const prop,
				  struct device_node **dai)
{
	int ret = 0;

	np = of_get_child_by_name(np, prop);
	if (!np)
		return -ENOENT;

	*dai = of_parse_phandle(np, "sound-dai", 0);
	if (!*dai) {
		ret = -ENODEV;
		goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_cpu(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;

	np = of_get_child_by_name(np, "cpu");
	if (!np)
		return -ENOENT;

	dai_link->cpu_of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!dai_link->cpu_of_node) {
		ret = -ENODEV;
		goto out;
	}

	if (dai_link->cpu_dai_name == NULL) {
		/* Ignoring the return as we don't register DAIs to the platform */
		ret = snd_soc_of_get_dai_name(np, &dai_link->cpu_dai_name);
		if (ret)
			goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_codec(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	np = of_get_child_by_name(np, "codec");
	if (!np)
		return -ENOENT;

	return snd_soc_of_get_dai_link_codecs(dev, np, dai_link);
}

static void control_xclkout(bool on)
{
	if (on) {
		clk_prepare_enable(xclkout);
#if defined(CONFIG_SND_SOC_TFA9894)
		exynos9610_set_xclkout0_13();
#endif
	} else
		clk_disable_unprepare(xclkout);
}

static struct snd_soc_card exynos9610_audio = {
	.name = "Exynos9610-Audio",
	.owner = THIS_MODULE,
	.dai_link = exynos9610_dai,
	.num_links = ARRAY_SIZE(exynos9610_dai),

	.late_probe = exynos9610_late_probe,

	.controls = exynos9610_audio_controls,
	.num_controls = ARRAY_SIZE(exynos9610_audio_controls),
	.dapm_widgets = exynos9610_audio_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(exynos9610_audio_dapm_widgets),

	.drvdata = (void *)&exynos9610_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int exynos9610_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &exynos9610_audio;
	struct rt5665_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	int nlink = 0;
	int i, rc, ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	card->dev = &pdev->dev;
	drvdata->dev = card->dev;
	snd_soc_card_set_drvdata(card, drvdata);

	xclkout = devm_clk_get(&pdev->dev, "xclkout");
	if (IS_ERR(xclkout)) {
		dev_err(&pdev->dev, "xclkout get failed\n");
		xclkout = NULL;
	}
	control_xclkout(true);
	dev_info(&pdev->dev, "xclkout is enabled\n");

	for_each_child_of_node(np, dai) {
		struct snd_soc_dai_link *link = &exynos9610_dai[nlink];

		if (!link->name)
			link->name = dai->name;
		if (!link->stream_name)
			link->stream_name = dai->name;

		if (!link->cpu_name) {
			ret = read_cpu(dai, card->dev, link);
			if (ret) {
				dev_err(card->dev, "Failed to parse cpu DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}
		}

		if (!link->platform_name) {
			ret = read_platform(dai, "platform",
				&link->platform_of_node);
			if (ret) {
				link->platform_of_node = link->cpu_of_node;
				dev_info(card->dev, "Cpu node is used as platform for %s: %d\n",
						dai->name, ret);
			}
		}

		if (!link->codec_name) {
			ret = read_codec(dai, card->dev, link);
			if (ret) {
				dev_err(card->dev, "Failed to parse codec DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}

		}

		link->dai_fmt = snd_soc_of_parse_daifmt(dai, NULL, NULL, NULL);

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_err(card->dev, "No DAIs specified\n");
		return -EINVAL;
	}

	if (of_property_read_bool(np, "samsung,routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "samsung,routing");
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(codec_conf); i++) {
		codec_conf[i].of_node = of_parse_phandle(np, "samsung,codec", i);
		if (IS_ERR_OR_NULL(codec_conf[i].of_node)) {
			exynos9610_audio.num_configs = i;
			break;
		}

		rc = of_property_read_string_index(np, "samsung,prefix", i,
				&codec_conf[i].name_prefix);
		if (rc < 0)
			codec_conf[i].name_prefix = "";
	}

	for (i = 0; i < ARRAY_SIZE(aux_dev); i++) {
		aux_dev[i].codec_of_node = of_parse_phandle(np, "samsung,aux", i);
		if (IS_ERR_OR_NULL(aux_dev[i].codec_of_node)) {
			exynos9610_audio.num_aux_devs = i;
			break;
		}
	}

	ret = devm_snd_soc_register_card(card->dev, card);
	if (ret) {
		dev_err(card->dev, "%s: snd_soc_register_card() failed:%d\n", __func__, ret);
		sec_audio_bootlog(3, "%s: snd_soc_register_card() failed:%d\n", __func__, ret);
	} else {
		dev_info(card->dev, "%s done\n", __func__);
		sec_audio_bootlog(6, "%s done\n", __func__);
	}

	return ret;
}

static const struct of_device_id exynos9610_audio_of_match[] = {
	{.compatible = "samsung,exynos9610-audio",},
	{},
};
MODULE_DEVICE_TABLE(of, exynos9610_audio_of_match);

static struct platform_driver exynos9610_audio_driver = {
	.driver = {
		.name = "Exynos9610-audio",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(exynos9610_audio_of_match),
	},

	.probe = exynos9610_audio_probe,
};

module_platform_driver(exynos9610_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos9610 Audio Driver");
MODULE_AUTHOR("Shinhyung Kang <s47.kang@samsung.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos9610-audio");
