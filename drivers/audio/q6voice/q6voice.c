// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
// Copyright (c) 2020, Stephan Gerhold
#define DEBUG
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include "q6cvp.h"
#include "q6cvs.h"
#include "q6mvm.h"
#include "q6voice-common.h"

/* FIXME: Remove */
#define AFE_PORT_ID_PRIMARY_MI2S_RX         0x1000
#define AFE_PORT_ID_PRIMARY_MI2S_TX         0x1001
#define AFE_PORT_ID_SECONDARY_MI2S_RX       0x1002
#define AFE_PORT_ID_SECONDARY_MI2S_TX       0x1003
#define AFE_PORT_ID_TERTIARY_MI2S_RX        0x1004
#define AFE_PORT_ID_TERTIARY_MI2S_TX        0x1005
#define AFE_PORT_ID_QUATERNARY_MI2S_RX      0x1006
#define AFE_PORT_ID_QUATERNARY_MI2S_TX      0x1007

/* SLIMbus Rx port on channel 0. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_RX      0x4000
/* SLIMbus Tx port on channel 0. */
#define AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_TX      0x4001
/* SLIMbus Rx port on channel 1. */

struct q6voice_path_runtime {
	struct q6voice_session *sessions[Q6VOICE_SERVICE_COUNT];
	unsigned int started;
};

struct q6voice_path {
	struct q6voice *v;

	enum q6voice_path_type type;
	/* Serialize access to voice path session */
	struct mutex lock;
	struct q6voice_path_runtime *runtime;
};

struct q6voice {
	struct device *dev;
	struct q6voice_path paths[Q6VOICE_PATH_COUNT];
};

/*
 * A modem call is the modem's stream: the modem creates its own CVS and
 * attaches it to the passive voice manager session, which it finds by name.
 * Creating one here as well and attaching that puts an empty stream in the
 * slot the modem's should occupy. That starts cleanly and moves no audio in
 * either direction. Off by default; the code is kept for a stream the AP
 * really does own, such as VoIP.
 */
static bool attach_stream;
module_param(attach_stream, bool, 0644);
MODULE_PARM_DESC(attach_stream, "create a CVS stream on the AP side and attach it");

static bool unmute = true;
module_param(unmute, bool, 0644);
MODULE_PARM_DESC(unmute, "send an explicit unmute for both directions when a call starts");

/*
 * AFE port the DSP should mix into the uplink, so the far end hears audio
 * this phone plays rather than what its microphone picks up. Needs
 * attach_stream, because the command goes to a voice stream this side owns.
 * Zero leaves in-call playback alone.
 */
static unsigned int playback_port;
module_param(playback_port, uint, 0644);
MODULE_PARM_DESC(playback_port, "AFE port to mix into the uplink (0xffff = the DSP default pseudoport, 0 = off)");

/*
 * The AFE port the voice processor takes the microphone from.
 *
 * Worth being able to change. The transmit leg does not carry audio on this
 * DSP, but naming a port still reserves it, and then nothing on the
 * application processor can record from it: whichever claims the port first
 * keeps it. The processor will not enable at all with PORT_ID_NONE
 * (VSS_IVOCPROC_CMD_ENABLE returns EFAILED), but it accepts any real port,
 * so it can be pointed at an unused one instead.
 */
#define VSS_IVOCPROC_PORT_ID_NONE	0xFFFF

static unsigned int tx_port = AFE_PORT_ID_SLIMBUS_MULTI_CHAN_0_TX;
module_param(tx_port, uint, 0644);
MODULE_PARM_DESC(tx_port, "AFE port the voice processor takes the microphone from");

/* q6voice_start() and _stop() take substream->stream, where 0 is playback. */
#define Q6VOICE_STREAM_PLAYBACK	0

/*
 * Whether both directions of the voice PCM have to be open before the
 * session starts.
 *
 * Opening the capture direction starts the microphone's back end, which
 * reserves its port. That costs the microphone and buys nothing, because the
 * transmit leg carries no audio here. With this off the session starts on the
 * playback direction alone and the downlink works as before. Whatever opens
 * the PCM has to open only playback to match; holdpcm.c takes "playback" as
 * its third argument for that.
 */
static bool require_both = true;
module_param(require_both, bool, 0644);
MODULE_PARM_DESC(require_both, "wait for both directions of the voice PCM before starting");

static int q6voice_path_start(struct q6voice_path *p)
{
	struct device *dev = p->v->dev;
	struct q6voice_session *mvm, *cvs, *cvp;
	int ret;

	dev_dbg(dev, "start path %d\n", p->type);

	mvm = p->runtime->sessions[Q6VOICE_SERVICE_MVM];
	if (!mvm) {
		mvm = q6mvm_session_create(p->type);
		if (IS_ERR(mvm))
			return PTR_ERR(mvm);

		/*
		 * Dual control has to reach the session before anything is
		 * attached to it: it is what tells the DSP the modem is a
		 * co-controller of this session rather than the AP alone.
		 */
		ret = q6mvm_set_dual_control(mvm);
		if (ret) {
			dev_err(mvm->dev, "failed to set dual control: %d\n", ret);
			q6voice_session_release(mvm);
			return ret;
		}

		p->runtime->sessions[Q6VOICE_SERVICE_MVM] = mvm;
	}

	if (attach_stream) {
		cvs = p->runtime->sessions[Q6VOICE_SERVICE_CVS];
		if (!cvs) {
			cvs = q6cvs_session_create(p->type);
			if (IS_ERR(cvs))
				return PTR_ERR(cvs);
			p->runtime->sessions[Q6VOICE_SERVICE_CVS] = cvs;
		}

		ret = q6mvm_attach_stream(mvm, cvs, true);
		if (ret) {
			dev_err(mvm->dev, "failed to attach stream: %d\n", ret);
			return ret;
		}
	}

	cvp = p->runtime->sessions[Q6VOICE_SERVICE_CVP];
	if (!cvp) {
		/* FIXME: Stop hardcoding */
		cvp = q6cvp_session_create(p->type, tx_port,
					   AFE_PORT_ID_QUATERNARY_MI2S_RX);
		if (IS_ERR(cvp))
			return PTR_ERR(cvp);
		p->runtime->sessions[Q6VOICE_SERVICE_CVP] = cvp;
	}

	// ret = q6cvp_send_topology_commit(cvp);
	// if (ret) {
	// 	dev_err(dev, "failed to send topology commit: %d\n", ret);
	// 	goto cvp_err;
	// }

	ret = q6cvp_enable(cvp, true);
	if (ret) {
		dev_err(dev, "failed to enable cvp: %d\n", ret);
		goto cvp_err;
	}

	ret = q6mvm_attach(mvm, cvp, true);
	if (ret) {
		dev_err(dev, "failed to attach cvp to mvm: %d\n", ret);
		goto attach_err;
	}

	ret = q6mvm_start(mvm, true);
	if (ret) {
		dev_err(dev, "failed to start voice: %d\n", ret);
		goto start_err;
	}

	/*
	 * The downlink comes up audible without being told anything, and the
	 * uplink does not, so the two do not default alike: this DSP appears
	 * to start with the microphone direction muted. Android's audio HAL
	 * sets the mute state explicitly at the start of every call rather
	 * than trusting a default, and so do we.
	 *
	 * Not fatal if it fails. A call with one direction is worth more than
	 * no call, and the log says which happened.
	 */
	if (unmute) {
		ret = q6cvp_set_mute(cvp, false);
		if (ret)
			dev_err(dev, "failed to unmute: %d\n", ret);
	}

	/* Also not fatal: a call without it is still a call. */
	if (playback_port && cvs) {
		ret = q6cvs_start_playback(cvs, playback_port);
		if (ret)
			dev_err(dev, "failed to start in-call playback: %d\n",
				ret);
	}

	return 0;

start_err:
	q6mvm_start(mvm, false);
attach_err:
	q6mvm_attach(mvm, cvp, false);
cvp_err:
	q6cvp_enable(cvp, false);
	return ret;
}

int q6voice_start(struct q6voice *v, enum q6voice_path_type path, bool capture)
{
	struct q6voice_path *p = &v->paths[path];
	int ret = 0;

	mutex_lock(&p->lock);
	if (!p->runtime) {
		p->runtime = kzalloc(sizeof(*p), GFP_KERNEL);
		if (!p->runtime) {
			ret = -ENOMEM;
			goto out;
		}
	}

	if (p->runtime->started & BIT(capture)) {
		ret = -EALREADY;
		goto out;
	}

	p->runtime->started |= BIT(capture);

	if (require_both) {
		if (p->runtime->started != 3)
			goto out;
	} else if (!(p->runtime->started & BIT(Q6VOICE_STREAM_PLAYBACK))) {
		goto out;
	}

	ret = q6voice_path_start(p);
	if (ret) {
		p->runtime->started &= ~BIT(capture);
		dev_err(v->dev, "failed to start path %d: %d\n", path, ret);
		goto out;
	}

out:
	mutex_unlock(&p->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(q6voice_start);

static void q6voice_path_stop(struct q6voice_path *p)
{
	struct device *dev = p->v->dev;
	struct q6voice_session *mvm = p->runtime->sessions[Q6VOICE_SERVICE_MVM];
	struct q6voice_session *cvs = p->runtime->sessions[Q6VOICE_SERVICE_CVS];
	struct q6voice_session *cvp = p->runtime->sessions[Q6VOICE_SERVICE_CVP];
	int ret;

	dev_dbg(dev, "stop path %d\n", p->type);

	/*
	 * Stop in-call playback before anything else. The DSP remembers it
	 * across sessions: leave it running and the next call's start command
	 * comes back EALREADY, which looks like a driver that cannot start
	 * playback when in fact it never stopped.
	 */
	if (playback_port && cvs) {
		ret = q6cvs_stop_playback(cvs);
		if (ret)
			dev_err(dev, "failed to stop in-call playback: %d\n",
				ret);
	}

	ret = q6mvm_start(mvm, false);
	if (ret)
		dev_err(dev, "failed to stop voice: %d\n", ret);

	ret = q6mvm_attach(mvm, cvp, false);
	if (ret)
		dev_err(dev, "failed to detach cvp from mvm: %d\n", ret);

	ret = q6cvp_enable(cvp, false);
	if (ret)
		dev_err(dev, "failed to disable cvp: %d\n", ret);

	/* There is only a stream to detach if we were the one who attached it. */
	if (cvs) {
		ret = q6mvm_attach_stream(mvm, cvs, false);
		if (ret)
			dev_err(dev, "failed to detach stream from mvm: %d\n", ret);
	}
}

static void q6voice_path_destroy(struct q6voice_path *p)
{
	struct q6voice_path_runtime *runtime = p->runtime;
	enum q6voice_service_type svc;

	for (svc = 0; svc < Q6VOICE_SERVICE_COUNT; ++svc) {
		if (runtime->sessions[svc])
			q6voice_session_release(runtime->sessions[svc]);
	}

	p->runtime = NULL;
	kfree(runtime);
}

int q6voice_stop(struct q6voice *v, enum q6voice_path_type path, bool capture)
{
	struct q6voice_path *p = &v->paths[path];
	int ret = 0;

	mutex_lock(&p->lock);
	if (!p->runtime || !(p->runtime->started & BIT(capture)))
		goto out;

	if (require_both ? p->runtime->started == 3
			 : (p->runtime->started & BIT(Q6VOICE_STREAM_PLAYBACK)))
		q6voice_path_stop(p);

	p->runtime->started &= ~BIT(capture);

	if (p->runtime->started == 0)
		q6voice_path_destroy(p);

out:
	mutex_unlock(&p->lock);
	return ret;
}
EXPORT_SYMBOL_GPL(q6voice_stop);

static void q6voice_free(void *data)
{
	struct q6voice *v = data;
	enum q6voice_path_type path;

	for (path = 0; path < Q6VOICE_PATH_COUNT; ++path) {
		struct q6voice_path *p = &v->paths[path];

		mutex_lock(&p->lock);
		if (p->runtime) {
			dev_warn(v->dev,
				 "q6voice_remove() called while path %d is active\n",
				 path);

			if (p->runtime->started == 3)
				q6voice_path_stop(p);
			q6voice_path_destroy(p);
		}
		mutex_unlock(&p->lock);
		mutex_destroy(&p->lock);
	}
}

struct q6voice *q6voice_create(struct device *dev)
{
	struct q6voice *v;
	enum q6voice_path_type path;
	int ret;

	v = devm_kzalloc(dev, sizeof(*v), GFP_KERNEL);
	if (!v)
		return ERR_PTR(-ENOMEM);

	v->dev = dev;

	for (path = 0; path < Q6VOICE_PATH_COUNT; ++path) {
		struct q6voice_path *p = &v->paths[path];

		p->v = v;
		p->type = path;
		mutex_init(&p->lock);
	}

	ret = devm_add_action(dev, q6voice_free, v);
	if (ret)
		return ERR_PTR(ret);

	return v;
}
EXPORT_SYMBOL_GPL(q6voice_create);

MODULE_AUTHOR("Stephan Gerhold <stephan@gerhold.net>");
MODULE_DESCRIPTION("Q6Voice driver");
MODULE_LICENSE("GPL v2");
