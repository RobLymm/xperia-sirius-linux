# Audio on the Xperia Z2

Speakers, the earpiece and headphone playback all work. Microphones do not:
they are on the same codec as the headphones and still need its capture path
brought up.

## The working path

    PipeWire -> MultiMedia1 front end -> q6asm/q6adm/q6routing
             -> QUATERNARY_MI2S_RX back end (q6afe)
             -> LPAIF Quaternary MI2S, 48 kHz 16-bit stereo
             -> two NXP TFA9890 amplifiers at 7-bit I2C 0x34 and 0x35
                on blsp2_i2c5 (i2c@f9967000)

Everything on the DSP side is mainline: QCOM_APR over the ADSP remoteproc's
`apr_audio_svc` SMD channel, then q6core, q6afe, q6asm, q6adm and q6routing.
The amplifier driver is mainline too (`tfa989x`, which runs the amp without
its own DSP). Two pieces were missing.

## What is in this directory

**`msm8974-sndcard.c`** — the ASoC machine driver, compatible
`qcom,msm8974-sndcard`. It follows the pattern of the existing `sdm845.c` and
`apq8096.c` machine drivers. In `msm8974_snd_startup` it requests
`LPAIF_OSR_CLK` at 48000 x 256 and `LPAIF_BIT_CLK` at 48000 x 16 x 2, then
sets the DAI formats. Needs a Kconfig entry (`SND_SOC_MSM8974`) and a Makefile
line alongside the other qcom machine drivers.

It also turns a **codec-less back end** (a link with a `platform` but no
`codec`) into a DPCM back end instead of the front end `qcom_snd_parse_of()`
would otherwise make it, and gives it word-select-consumer ops (word select
taken from outside; LPASS still runs its own bit clock, because the LPAIF
MI2S block cannot consume an external one). That is how FM audio is captured
(see below): an AFE port wired to another chip that clocks the bus itself.
`drivers-wip/msm8974-sndcard-fm-backend.diff` is this change as a diff against
the speaker-only driver.

## FM radio audio

The FM tuner is in the Broadcom BCM4335C0 Bluetooth chip. Its digital audio
leaves the chip as I2S on the pads Sony wires to the SoC's **secondary MI2S**
(gpio79 bit clock, gpio80 word select, gpio81 data in). The chip is the I2S
master (vendor command `0xFC61` `05 19 18 18 18`); LPASS consumes its clocks.
So the route is:

    Broadcom FM I2S (chip = master) -> SECONDARY_MI2S_TX (q6afe, already in
    mainline) -> q6routing -> its own MultiMedia front end -> capture

No q6afe change is needed for this: `SECONDARY_MI2S_TX` exists upstream. The
only kernel change is the codec-less back-end handling above. The device tree
adds a `sec-mi2s-dai-link` (cpu `SECONDARY_MI2S_TX`, platform q6routing), a
second q6asm front end for capture, and the `sec_mi2s` pinctrl; see the FM
variant in `../../devicetree/` and `../../docs/fm-broadcom.md`.

### The 41.6 Hz "flicking" and the fmrepair plugin

The raw capture carries a periodic click train: a burst of ~30 corrupted
frames every ~1152 frames (24 ms), about half of the samples in a burst being
the true value with bit 15 flipped. It is on every station at the same level,
independent of aerial, signal strength and power, and it is in the captured
samples (a recording plays it back on any machine).

Measured cause (2026-09-14; captures analysed offline, clocks timed on the
pads through /dev/mem — `../../tools/fm-diag/`):

- The chip's word-select clock is 48000.6 Hz and the capture DMA delivers
  48000.0 Hz: the frame clocks match, so no frames are dropped or repeated.
- The chip is a fixed I2S master on its own 37.4 MHz crystal, driving bit
  clock, word select and data. The LPAIF MI2S receiver always shifts data on
  its own bit clock from the SoC's 19.2 MHz reference. The two bit clocks are
  ~27 ppm apart, so their phase slides through one bit period every ~24 ms;
  once per cycle the word-select edge lands on LPASS's sampling edge and the
  first data bit of each word — the sign bit — is sampled at its transition
  for ~0.6 ms. The burst period drifts slowly with temperature, which is how
  it was recognised as a clock beat rather than a fixed block size.

Not fixable at the link: the chip sends no data as an I2S slave (pin function
7, even with LPASS clocks running before its I2S block is enabled); the AFE
I2S configuration has no bit-clock polarity or justification field; and the
chip's pad drive strength, the LPASS bit-clock rate and an AFE request for an
external bit clock all leave the glitch rate unchanged. Flipping the sign bit
back in software sounded worse — the corrupted samples are not cleanly
recoverable — so they are treated as lost.

**`fmrepair/`** is the fix in use: an ALSA external PCM plugin that exposes
the raw MultiMedia2 capture as `sirius_fm` (`sirius_fm:CARD=0,DEV=3`). It
clusters outlier samples into bursts, tracks their ~1152-frame period, and
replaces each burst window with linear interpolation between the good frames
either side, on both channels; every other frame passes through unchanged
(3–4 % of frames are touched; 4 ms delay). Live result: 0–1 glitches per
second against ~900 on the raw device. `fmrepair/install.sh` builds it
(alsa-lib-dev, `-DPIC`) and installs the plugin and `60-sirius-fm.conf`; the
[Robwatts FM Radio](https://github.com/RobLymm/robwatts-fm-radio) app and `tools/fm-play.sh` read from `sirius_fm` when it is present.

The clean long-term route is the one Sony shipped: the tuner's own DAC
(AUD_CTL0 bit 4) into the WCD9320 codec's analogue inputs (ADC5/ADC6 →
SLIM_0_TX), which has no digital link to corrupt. That waits on the WCD9320 +
SLIMbus bring-up headphones need anyway. FM over the chip's SCO/PCM block is
voice-band and not worth pursuing.

`0014-ASoC-qdsp6-add-the-internal-FM-capture-port.patch` was the **first**
attempt — the LPASS "internal FM" port (INT_FM_TX, AFE 0x3005) that Sony's
stock mixer paths name. On mainline it captured only zeros: the tuner's I2S
does not reach that port, it reaches the secondary MI2S pads. 0014 is kept
only as documentation of the port; it is not used and not needed.

**`0001-ASoC-qdsp6-q6afe-send-both-LPAIF-clocks-in-one-comman.patch`** is the
q6afe fix, generated against 6.16.12 and verified to apply cleanly to a
pristine tree. checkpatch passes with one deliberate exception: there is no
`Signed-off-by` line, because that is the author's certification to add and
not something a tool should write.

It remembers each LPAIF clock as it is set and sends both together once both
are known. Note the one design decision in it: the both-valid mode is used
only when both clocks are non-zero, so platforms that set a single clock keep
exactly the behaviour they had. Sending both unconditionally would have been
a smaller patch and a behaviour change for every other qcom board.

This is a bug fix to an existing mainline driver, it is not Z2 specific, and
it should benefit any Qualcomm device driving MI2S where the DSP needs both
the bit clock and the oversampling clock. It is the smallest and most
independent thing in this repository to upstream, and the best first patch.

## Three things that cost time, recorded so they do not cost yours

**The CPU DAI must be told it is the clock provider.** q6afe reads the
provider flags from the point of view of the DAI it is given, not from the
codec's. Setting the conventional codec-centric `SND_SOC_DAIFMT_CBC_CFC` on
the CPU DAI makes q6afe set `ws_src = EXTERNAL`, after which the ADSP waits
forever for a word-select clock that nothing generates. Every AFE command
still returns success, and the stream simply never advances. The CPU DAI needs
`SND_SOC_DAIFMT_BP_FP`.

**Both LPAIF clocks must be sent in a single command.** Enabling the bit clock
(1.536 MHz) and the oversampling clock (12.288 MHz) in two separate calls does
not work; the second supersedes the first. Hence the q6afe change above.

**The LPAIF mode-mux registers belong to AUXPCM, not MI2S.** Sony's downstream
card writes a quaternary mode-mux register from the AP, which is easy to copy
and wrong here. Writing it is unnecessary and gets in the way.

Also worth knowing: the amplifiers are at **7-bit 0x34 and 0x35**. Sony's
device tree lists 0x68 and 0x6A, which are the same addresses shifted for the
8-bit convention. Using those directly finds nothing on the bus.

## Building it out of tree

If you build the QDSP6 stack as modules against a kernel that was not
configured with them, drop the Kconfig `select` lines for `SND_SOC_TOPOLOGY`
and `SND_SOC_COMPRESS` and use `SND_COMPRESS_OFFLOAD` instead. `SND_SOC_TOPOLOGY`
adds a `struct snd_soc_dobj` member to four core ASoC structures, which
changes their layout and therefore their symbol CRCs. The modules will build
and then refuse to load with "disagrees about version of symbol" against a
kernel built without it.

The cleaner answer, if you are rebuilding the kernel anyway, is to enable
`SND_SOC_TOPOLOGY` and `SND_SOC_COMPRESS` properly and drop the Kconfig edits.

Config needed either way:

    CONFIG_QCOM_APR=m  QCOM_PDR_HELPERS=m  QCOM_PDR_MSG=m
    CONFIG_SND_SOC_QCOM=m  SND_SOC_QCOM_COMMON=m  SND_SOC_QDSP6=m
    CONFIG_SND_SOC_MSM8974=m  SND_SOC_TFA989X=m  SND_COMPRESS_OFFLOAD=m

It also sets `card->driver_name` to `msm8974`, which every other qcom machine
driver does and ours did not. Without it the card's driver name is derived
from the board model string, and the UCM lookup path moves with it.

## Still missing

**~~An ALSA UCM profile~~** — written, in `../../userspace/ucm2/`, not yet
verified on hardware. Without one, PipeWire falls back to a generic stereo
profile: sound comes out, but there is no "Speaker" port, and volume controls
do not behave the way a phone's should. This is userspace only, it needs no
kernel work, and `alsa-ucm-conf` accepts contributions on GitHub. It is
probably the highest value per hour of anything left in this directory.

**Headphones and microphones.** These are on a WCD9320 (Taiko) codec on
SLIMbus, whose master is inside the ADSP (an NGD satellite on the apps
side). The earpiece is not affected — it is a separate TFA9890 amplifier on
MI2S, the same as the loudspeakers, and already works.

- *SLIMbus.* Mainline's `qcom-ngd-ctrl` already handles NGD v1.5.0 (msm8996)
  and v2.1.0 (sdm845); the Fairphone 2 / Nexus 5 work declares the msm8974
  `slim@fe12f000` node as `compatible = "qcom,slim-ngd-v1.5.0"` with a
  `slimbam` BAM, reusing the v1.5.0 path — so this is device tree plus
  `CONFIG_SLIM_QCOM_NGD_CTRL`, not a controller rewrite. The Z2's device
  tree for this is `devicetree/qcom-msm8974pro-sony-xperia-sirius-codec.dts`.
- *Codec.* No mainline WCD9320/Taiko driver exists (v6.16 has wcd9335,
  wcd934x, wcd937x/938x/939x, msm8916-wcd, and the shared `wcd-mbhc-v2` /
  `wcd-clsh-v2`, but no 9320/9310/9330). z3ntu's driver from the
  `flto-msm8974-5.11` branch of `z3ntu/linux` has been forward-ported to 6.16
  and lives in `drivers/audio/wcd9320/`.
- *Master clock.* The codec needs 9.6 MHz on its MCLK pin. That clock is made
  by a divider inside the PM8941 and leaves the PMIC on PMIC GPIO 15,
  alternate function 1. Both halves are device tree, and the divider needs
  `drivers/clk/pmic-clkdiv/`. Leave either out and the codec still answers on
  SLIMbus, every register write lands, every widget reports itself powered,
  and nothing comes out of the jack.

Playback now runs end to end: the ADSP starts the SLIMbus port, the whole
path from `AIF1 PB` to `HPHL`/`HPHR` powers up, and the headphone amplifier
status registers respond to the signal while a tone plays. Microphones are
untested and have no device tree links, there is no jack detection, and there
is no UCM profile for the jack. See `drivers/audio/wcd9320/README.md`.
