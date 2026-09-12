// SPDX-License-Identifier: GPL-2.0
/*
 * ASoC machine driver for MSM8974 boards with ADSP (QDSP6) audio.
 *
 * Copyright (c) 2026 Rob Watson <rob@mediaeden.com>
 *
 * Modelled on sdm845.c and apq8096.c. Front ends are the q6asm
 * MultiMedia DAIs, back ends are q6afe ports. The application processor is
 * the clock master on MI2S.
 *
 * Two clocks have to be enabled, not one: the LPAIF bit clock (48000 * 16 * 2
 * = 1.536 MHz) and the oversampling clock (48000 * 256 = 12.288 MHz) that the
 * bit-clock divider runs from. Requesting only the bit clock is accepted by
 * the ADSP but produces no clock on the pins, and the amplifiers then never
 * see a valid I2S stream, so the DSP's output pipeline backs up and playback
 * stalls after the first couple of periods. The 2013 AVS firmware in this SoC
 * predates AFE_PARAM_ID_CLOCK_SET, so both go through the older
 * AFE_PARAM_ID_LPAIF_CLK_CONFIG command, which q6afe sends for LPAIF_BIT_CLK
 * and LPAIF_OSR_CLK. These are the same values the downstream card uses
 * (msm8974.c, lpass_mi2s_enable).
 *
 * The LPAIF mode-mux registers ("lpaif_*_mode_muxsel") are deliberately not
 * touched: downstream writes them only for the AUXPCM ports, which share pins
 * between I2S and PCM. The MI2S ports do not use them.
 */

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <dt-bindings/sound/qcom,q6afe.h>

#include "common.h"
#include "qdsp6/q6afe.h"

#define MI2S_DEFAULT_RATE	48000
#define MI2S_DEFAULT_BITS	16
#define MI2S_DEFAULT_CHANNELS	2
#define MI2S_BCLK_RATE		(MI2S_DEFAULT_RATE * MI2S_DEFAULT_BITS * \
				 MI2S_DEFAULT_CHANNELS)
#define MI2S_OSR_RATE		(MI2S_DEFAULT_RATE * 256)

struct msm8974_snd_data {
	struct snd_soc_card card;
};

static int msm8974_mi2s_index(int port_id)
{
	switch (port_id) {
	case PRIMARY_MI2S_RX:
	case PRIMARY_MI2S_TX:
		return 0;
	case SECONDARY_MI2S_RX:
	case SECONDARY_MI2S_TX:
		return 1;
	case TERTIARY_MI2S_RX:
	case TERTIARY_MI2S_TX:
		return 2;
	case QUATERNARY_MI2S_RX:
	case QUATERNARY_MI2S_TX:
		return 3;
	default:
		return -1;
	}
}

static int msm8974_snd_startup(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);
	struct snd_soc_dai *codec_dai;
	/*
	 * The provider flags are read from the point of view of the DAI they
	 * are given to. The AFE port drives the bit and word-select clocks,
	 * so the CPU DAI is the provider (BP_FP) and the codecs are the
	 * consumers (CBC_CFC). Handing the codec-side value to the CPU DAI
	 * makes q6afe set ws_src to "external", and the ADSP then waits for a
	 * word-select that nothing generates: every command still succeeds,
	 * but no clock appears on the pins and playback stalls once the DSP
	 * has buffered the first periods.
	 */
	unsigned int cpu_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			       SND_SOC_DAIFMT_BP_FP;
	unsigned int codec_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				 SND_SOC_DAIFMT_CBC_CFC;
	int i, ret;

	if (msm8974_mi2s_index(cpu_dai->id) < 0)
		return 0;

	ret = snd_soc_dai_set_sysclk(cpu_dai, LPAIF_OSR_CLK, MI2S_OSR_RATE,
				     SNDRV_PCM_STREAM_PLAYBACK);
	if (ret) {
		dev_err(rtd->card->dev,
			"failed to enable MI2S oversampling clock: %d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, LPAIF_BIT_CLK, MI2S_BCLK_RATE,
				     SNDRV_PCM_STREAM_PLAYBACK);
	if (ret) {
		dev_err(rtd->card->dev,
			"failed to enable MI2S bit clock: %d\n", ret);
		snd_soc_dai_set_sysclk(cpu_dai, LPAIF_OSR_CLK, 0,
				       SNDRV_PCM_STREAM_PLAYBACK);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(cpu_dai, cpu_fmt);
	if (ret)
		dev_warn(rtd->card->dev, "cpu dai format not accepted: %d\n", ret);

	for_each_rtd_codec_dais(rtd, i, codec_dai) {
		ret = snd_soc_dai_set_fmt(codec_dai, codec_fmt);
		if (ret && ret != -ENOTSUPP)
			dev_warn(rtd->card->dev, "codec dai format not accepted: %d\n", ret);
	}

	return 0;
}

static void msm8974_snd_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_soc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = snd_soc_rtd_to_cpu(rtd, 0);

	if (msm8974_mi2s_index(cpu_dai->id) < 0)
		return;

	snd_soc_dai_set_sysclk(cpu_dai, LPAIF_BIT_CLK, 0,
			       SNDRV_PCM_STREAM_PLAYBACK);
	snd_soc_dai_set_sysclk(cpu_dai, LPAIF_OSR_CLK, 0,
			       SNDRV_PCM_STREAM_PLAYBACK);
}

static int msm8974_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				      struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);
	struct snd_interval *channels = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_CHANNELS);
	struct snd_mask *fmt = hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT);

	rate->min = rate->max = MI2S_DEFAULT_RATE;
	channels->min = channels->max = MI2S_DEFAULT_CHANNELS;
	snd_mask_none(fmt);
	snd_mask_set_format(fmt, SNDRV_PCM_FORMAT_S16_LE);

	return 0;
}

static const struct snd_soc_ops msm8974_be_ops = {
	.startup = msm8974_snd_startup,
	.shutdown = msm8974_snd_shutdown,
};

static void msm8974_add_ops(struct snd_soc_card *card)
{
	struct snd_soc_dai_link *link;
	int i;

	for_each_card_prelinks(card, i, link) {
		if (link->no_pcm == 1) {
			link->ops = &msm8974_be_ops;
			link->be_hw_params_fixup = msm8974_be_hw_params_fixup;
		}
	}
}

static int msm8974_snd_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct msm8974_snd_data *data;
	struct snd_soc_card *card;
	int ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	card = &data->card;
	card->dev = dev;
	card->owner = THIS_MODULE;
	/*
	 * Fixed, so that the UCM configuration is found by a stable path
	 * rather than one derived from the board's model string.
	 */
	card->driver_name = "msm8974";
	dev_set_drvdata(dev, card);
	snd_soc_card_set_drvdata(card, data);

	ret = qcom_snd_parse_of(card);
	if (ret)
		return ret;

	msm8974_add_ops(card);

	return devm_snd_soc_register_card(dev, card);
}

static const struct of_device_id msm8974_snd_device_id[] = {
	{ .compatible = "qcom,msm8974-sndcard" },
	{}
};
MODULE_DEVICE_TABLE(of, msm8974_snd_device_id);

static struct platform_driver msm8974_snd_driver = {
	.probe = msm8974_snd_platform_probe,
	.driver = {
		.name = "msm8974-snd",
		.of_match_table = msm8974_snd_device_id,
	},
};
module_platform_driver(msm8974_snd_driver);

MODULE_DESCRIPTION("MSM8974 ASoC machine driver");
MODULE_LICENSE("GPL");
