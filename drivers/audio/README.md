# Audio on the Xperia Z2

Speakers work. Headphones, earpiece and microphones do not, and need a codec
driver that does not exist yet.

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

**Headphones, earpiece and microphones.** These are on a WCD9320 (Taiko)
codec, which sits on SLIMbus. On msm8974 the SLIMbus master is inside the
ADSP, so two things are needed: msm8974 support in mainline's `qcom-ngd-ctrl`
(which currently handles v1.5.0 and v2.1.0 only), and a WCD9320 codec driver.
The closest model for the latter is mainline's `wcd9335`, which is the same
family and also SLIMbus. Sony's stock device tree has the micbias and routing
configuration. This is a substantial piece of work, not an afternoon.
