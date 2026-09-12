// SPDX-License-Identifier: GPL-2.0-only
/*
 * V4L2 radio driver for the FM receiver inside Qualcomm WCNSS (WCN36xx).
 *
 * Copyright (c) 2026 Rob Watson <rob@mediaeden.com>
 *
 * The tuner lives in the WCNSS coprocessor alongside Wi-Fi and Bluetooth and
 * is reached over a single SMD channel, "APPS_FM", carrying an HCI-like
 * command and event protocol. This driver attaches the same way btqcomsmd
 * does: as a platform device below the WCNSS control node, taking the channel
 * from qcom_wcnss_open_channel().
 *
 * The protocol is not documented publicly. Opcodes, event codes and payload
 * layouts here were taken from the downstream radio-iris driver, which is the
 * only reference that exists.
 *
 * NOT YET TESTED ON HARDWARE. See the README beside this file.
 */

#include <linux/cleanup.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/slab.h>
#include <linux/soc/qcom/wcnss_ctrl.h>
#include <linux/unaligned.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>

#define WCNSS_FM_CHANNEL	"APPS_FM"

/* Packet types */
#define FM_PKT_COMMAND		0x11
#define FM_PKT_EVENT		0x14

/* Opcode groups */
#define FM_OGF_RECV_CTRL	0x13
#define FM_OGF_COMMON_CTRL	0x15

#define fm_opcode(ogf, ocf)	((u16)(((ocf) & 0x03ff) | ((ogf) << 10)))

/* Receiver control commands */
#define FM_OCF_ENABLE_RECV	0x0001
#define FM_OCF_DISABLE_RECV	0x0002
#define FM_OCF_SET_RECV_CONF	0x0004
#define FM_OCF_SET_MUTE_MODE	0x0005
#define FM_OCF_SET_STEREO_MODE	0x0006
#define FM_OCF_SET_ANTENNA	0x0007
#define FM_OCF_GET_STATION_PARAM 0x000a
#define FM_OCF_SEARCH_STATIONS	0x000e
#define FM_OCF_CANCEL_SEARCH	0x0011

/* Common control commands */
#define FM_OCF_TUNE_STATION	0x0001

/* Events */
#define FM_EV_TUNE_STATUS	0x01
#define FM_EV_SEARCH_PROGRESS	0x05
#define FM_EV_CMD_COMPLETE	0x0f
#define FM_EV_CMD_STATUS	0x10

#define FM_CMD_TIMEOUT		msecs_to_jiffies(5000)
#define FM_SEEK_TIMEOUT		msecs_to_jiffies(10000)

/* Band limits in kHz. Europe/most of the world; 87.5 to 108.0 MHz. */
#define FM_FREQ_LOW		87500
#define FM_FREQ_HIGH		108000
#define FM_FREQ_STEP		100

/* v4l2 frequencies are in 62.5 Hz units for tuners without CAP_LOW */
#define KHZ_TO_V4L2(k)		((k) * 16)
#define V4L2_TO_KHZ(f)		((f) / 16)

struct fm_recv_conf {
	u8 emphasis;
	u8 ch_spacing;
	u8 rds_std;
	u8 hlsi;
	__le32 band_low;
	__le32 band_high;
} __packed;

struct fm_tune_status {
	u8 sub_event;
	__le32 station_freq;
	u8 serv_avble;
	u8 rssi;
	u8 stereo_prg;
	u8 rds_sync_status;
	u8 mute_mode;
	s8 sinr;
	u8 intf_det_th;
} __packed;

struct wcnss_fm {
	struct device *dev;
	struct rpmsg_endpoint *channel;

	struct v4l2_device v4l2_dev;
	struct video_device vdev;
	struct v4l2_ctrl_handler ctrls;
	struct mutex lock;		/* serialises commands and V4L2 ops */

	struct completion cmd_done;
	u16 pending_opcode;
	u8 cmd_status;

	struct completion seek_done;

	u32 frequency;			/* kHz */
	u8 rssi;
	bool stereo;
	bool enabled;
};

/* ---------------------------------------------------------------- command */

static int wcnss_fm_send(struct wcnss_fm *fm, u16 opcode,
			 const void *payload, u8 plen)
{
	u8 buf[4 + 255];
	int ret;

	if (plen > sizeof(buf) - 4)
		return -EINVAL;

	buf[0] = FM_PKT_COMMAND;
	put_unaligned_le16(opcode, &buf[1]);
	buf[3] = plen;
	if (plen)
		memcpy(&buf[4], payload, plen);

	ret = rpmsg_send(fm->channel, buf, 4 + plen);
	if (ret)
		dev_err(fm->dev, "failed to send opcode %#x: %d\n", opcode, ret);

	return ret;
}

static int wcnss_fm_cmd(struct wcnss_fm *fm, u16 opcode,
			const void *payload, u8 plen)
{
	int ret;

	reinit_completion(&fm->cmd_done);
	fm->pending_opcode = opcode;
	fm->cmd_status = 0;

	ret = wcnss_fm_send(fm, opcode, payload, plen);
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&fm->cmd_done, FM_CMD_TIMEOUT)) {
		dev_err(fm->dev, "timeout waiting for opcode %#x\n", opcode);
		return -ETIMEDOUT;
	}

	if (fm->cmd_status) {
		dev_err(fm->dev, "opcode %#x failed with status %#x\n",
			opcode, fm->cmd_status);
		return -EIO;
	}

	return 0;
}

/* ------------------------------------------------------------------ events */

static void wcnss_fm_tune_status(struct wcnss_fm *fm, const void *data, int len)
{
	const struct fm_tune_status *st = data;

	if (len < sizeof(*st))
		return;

	fm->frequency = le32_to_cpu(st->station_freq);
	fm->rssi = st->rssi;
	fm->stereo = st->stereo_prg;
}

static int wcnss_fm_rx(struct rpmsg_device *rpdev, void *data, int len,
		       void *priv, u32 addr)
{
	struct wcnss_fm *fm = priv;
	const u8 *buf = data;
	u8 evt, plen;

	if (len < 3 || buf[0] != FM_PKT_EVENT)
		return 0;

	evt = buf[1];
	plen = buf[2];
	if (len < 3 + plen)
		return 0;

	switch (evt) {
	case FM_EV_CMD_COMPLETE:
		/* payload: num_hci_cmd_pkts(1), opcode(2), status(1) */
		if (plen < 4)
			break;
		if (get_unaligned_le16(&buf[4]) != fm->pending_opcode) {
			dev_dbg(fm->dev, "stray completion for %#x\n",
				get_unaligned_le16(&buf[4]));
			break;
		}
		fm->cmd_status = buf[6];
		complete(&fm->cmd_done);
		break;
	case FM_EV_CMD_STATUS:
		/* payload: status(1), num_hci_cmd_pkts(1), opcode(2) */
		if (plen >= 1)
			fm->cmd_status = buf[3];
		complete(&fm->cmd_done);
		break;
	case FM_EV_TUNE_STATUS:
		wcnss_fm_tune_status(fm, &buf[3], plen);
		break;
	case FM_EV_SEARCH_PROGRESS:
		complete(&fm->seek_done);
		break;
	default:
		dev_dbg(fm->dev, "unhandled event %#x\n", evt);
		break;
	}

	return 0;
}

/* ------------------------------------------------------------------- radio */

static int wcnss_fm_enable(struct wcnss_fm *fm)
{
	struct fm_recv_conf conf = {
		.emphasis = 0,			/* 75 us; 1 would be 50 us */
		.ch_spacing = 0,		/* 200 kHz */
		.rds_std = 0,			/* RBDS */
		.hlsi = 0,
		.band_low = cpu_to_le32(FM_FREQ_LOW),
		.band_high = cpu_to_le32(FM_FREQ_HIGH),
	};
	u8 antenna = 0;			/* headset wire antenna */
	int ret;

	ret = wcnss_fm_cmd(fm, fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_ENABLE_RECV),
			   &conf, sizeof(conf));
	if (ret)
		return ret;

	ret = wcnss_fm_cmd(fm, fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_SET_ANTENNA),
			   &antenna, sizeof(antenna));
	if (ret)
		dev_warn(fm->dev, "could not select the antenna: %d\n", ret);

	fm->enabled = true;
	return 0;
}

static int wcnss_fm_disable(struct wcnss_fm *fm)
{
	int ret;

	if (!fm->enabled)
		return 0;

	ret = wcnss_fm_cmd(fm, fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_DISABLE_RECV),
			   NULL, 0);
	fm->enabled = false;

	return ret;
}

static int wcnss_fm_tune(struct wcnss_fm *fm, u32 khz)
{
	__le32 freq = cpu_to_le32(khz);

	return wcnss_fm_cmd(fm,
			    fm_opcode(FM_OGF_COMMON_CTRL, FM_OCF_TUNE_STATION),
			    &freq, sizeof(freq));
}

/* ------------------------------------------------------------------- v4l2 */

static int wcnss_fm_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	strscpy(cap->driver, "radio-wcnss-fm", sizeof(cap->driver));
	strscpy(cap->card, "WCNSS FM Receiver", sizeof(cap->card));
	strscpy(cap->bus_info, "platform:radio-wcnss-fm", sizeof(cap->bus_info));

	return 0;
}

static int wcnss_fm_g_tuner(struct file *file, void *priv,
			    struct v4l2_tuner *tuner)
{
	struct wcnss_fm *fm = video_drvdata(file);
	int ret;

	if (tuner->index)
		return -EINVAL;

	ret = wcnss_fm_cmd(fm,
			   fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_GET_STATION_PARAM),
			   NULL, 0);
	if (ret)
		return ret;

	strscpy(tuner->name, "FM", sizeof(tuner->name));
	tuner->type = V4L2_TUNER_RADIO;
	tuner->capability = V4L2_TUNER_CAP_STEREO | V4L2_TUNER_CAP_HWSEEK_BOUNDED;
	tuner->rangelow = KHZ_TO_V4L2(FM_FREQ_LOW);
	tuner->rangehigh = KHZ_TO_V4L2(FM_FREQ_HIGH);
	tuner->rxsubchans = fm->stereo ? V4L2_TUNER_SUB_STEREO
				       : V4L2_TUNER_SUB_MONO;
	tuner->audmode = fm->stereo ? V4L2_TUNER_MODE_STEREO
				    : V4L2_TUNER_MODE_MONO;
	tuner->signal = fm->rssi << 8;
	tuner->afc = 0;

	return 0;
}

static int wcnss_fm_s_tuner(struct file *file, void *priv,
			    const struct v4l2_tuner *tuner)
{
	struct wcnss_fm *fm = video_drvdata(file);
	u8 mode;

	if (tuner->index)
		return -EINVAL;

	mode = tuner->audmode == V4L2_TUNER_MODE_MONO ? 1 : 0;

	return wcnss_fm_cmd(fm,
			    fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_SET_STEREO_MODE),
			    &mode, sizeof(mode));
}

static int wcnss_fm_g_frequency(struct file *file, void *priv,
				struct v4l2_frequency *f)
{
	struct wcnss_fm *fm = video_drvdata(file);

	if (f->tuner)
		return -EINVAL;

	f->type = V4L2_TUNER_RADIO;
	f->frequency = KHZ_TO_V4L2(fm->frequency);

	return 0;
}

static int wcnss_fm_s_frequency(struct file *file, void *priv,
				const struct v4l2_frequency *f)
{
	struct wcnss_fm *fm = video_drvdata(file);
	u32 khz;

	if (f->tuner)
		return -EINVAL;

	khz = clamp_t(u32, V4L2_TO_KHZ(f->frequency), FM_FREQ_LOW, FM_FREQ_HIGH);
	khz = rounddown(khz, FM_FREQ_STEP);

	return wcnss_fm_tune(fm, khz);
}

static int wcnss_fm_s_hw_freq_seek(struct file *file, void *priv,
				   const struct v4l2_hw_freq_seek *seek)
{
	struct wcnss_fm *fm = video_drvdata(file);
	u8 params[2];
	int ret;

	if (seek->tuner)
		return -EINVAL;
	if (file->f_flags & O_NONBLOCK)
		return -EWOULDBLOCK;

	params[0] = seek->seek_upward ? 1 : 0;
	params[1] = 1;			/* scan direction, one station */

	reinit_completion(&fm->seek_done);

	ret = wcnss_fm_cmd(fm,
			   fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_SEARCH_STATIONS),
			   params, sizeof(params));
	if (ret)
		return ret;

	if (!wait_for_completion_timeout(&fm->seek_done, FM_SEEK_TIMEOUT)) {
		wcnss_fm_cmd(fm,
			     fm_opcode(FM_OGF_RECV_CTRL, FM_OCF_CANCEL_SEARCH),
			     NULL, 0);
		return -ETIMEDOUT;
	}

	return 0;
}

static int wcnss_fm_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct wcnss_fm *fm = container_of(ctrl->handler, struct wcnss_fm, ctrls);
	u8 mute;

	switch (ctrl->id) {
	case V4L2_CID_AUDIO_MUTE:
		/* 0 = unmuted, 3 = both channels hard muted */
		mute = ctrl->val ? 3 : 0;
		return wcnss_fm_cmd(fm,
				    fm_opcode(FM_OGF_RECV_CTRL,
					      FM_OCF_SET_MUTE_MODE),
				    &mute, sizeof(mute));
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops wcnss_fm_ctrl_ops = {
	.s_ctrl = wcnss_fm_s_ctrl,
};

static int wcnss_fm_open(struct file *file)
{
	struct wcnss_fm *fm = video_drvdata(file);
	int ret;

	ret = v4l2_fh_open(file);
	if (ret)
		return ret;

	guard(mutex)(&fm->lock);

	if (v4l2_fh_is_singular_file(file)) {
		ret = wcnss_fm_enable(fm);
		if (ret) {
			v4l2_fh_release(file);
			return ret;
		}
	}

	return 0;
}

static int wcnss_fm_release(struct file *file)
{
	struct wcnss_fm *fm = video_drvdata(file);

	scoped_guard(mutex, &fm->lock) {
		if (v4l2_fh_is_singular_file(file))
			wcnss_fm_disable(fm);
	}

	return v4l2_fh_release(file);
}

static const struct v4l2_file_operations wcnss_fm_fops = {
	.owner		= THIS_MODULE,
	.open		= wcnss_fm_open,
	.release	= wcnss_fm_release,
	.unlocked_ioctl	= video_ioctl2,
	.poll		= v4l2_ctrl_poll,
};

static const struct v4l2_ioctl_ops wcnss_fm_ioctl_ops = {
	.vidioc_querycap	= wcnss_fm_querycap,
	.vidioc_g_tuner		= wcnss_fm_g_tuner,
	.vidioc_s_tuner		= wcnss_fm_s_tuner,
	.vidioc_g_frequency	= wcnss_fm_g_frequency,
	.vidioc_s_frequency	= wcnss_fm_s_frequency,
	.vidioc_s_hw_freq_seek	= wcnss_fm_s_hw_freq_seek,
	.vidioc_log_status	= v4l2_ctrl_log_status,
	.vidioc_subscribe_event	= v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

/* ------------------------------------------------------------------ driver */

static int wcnss_fm_probe(struct platform_device *pdev)
{
	struct wcnss_fm *fm;
	void *wcnss;
	int ret;

	fm = devm_kzalloc(&pdev->dev, sizeof(*fm), GFP_KERNEL);
	if (!fm)
		return -ENOMEM;

	fm->dev = &pdev->dev;
	fm->frequency = FM_FREQ_LOW;
	mutex_init(&fm->lock);
	init_completion(&fm->cmd_done);
	init_completion(&fm->seek_done);

	wcnss = dev_get_drvdata(pdev->dev.parent);
	if (!wcnss)
		return -EPROBE_DEFER;

	fm->channel = qcom_wcnss_open_channel(wcnss, WCNSS_FM_CHANNEL,
					      wcnss_fm_rx, fm);
	if (IS_ERR(fm->channel))
		return dev_err_probe(&pdev->dev, PTR_ERR(fm->channel),
				     "failed to open the " WCNSS_FM_CHANNEL " channel\n");

	ret = v4l2_device_register(&pdev->dev, &fm->v4l2_dev);
	if (ret)
		goto destroy_channel;

	v4l2_ctrl_handler_init(&fm->ctrls, 1);
	v4l2_ctrl_new_std(&fm->ctrls, &wcnss_fm_ctrl_ops,
			  V4L2_CID_AUDIO_MUTE, 0, 1, 1, 0);
	ret = fm->ctrls.error;
	if (ret)
		goto free_ctrls;

	fm->v4l2_dev.ctrl_handler = &fm->ctrls;

	strscpy(fm->vdev.name, "WCNSS FM Receiver", sizeof(fm->vdev.name));
	fm->vdev.v4l2_dev = &fm->v4l2_dev;
	fm->vdev.fops = &wcnss_fm_fops;
	fm->vdev.ioctl_ops = &wcnss_fm_ioctl_ops;
	fm->vdev.release = video_device_release_empty;
	fm->vdev.lock = &fm->lock;
	fm->vdev.device_caps = V4L2_CAP_TUNER | V4L2_CAP_RADIO |
			       V4L2_CAP_HW_FREQ_SEEK;

	video_set_drvdata(&fm->vdev, fm);
	platform_set_drvdata(pdev, fm);

	ret = video_register_device(&fm->vdev, VFL_TYPE_RADIO, -1);
	if (ret)
		goto free_ctrls;

	dev_info(&pdev->dev, "WCNSS FM receiver registered as %s\n",
		 video_device_node_name(&fm->vdev));

	return 0;

free_ctrls:
	v4l2_ctrl_handler_free(&fm->ctrls);
	v4l2_device_unregister(&fm->v4l2_dev);
destroy_channel:
	rpmsg_destroy_ept(fm->channel);

	return ret;
}

static void wcnss_fm_remove(struct platform_device *pdev)
{
	struct wcnss_fm *fm = platform_get_drvdata(pdev);

	video_unregister_device(&fm->vdev);
	v4l2_ctrl_handler_free(&fm->ctrls);
	v4l2_device_unregister(&fm->v4l2_dev);
	rpmsg_destroy_ept(fm->channel);
}

static const struct of_device_id wcnss_fm_of_match[] = {
	{ .compatible = "qcom,wcnss-fm" },
	{}
};
MODULE_DEVICE_TABLE(of, wcnss_fm_of_match);

static struct platform_driver wcnss_fm_driver = {
	.probe = wcnss_fm_probe,
	.remove = wcnss_fm_remove,
	.driver = {
		.name = "radio-wcnss-fm",
		.of_match_table = wcnss_fm_of_match,
	},
};
module_platform_driver(wcnss_fm_driver);

MODULE_DESCRIPTION("Qualcomm WCNSS FM receiver driver");
MODULE_AUTHOR("Rob Watson <rob@mediaeden.com>");
MODULE_LICENSE("GPL");
