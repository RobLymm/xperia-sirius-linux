// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2020, Stephan Gerhold

#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include "qcom,q6voice.h"
#include "q6voice.h"

#define DRV_NAME	"q6voice-dai"

/*
 * Which of the DSP's voice sessions to open.
 *
 * Session 0, "default modem voice", is the classic circuit-switched voice
 * session, and is what a modem of this generation uses: msm8974 predates the
 * multi-mode sessions by several years. The branch this driver came from
 * hardcoded VoiceMMode2, which is a two-SIM multi-mode session on much newer
 * hardware, and the attempt there never got the voice processor to start.
 *
 * Left as a parameter because the only way to find out which session a
 * particular modem firmware will accept is to ask it.
 *
 *   0  default modem voice     6  VoiceMMode1
 *   2  VoLTE                   7  VoiceMMode2
 */
static int voice_path = Q6VOICE_PATH_VOICE;
module_param(voice_path, int, 0644);
MODULE_PARM_DESC(voice_path,
		 "DSP voice session to open (0 = default modem voice)");

/*
 * Start the session from prepare, not from startup.
 *
 * startup runs when the PCM is opened, which is before hw_params has
 * configured the back ends. The voice processor is created with the AFE port
 * numbers it is to use, so creating it that early means naming a transmit
 * port the DSP has not been told about yet: the uplink port was being
 * configured about thirty milliseconds after the session had already
 * started.
 *
 * The downlink did not suffer from this, which is what made it confusing.
 * Its port carries ordinary loudspeaker playback as well, so it is already
 * configured and running long before a call, whatever this driver does. The
 * uplink port has no other user, so only that direction was silent.
 *
 * ASoC runs prepare on the back ends before the front end, and back end
 * hw_params earlier still, so by the time this is called both ports exist
 * and are started.
 */
static int q6voice_dai_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct q6voice *v = snd_soc_dai_get_drvdata(dai);
	int ret;

	ret = q6voice_start(v, voice_path, substream->stream);

	/*
	 * prepare can be called again on a stream that is already running,
	 * after an underrun for instance. That is not an error here.
	 */
	if (ret == -EALREADY)
		return 0;

	return ret;
}

static void q6voice_dai_shutdown(struct snd_pcm_substream *substream,
				 struct snd_soc_dai *dai)
{
	struct q6voice *v = snd_soc_dai_get_drvdata(dai);

	q6voice_stop(v, voice_path, substream->stream);
}

static struct snd_soc_dai_ops q6voice_dai_ops = {
	.prepare = q6voice_dai_prepare,
	.shutdown = q6voice_dai_shutdown,
};

static struct snd_soc_dai_driver q6voice_dais[] = {
	{
		.id = VOICEMMODE2,
		.name = "Voice",
		/* The constraints here are not really meaningful... */
		.playback = {
			.stream_name =	"Voice Playback",
			.formats =	SNDRV_PCM_FMTBIT_S16_LE,
			.rates =	SNDRV_PCM_RATE_8000,
			.rate_min =	8000,
			.rate_max =	8000,
			.channels_min =	1,
			.channels_max =	1,
		},
		.capture = {
			.stream_name =	"Voice Capture",
			.formats =	SNDRV_PCM_FMTBIT_S16_LE,
			.rates =	SNDRV_PCM_RATE_8000,
			.rate_min =	8000,
			.rate_max =	8000,
			.channels_min =	1,
			.channels_max =	1,
		},
		.ops = &q6voice_dai_ops,
	},
};

/* FIXME: Use codec2codec instead */
static struct snd_pcm_hardware q6voice_dai_hardware = {
	.info =			SNDRV_PCM_INFO_INTERLEAVED,
	.buffer_bytes_max =	4096 * 2,
	.period_bytes_min =	2048,
	.period_bytes_max =	4096,
	.periods_min =		2,
	.periods_max =		4,
	.fifo_size =		0,
};

static int q6voice_dai_open(struct snd_soc_component *component,
			    struct snd_pcm_substream *substream)
{
	substream->runtime->hw = q6voice_dai_hardware;
	return 0;
}

static const struct snd_soc_dapm_widget q6voice_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("VOICE_DL", "Voice Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("VOICE_UL", "Voice Capture", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route q6voice_dapm_routes[] = {
	/* TODO: Make routing configurable */
	{"VOICE_UL", NULL, "SLIMBUS_0_TX"},
	{"QUAT_MI2S_RX", NULL, "VOICE_DL"},
};

static const struct snd_soc_component_driver q6voice_dai_component = {
	.name = DRV_NAME,
	.open = q6voice_dai_open,

	.dapm_widgets = q6voice_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(q6voice_dapm_widgets),
	.dapm_routes = q6voice_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(q6voice_dapm_routes),

	/* Needs to probe after q6afe */
	.probe_order = SND_SOC_COMP_ORDER_LATE,
};

static int q6voice_dai_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct q6voice *v;

	v = q6voice_create(dev);
	if (IS_ERR(v))
		return PTR_ERR(v);

	dev_set_drvdata(dev, v);

	return devm_snd_soc_register_component(dev, &q6voice_dai_component,
					       q6voice_dais,
					       ARRAY_SIZE(q6voice_dais));
}

static const struct of_device_id q6voice_dai_device_id[] = {
	{ .compatible = "qcom,q6voice-dais" },
	{},
};
MODULE_DEVICE_TABLE(of, q6voice_dai_device_id);

static struct platform_driver q6voice_dai_platform_driver = {
	.driver = {
		.name = "q6voice-dai",
		.of_match_table = of_match_ptr(q6voice_dai_device_id),
	},
	.probe = q6voice_dai_probe,
};
module_platform_driver(q6voice_dai_platform_driver);

MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_DESCRIPTION("Q6Voice DAI driver");
MODULE_LICENSE("GPL v2");
