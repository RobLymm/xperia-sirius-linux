// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2020, Stephan Gerhold

#include <linux/module.h>
#include <linux/of.h>
#include <linux/soc/qcom/apr.h>
#include "q6cvp.h"
#include "q6voice-common.h"

#define VSS_IVOCPROC_DIRECTION_RX	0
#define VSS_IVOCPROC_DIRECTION_TX	1
#define VSS_IVOCPROC_DIRECTION_RX_TX	2

#define VSS_IVOCPROC_PORT_ID_NONE	0xFFFF

#define VSS_IVOCPROC_TOPOLOGY_ID_NONE			0x00010F70
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS		0x00010F71
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS_V2		0x00010F89
#define VSS_IVOCPROC_TOPOLOGY_ID_TX_DM_FLUENCE		0x00010F72

#define VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT		0x00010F77

#define VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING		0x00010F7C
#define VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING		0x00010F7D

#define VSS_ICOMMON_CAL_NETWORK_ID_NONE			0x0001135E

/*
 * Mute and volume are not needed to bring a call up: the DSP defaults to
 * unmuted at a usable level. They are here for a future mixer control, and
 * for in-call mute in a dialer.
 */
#define VSS_IVOLUME_CMD_MUTE_V2				0x0001138B
#define VSS_IVOLUME_CMD_SET_STEP			0x000112C2

#define VSS_IVOLUME_DIRECTION_TX			0
#define VSS_IVOLUME_DIRECTION_RX			1
#define VSS_IVOLUME_MUTE_OFF				0

#define VSS_IVOCPROC_CMD_ENABLE				0x000100C6
#define VSS_IVOCPROC_CMD_DISABLE			0x000110E1

#define VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT    0x00013198

/*
 * Three versions of this command exist, and which one a DSP accepts depends
 * on its age. The struct below is the V2 layout, and V2 is what a 2014 DSP
 * such as the msm8974's implements: asked for V3 it answers
 * ADSP_EUNSUPPORTED and the voice processor session is never created, which
 * leaves a call connected and silent. Both opcodes are from Sony's own
 * kernel for this platform.
 */
#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2	0x000112BF
#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V3	0x00013169

/*
 * The voice processor is created with a processing topology for each
 * direction, and these two defaults are what carry a call on this phone.
 * They need no calibration data, which is worth stating because the obvious
 * assumption is the opposite.
 *
 * Three facts, each established by asking this DSP:
 *
 *  - TOPOLOGY_ID_NONE is accepted and produces a vocproc containing no
 *    processing. A session built that way establishes completely, every
 *    command returning success, and then carries no audio in either
 *    direction. It is the most misleading result available here.
 *  - The V2 topologies, TX_SM_ECNS_V2 among them, are rejected with
 *    EBADPARAM. Those are the ones that need calibration loaded first.
 *  - TX_SM_ECNS with RX_DEFAULT is accepted and carries audio both ways.
 *
 * TX_SM_ECNS is single-microphone echo cancellation and noise suppression,
 * so the uplink has the loudspeaker's own output subtracted from it. Audio
 * played out of the phone's speaker during a call will not reach the far
 * end; that path needs the DSP's in-call playback command instead.
 *
 * Parameters rather than constants because which combination a given DSP
 * will accept is only discoverable by asking it.
 */
static unsigned int tx_topology = VSS_IVOCPROC_TOPOLOGY_ID_TX_SM_ECNS;
module_param(tx_topology, uint, 0644);
MODULE_PARM_DESC(tx_topology, "uplink processing topology (0x10f71 = echo cancel and noise suppress)");

static unsigned int rx_topology = VSS_IVOCPROC_TOPOLOGY_ID_RX_DEFAULT;
module_param(rx_topology, uint, 0644);
MODULE_PARM_DESC(rx_topology, "downlink processing topology (0x10f77 = default)");

static unsigned int vocproc_mode = VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING;
module_param(vocproc_mode, uint, 0644);
MODULE_PARM_DESC(vocproc_mode, "echo canceller mixing mode");

struct vss_ivocproc_cmd_create_full_control_session_v2_cmd {
	struct apr_hdr hdr;

	/*
	 * Vocproc direction. The supported values:
	 * VSS_IVOCPROC_DIRECTION_RX
	 * VSS_IVOCPROC_DIRECTION_TX
	 * VSS_IVOCPROC_DIRECTION_RX_TX
	 */
	u16 direction;

	/*
	 * Tx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	u16 tx_port_id;

	/*
	 * Tx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	u32 tx_topology_id;

	/*
	 * Rx device port ID to which the vocproc connects. If a port ID is
	 * not being supplied, set this to #VSS_IVOCPROC_PORT_ID_NONE.
	 */
	u16 rx_port_id;

	/*
	 * Rx path topology ID. If a topology ID is not being supplied, set
	 * this to #VSS_IVOCPROC_TOPOLOGY_ID_NONE.
	 */
	u32 rx_topology_id;

	/* Voice calibration profile ID. */
	u32 profile_id;

	/*
	 * Vocproc mode. The supported values:
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_EXT_MIXING
	 */
	u32 vocproc_mode;

	/*
	 * Port ID to which the vocproc connects for receiving echo
	 * cancellation reference signal. If a port ID is not being supplied,
	 * set this to #VSS_IVOCPROC_PORT_ID_NONE. This parameter value is
	 * ignored when the vocproc_mode parameter is set to
	 * VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING.
	 */
	u16 ec_ref_port_id;

	/*
	 * Session name string used to identify a session that can be shared
	 * with passive controllers (optional).
	 */
	char name[20];
} __packed;

struct q6voice_session *q6cvp_session_create(enum q6voice_path_type path,
					     u16 tx_port, u16 rx_port)
{
	struct vss_ivocproc_cmd_create_full_control_session_v2_cmd cmd;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2;

	cmd.tx_topology_id = tx_topology;
	cmd.rx_topology_id = rx_topology;

	cmd.direction = VSS_IVOCPROC_DIRECTION_RX_TX;
	cmd.tx_port_id = tx_port;
	cmd.rx_port_id = rx_port;
	cmd.profile_id = VSS_ICOMMON_CAL_NETWORK_ID_NONE;
	cmd.vocproc_mode = vocproc_mode;
	cmd.ec_ref_port_id = VSS_IVOCPROC_PORT_ID_NONE;

	pr_info("q6cvp: create tx port %#x topology %#x, rx port %#x topology %#x, mode %#x\n",
		tx_port, tx_topology, rx_port, rx_topology, vocproc_mode);

	return q6voice_session_create(Q6VOICE_SERVICE_CVP, path, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_session_create);

struct vss_ivolume_cmd_mute_v2_cmd {
	struct apr_hdr hdr;
	u16 direction;
	u16 mute_flag;
	u16 ramp_duration_ms;
} __packed;

struct vss_ivolume_cmd_set_step_cmd {
	struct apr_hdr hdr;
	u16 direction;
	u32 value;
	u16 ramp_duration_ms;
} __packed;

int q6cvp_set_mute(struct q6voice_session *cvp, bool mute)
{
	struct vss_ivolume_cmd_mute_v2_cmd cmd;
	int dir, ret;

	for (dir = VSS_IVOLUME_DIRECTION_TX; dir <= VSS_IVOLUME_DIRECTION_RX; dir++) {
		cmd.hdr.pkt_size = sizeof(cmd);
		cmd.hdr.opcode = VSS_IVOLUME_CMD_MUTE_V2;
		cmd.direction = dir;
		cmd.mute_flag = mute ? 1 : VSS_IVOLUME_MUTE_OFF;
		cmd.ramp_duration_ms = 0;

		ret = q6voice_common_send(cvp, &cmd.hdr);
		if (ret)
			return ret;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(q6cvp_set_mute);

int q6cvp_set_volume(struct q6voice_session *cvp, unsigned int value)
{
	struct vss_ivolume_cmd_set_step_cmd cmd;

	cmd.hdr.pkt_size = sizeof(cmd);
	cmd.hdr.opcode = VSS_IVOLUME_CMD_SET_STEP;
	cmd.direction = VSS_IVOLUME_DIRECTION_RX;
	cmd.value = value;
	cmd.ramp_duration_ms = 0;

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_set_volume);

int q6cvp_enable(struct q6voice_session *cvp, bool state)
{
	struct apr_pkt cmd;

	cmd.hdr.pkt_size = APR_HDR_SIZE;
	cmd.hdr.opcode = state ? VSS_IVOCPROC_CMD_ENABLE : VSS_IVOCPROC_CMD_DISABLE;

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_enable);

int q6cvp_send_topology_commit(struct q6voice_session *cvp)
{
	struct apr_pkt cmd;

	cmd.hdr.pkt_size = APR_HDR_SIZE;
	cmd.hdr.opcode = VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT;

	return q6voice_common_send(cvp, &cmd.hdr);
}
EXPORT_SYMBOL_GPL(q6cvp_send_topology_commit);

static int q6cvp_probe(struct apr_device *adev)
{
	return q6voice_common_probe(adev, Q6VOICE_SERVICE_CVP);
}

static const struct of_device_id q6cvp_device_id[]  = {
	{ .compatible = "qcom,q6cvp" },
	{},
};
MODULE_DEVICE_TABLE(of, q6cvp_device_id);

static struct apr_driver qcom_q6cvp_driver = {
	.probe = q6cvp_probe,
	.remove = q6voice_common_remove,
	.callback = q6voice_common_callback,
	.driver = {
		.name = "qcom-q6cvp",
		.of_match_table = of_match_ptr(q6cvp_device_id),
	},
};

module_apr_driver(qcom_q6cvp_driver);

MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_DESCRIPTION("Q6 Core Voice Processor");
MODULE_LICENSE("GPL v2");
