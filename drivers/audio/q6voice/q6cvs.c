// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2020, Stephan Gerhold

#include <linux/module.h>
#include <linux/of.h>
#include <linux/soc/qcom/apr.h>
#include "q6cvs.h"
#include "q6voice-common.h"

#define VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION	0x00011140

/*
 * In-call playback: the DSP reads audio from an AFE port and mixes it into
 * the uplink, so the far end hears it. It never passes through a microphone,
 * a codec or the loudspeaker, which is the point of it.
 *
 * The port is normally the AFE pseudoport 0x8005, which an ordinary playback
 * stream is routed to; passing PORT_ID_DEFAULT asks the DSP to use that one.
 * Made an argument because whether this DSP will tap an ordinary port is
 * worth finding out before plumbing a pseudoport through q6afe and
 * q6routing.
 */
#define VSS_IPLAYBACK_CMD_START				0x000112BD
#define VSS_IPLAYBACK_CMD_STOP				0x00011239

#define VSS_IPLAYBACK_PORT_ID_DEFAULT			0xFFFF
#define VSS_IPLAYBACK_PORT_ID_VOICE			0x8005

struct cvs_create_passive_control_session_cmd {
	struct apr_hdr hdr;
	char name[20];
} __packed;

struct q6voice_session *q6cvs_session_create(enum q6voice_path_type path)
{
	struct cvs_create_passive_control_session_cmd cmd;
	struct q6voice_session *cvs;
	const char *session_name;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION;

	session_name = q6voice_get_session_name(path);
	if (session_name)
		strscpy(cmd.name, session_name, sizeof(cmd.name));

	cvs = q6voice_session_create(Q6VOICE_SERVICE_CVS, path, &cmd.hdr);

	return cvs;
}
EXPORT_SYMBOL_GPL(q6cvs_session_create);

struct cvs_start_playback_cmd {
	struct apr_hdr hdr;
	u16 port_id;
} __packed;

int q6cvs_start_playback(struct q6voice_session *cvs, u16 port_id)
{
	struct cvs_start_playback_cmd cmd;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_IPLAYBACK_CMD_START;
	cmd.port_id = port_id;

	pr_info("q6cvs: start in-call playback from port %#x\n", port_id);

	return q6voice_common_send(cvs, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvs_start_playback);

int q6cvs_stop_playback(struct q6voice_session *cvs)
{
	struct apr_pkt cmd;

	cmd.hdr.pkt_size = APR_HDR_SIZE;
	cmd.hdr.opcode = VSS_IPLAYBACK_CMD_STOP;

	return q6voice_common_send(cvs, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvs_stop_playback);

static int q6cvs_probe(struct apr_device *adev)
{
	return q6voice_common_probe(adev, Q6VOICE_SERVICE_CVS);
}

static const struct of_device_id q6cvs_device_id[]  = {
	{ .compatible = "qcom,q6cvs" },
	{},
};
MODULE_DEVICE_TABLE(of, q6cvs_device_id);

static struct apr_driver qcom_q6cvs_driver = {
	.probe = q6cvs_probe,
	.remove = q6voice_common_remove,
	.callback = q6voice_common_callback,
	.driver = {
		.name = "qcom-q6cvs",
		.of_match_table = of_match_ptr(q6cvs_device_id),
	},
};

module_apr_driver(qcom_q6cvs_driver);

MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_DESCRIPTION("Q6 Core Voice Stream");
MODULE_LICENSE("GPL v2");
