// Hold both directions of a PCM open, without reading or writing.
//
// The voice front end carries no data through the processor: the DSP moves
// the audio. What opening it does is bring up the back ends and start the
// DSP's voice session, and both have to stay up for as long as the call
// lasts. This opens playback and capture, configures and prepares them, then
// waits.
//
//   holdpcm hw:0,5 30
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static snd_pcm_t *setup(const char *dev, snd_pcm_stream_t dir, const char *what)
{
	snd_pcm_t *pcm = NULL;
	snd_pcm_hw_params_t *hw;
	unsigned int rate = 8000;
	int err;

	err = snd_pcm_open(&pcm, dev, dir, 0);
	if (err < 0) {
		fprintf(stderr, "%s: open: %s\n", what, snd_strerror(err));
		return NULL;
	}
	snd_pcm_hw_params_alloca(&hw);
	snd_pcm_hw_params_any(pcm, hw);
	snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(pcm, hw, 1);
	snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
	err = snd_pcm_hw_params(pcm, hw);
	if (err < 0) {
		fprintf(stderr, "%s: hw_params: %s\n", what, snd_strerror(err));
		snd_pcm_close(pcm);
		return NULL;
	}
	err = snd_pcm_prepare(pcm);
	if (err < 0)
		fprintf(stderr, "%s: prepare: %s\n", what, snd_strerror(err));
	printf("%s: open and prepared\n", what);
	return pcm;
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "hw:0,5";
	int secs = argc > 2 ? atoi(argv[2]) : 30;
	snd_pcm_t *p = setup(dev, SND_PCM_STREAM_PLAYBACK, "playback");
	snd_pcm_t *c = setup(dev, SND_PCM_STREAM_CAPTURE, "capture");

	if (!p || !c) {
		fprintf(stderr, "could not hold both directions open\n");
		return 1;
	}
	printf("holding open for %d seconds\n", secs);
	fflush(stdout);
	sleep(secs);
	snd_pcm_close(p);
	snd_pcm_close(c);
	printf("released\n");
	return 0;
}
