# FM capture diagnostics

The tools that found the Xperia Z2 FM "flicking" (see
`../../drivers/audio/README.md`, "The 41.6 Hz flicking").

Record the raw capture on the phone and copy it to a workstation:

    arecord -D hw:0,3 -f S16_LE -r 48000 -c 2 -t raw -d 40 fm.raw

Analyse (needs numpy; a venv is fine):

- `fmanalyse.py fm.raw` — levels, identical/zero frames, second-difference
  spikes and their spacing, spectral peaks, L/R pairing checks.
- `fmcomb.py fm.raw` — comb search in the 16–24 kHz band (FM broadcast audio
  stops at 15 kHz, so a regular line spacing there is a periodic artefact),
  synchronous-averaging search for a repeating click period, the largest
  events in context.
- `fmglitch.py fm.raw [period]` — for a found period: burst phase per block,
  its drift over time, the folded glitch shape, raw frames around bursts, and
  the drop-vs-insert step test.

Clocks, on the phone (root, /dev/mem, real-time priority; stop audio
capture while it runs or the capture overruns):

- `padclock.c` — `cc -O2 -o padclock padclock.c; sudo ./padclock 80 8` times
  the rising edges of a TLMM pad (msm8974 base 0xfd510000) and reports the
  median interval and a stall-tolerant rate. Resolves word select (48 kHz)
  cleanly; a 1.5 MHz bit clock only shows as toggling.
- `dmarate.py DEV SECS` — capture DMA rate from the instants `hw_ptr`
  changes in `/proc/asound/card0/pcmDEVc/sub0/status`; run a capture with a
  small period (`--period-size=480`) alongside.
