// SPDX-License-Identifier: GPL-2.0-only
/*
 * ALSA external PCM plugin "fmrepair" for the Sony Xperia Z2 FM capture.
 *
 * The Broadcom FM tuner is a fixed I2S master on the secondary MI2S pads and
 * the LPASS MI2S receiver always samples on its own bit clock, so the two
 * crystals (chip 37.4 MHz, SoC 19.2 MHz) drift past each other by ~27 ppm.
 * Once every ~24 ms (~1152 frames at 48 kHz) the chip's word-select edge lands
 * on the SoC's sampling edge and for ~30 frames the first data bit of each
 * sample (the sign bit) is read at its transition: about half of those
 * samples come out as the true value with bit 15 flipped, a burst of
 * full-scale clicks heard as a 41.6 Hz "flicking". The frame clocks match to
 * 0.6 Hz (nothing is dropped), the chip sends no data as a slave, and the AFE
 * I2S config has no bit-clock polarity field, so the link cannot be fixed at
 * the source (see docs/audio and the sirius-fm-broadcom notes).
 *
 * This plugin exposes a capture PCM that wraps the raw MultiMedia2 device and
 * repairs those bursts: outlier samples (a value that fits its neighbours far
 * better with bit 15 flipped, or a jump of full-scale size) are clustered into
 * bursts, a tracker locks onto their ~1152-frame period, and each burst window
 * is replaced by linear interpolation between the last good frame before it
 * and the first good frame after it, on both channels. Every other frame is
 * passed through unchanged. Bursts far from the tracked phase are ignored
 * unless they are strong, so loud programme transients are not touched.
 * The output is delayed by DELAY frames (4 ms) to see a burst's full extent.
 *
 *   pcm.sirius_fm { type fmrepair; slave.pcm "hw:0,3" }
 *   arecord -D sirius_fm -f S16_LE -r 48000 -c 2 ...
 */
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/pcm_external.h>

#define RING		4096	/* frames of history (power of two) */
#define DELAY		192	/* output delay: lookahead for burst extent */
#define WIN		15	/* median window for the local baseline */
#define HALF		(WIN / 2)
#define CLUSTER		48	/* outliers this close belong to one burst */
#define MIN_OUTLIERS	3	/* outliers needed to declare a burst */
#define STRONG		6	/* a burst this strong is accepted at any phase */
#define MAX_BURST	120	/* a burst is closed after this many frames */
#define MARGIN		6	/* extra frames blanked either side of a burst */
#define P_INIT		1152	/* initial burst period, frames */
#define P_MIN		1050
#define P_MAX		1260
#define PHASE_TOL	150	/* accept a burst this far from the prediction */
#define PRED_HALF	30	/* half-window blanked for a predicted, undetected burst */
#define MAX_BLANKS	16

#define T_FLIP		6000	/* deviation above which a bit-15 flip is tested */
#define T_JUMP		20000	/* deviation that is an outlier regardless */

struct blank { int64_t a, b; };

typedef struct {
	snd_pcm_extplug_t ext;
	int16_t ring[RING][2];
	int64_t n_in;		/* frames received from the slave */
	int64_t n_out;		/* frames delivered to the application */
	/* outlier ring, one flag per frame (either channel) */
	uint8_t outlier[RING];
	int64_t n_cls;		/* frames classified so far */
	/* burst under construction */
	int open;
	int64_t first, last;
	int count;
	/* tracker */
	int locked;
	double period;
	int64_t c_last;		/* centre of the last accepted burst */
	int misses;
	/* scheduled blank intervals [a, b] inclusive, absolute frames */
	struct blank blanks[MAX_BLANKS];
	int nblanks;
} fm_t;

static inline int16_t *frame(fm_t *fm, int64_t n)
{
	return fm->ring[n & (RING - 1)];
}

static int cmp16(const void *a, const void *b)
{
	int x = *(const int16_t *)a, y = *(const int16_t *)b;
	return x < y ? -1 : x > y;
}

/* is frame n (both channels) an outlier? needs frames n-HALF..n+HALF */
static int classify(fm_t *fm, int64_t n)
{
	int ch, out = 0;
	for (ch = 0; ch < 2 && !out; ch++) {
		int16_t w[WIN];
		int k;
		for (k = 0; k < WIN; k++)
			w[k] = frame(fm, n - HALF + k)[ch];
		int x = frame(fm, n)[ch];
		qsort(w, WIN, sizeof(w[0]), cmp16);
		int base = w[HALF];
		int dev = abs(x - base);
		if (dev > T_JUMP) { out = 1; break; }
		if (dev > T_FLIP) {
			int cand = (int16_t)(x ^ 0x8000);
			if (abs(cand - base) * 3 < dev) { out = 1; break; }
		}
		int prev = frame(fm, n - 1)[ch], next = frame(fm, n + 1)[ch];
		if (abs(2 * x - prev - next) > 2 * T_JUMP) out = 1;
	}
	return out;
}

static void add_blank(fm_t *fm, int64_t a, int64_t b)
{
	if (a < 1) a = 1;
	if (fm->nblanks == MAX_BLANKS) {
		memmove(fm->blanks, fm->blanks + 1, sizeof(fm->blanks[0]) * (MAX_BLANKS - 1));
		fm->nblanks--;
	}
	fm->blanks[fm->nblanks].a = a;
	fm->blanks[fm->nblanks].b = b;
	fm->nblanks++;
}

static void close_burst(fm_t *fm)
{
	int64_t centre = (fm->first + fm->last) / 2;
	int accept = 1;

	fm->open = 0;
	if (fm->locked) {
		int64_t pred = fm->c_last + (int64_t)(fm->period + 0.5);
		int64_t off = centre - pred;
		if (off < -PHASE_TOL || off > PHASE_TOL) {
			/* off-phase: only a strong burst is believed */
			accept = fm->count >= STRONG;
			if (accept) {
				/* a stronger burst two periods late is still on phase */
				double d = (double)(centre - fm->c_last);
				if (d > P_MIN && d < P_MAX)
					fm->period += 0.15 * (d - fm->period);
				else if (d > 1.5 * P_MIN)
					fm->period += 0.05 * (d / (double)(int64_t)(d / fm->period + 0.5) - fm->period);
			}
		} else {
			double d = (double)(centre - fm->c_last);
			if (d > P_MIN && d < P_MAX)
				fm->period += 0.15 * (d - fm->period);
		}
		if (accept) { fm->c_last = centre; fm->misses = 0; }
	} else {
		double d = (double)(centre - fm->c_last);
		if (fm->c_last && d > P_MIN && d < P_MAX) {
			fm->locked = 1;
			fm->period = d;
		}
		fm->c_last = centre;
		fm->misses = 0;
	}
	if (accept)
		add_blank(fm, fm->first - MARGIN, fm->last + MARGIN);
}

/* called once per classified frame n */
static void track(fm_t *fm, int64_t n, int outlier)
{
	if (outlier) {
		if (!fm->open) {
			fm->open = 1;
			fm->first = fm->last = n;
			fm->count = 1;
		} else {
			fm->last = n;
			fm->count++;
		}
	}
	if (fm->open && (n - fm->last > CLUSTER || n - fm->first > MAX_BURST)) {
		if (fm->count >= MIN_OUTLIERS)
			close_burst(fm);
		else
			fm->open = 0;
	}
	/* a predicted burst that never showed: blank it anyway, count a miss */
	if (fm->locked && !fm->open) {
		int64_t pred = fm->c_last + (int64_t)(fm->period + 0.5);
		if (n > pred + PHASE_TOL) {
			add_blank(fm, pred - PRED_HALF, pred + PRED_HALF);
			fm->c_last = pred;
			if (++fm->misses >= 3)
				fm->locked = 0;
		}
	}
}

/* output frame m: pass-through or interpolated inside a blank */
static void emit(fm_t *fm, int64_t m, int16_t *out)
{
	int i, ch;
	/* drop blanks that are behind us */
	while (fm->nblanks && fm->blanks[0].b < m) {
		memmove(fm->blanks, fm->blanks + 1, sizeof(fm->blanks[0]) * (fm->nblanks - 1));
		fm->nblanks--;
	}
	for (i = 0; i < fm->nblanks; i++) {
		int64_t a = fm->blanks[i].a, b = fm->blanks[i].b;
		if (m >= a && m <= b) {
			int16_t *pa = frame(fm, a - 1), *pb = frame(fm, b + 1);
			double t = (double)(m - (a - 1)) / (double)(b + 2 - a);
			for (ch = 0; ch < 2; ch++)
				out[ch] = (int16_t)(pa[ch] + (pb[ch] - pa[ch]) * t + 0.5);
			return;
		}
	}
	out[0] = frame(fm, m)[0];
	out[1] = frame(fm, m)[1];
}

static inline int16_t *area_ptr(const snd_pcm_channel_area_t *a, snd_pcm_uframes_t off)
{
	return (int16_t *)((char *)a->addr + (a->first + a->step * off) / 8);
}

static snd_pcm_sframes_t fm_transfer(snd_pcm_extplug_t *ext,
				     const snd_pcm_channel_area_t *dst_areas,
				     snd_pcm_uframes_t dst_offset,
				     const snd_pcm_channel_area_t *src_areas,
				     snd_pcm_uframes_t src_offset,
				     snd_pcm_uframes_t size)
{
	fm_t *fm = ext->private_data;
	snd_pcm_uframes_t i;

	for (i = 0; i < size; i++) {
		int16_t *f = frame(fm, fm->n_in);
		f[0] = *area_ptr(&src_areas[0], src_offset + i);
		f[1] = *area_ptr(&src_areas[1], src_offset + i);
		fm->n_in++;

		/* classify the frame whose whole window has now arrived */
		while (fm->n_cls + HALF + 1 < fm->n_in) {
			int64_t n = fm->n_cls;
			int o = n > HALF ? classify(fm, n) : 0;
			fm->outlier[n & (RING - 1)] = o;
			track(fm, n, o);
			fm->n_cls++;
		}

		int16_t out[2] = { 0, 0 };
		if (fm->n_in > DELAY) {
			emit(fm, fm->n_out, out);
			fm->n_out++;
		}
		*area_ptr(&dst_areas[0], dst_offset + i) = out[0];
		*area_ptr(&dst_areas[1], dst_offset + i) = out[1];
	}
	return size;
}

static int fm_init(snd_pcm_extplug_t *ext)
{
	fm_t *fm = ext->private_data;
	memset(fm->ring, 0, sizeof(fm->ring));
	memset(fm->outlier, 0, sizeof(fm->outlier));
	fm->n_in = fm->n_out = fm->n_cls = 0;
	fm->open = 0;
	fm->locked = 0;
	fm->period = P_INIT;
	fm->c_last = 0;
	fm->misses = 0;
	fm->nblanks = 0;
	return 0;
}

static int fm_close(snd_pcm_extplug_t *ext)
{
	free(ext->private_data);
	return 0;
}

static const snd_pcm_extplug_callback_t fm_callback = {
	.transfer = fm_transfer,
	.init = fm_init,
	.close = fm_close,
};

SND_PCM_PLUGIN_DEFINE_FUNC(fmrepair)
{
	snd_config_iterator_t i, next;
	snd_config_t *sconf = NULL;
	fm_t *fm;
	int err;

	if (stream != SND_PCM_STREAM_CAPTURE) {
		SNDERR("fmrepair is a capture-only plugin");
		return -EINVAL;
	}
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (!strcmp(id, "comment") || !strcmp(id, "type") || !strcmp(id, "hint"))
			continue;
		if (!strcmp(id, "slave")) { sconf = n; continue; }
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!sconf) {
		SNDERR("No slave configuration for fmrepair");
		return -EINVAL;
	}
	fm = calloc(1, sizeof(*fm));
	if (!fm)
		return -ENOMEM;
	fm->ext.version = SND_PCM_EXTPLUG_VERSION;
	fm->ext.name = "Xperia Z2 FM capture burst repair";
	fm->ext.callback = &fm_callback;
	fm->ext.private_data = fm;
	fm_init(&fm->ext);

	err = snd_pcm_extplug_create(&fm->ext, name, root, sconf, stream, mode);
	if (err < 0) {
		free(fm);
		return err;
	}
	snd_pcm_extplug_set_param(&fm->ext, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_S16);
	snd_pcm_extplug_set_slave_param(&fm->ext, SND_PCM_EXTPLUG_HW_FORMAT, SND_PCM_FORMAT_S16);
	snd_pcm_extplug_set_param_minmax(&fm->ext, SND_PCM_EXTPLUG_HW_CHANNELS, 2, 2);
	snd_pcm_extplug_set_slave_param_minmax(&fm->ext, SND_PCM_EXTPLUG_HW_CHANNELS, 2, 2);
	*pcmp = fm->ext.pcm;
	return 0;
}

SND_PCM_PLUGIN_SYMBOL(fmrepair);
