# What is left, and what each piece actually costs

Six subsystems are unfinished. This is the evidence for each, so nobody starts
one with the wrong idea of its size.

An earlier version of this file was wrong about two of them. Both corrections
are in the relevant sections below, because the mistake is instructive: the
absence of something in mainline does not mean nobody has written it.

## Needs a device tree node and a test — NFC

The chip is an NXP PN547 at 0x28 on `blsp1_i2c6`. Mainline has the driver
(`drivers/nfc/nxp-nci/i2c.c`) and the binding names `nxp,pn547` explicitly.
GPIOs from Sony's tree: interrupt TLMM 24, firmware download TLMM 57, VEN on
PM8941 MPP 2.

Node, caveats and test procedure in `nfc.md`. One unknown: VEN polarity,
which is a one-line change to try both ways.

## Needs diagnosis on hardware

### Modem: boots, then stalls

Firmware is finished and packaged, and five hypotheses have been ruled out:
region size, missing segments, load addresses, rmtfs and the userspace stack.
The modem brings up its SMD transport and stops before registering services.
Next candidates are lk2nd and the Z2's own ADSP firmware. Detail in
`modem.md`.

### Suspend and resume: comes back without Wi-Fi or touch

Two devices fail independently, suggesting two causes. The MAX1187x touch
driver is out of tree and written against a downstream kernel — whether it has
`suspend`/`resume` callbacks at all is the first thing to check. Separately,
whether the brcmfmac SDIO card keeps power across suspend and is re-probed on
resume. Neither has been investigated; neither needs a new driver.

## Headphones, earpiece and microphones — a port, not a rewrite

**Correction.** This was previously described as needing two drivers written
from scratch. That was wrong on both counts.

**SLIMbus needs nothing.** The NGD controller driver in mainline was
originally developed and tested on msm8974 with the wcd9320. msm8974 works
with the existing `qcom,slim-ngd-v1.5.0` compatible — the
`msm8974-mainline/linux` tree uses exactly that. The whole of the SoC-specific
data in that driver is `{ .offset = 0x1000, .size = 0x1000 }`, shared by both
existing entries.

**A WCD9320 driver exists**, out of tree, in `msm8974-mainline/linux` on
branch `old-4.18.0/qcom-audio-wip`:

    sound/soc/codecs/wcd9320.c            3376 lines
    sound/soc/codecs/wcd9320-registers.h  1677 lines
    sound/soc/codecs/wcd9320-regmap.c     1445 lines
    sound/soc/codecs/wcd9320-slim.c        384 lines
    sound/soc/codecs/wcd9320.h             194 lines

So the work is a forward-port from 4.18 to current, not authorship. The main
obstacle is ASoC API churn: the `snd_soc_codec` to `snd_soc_component`
conversion landed immediately after 4.18, so every callback signature in that
driver is from the older API. That is mechanical but extensive.

After the port: a DT node for the Taiko on the SLIMbus controller at
`fe12f000` (Sony's tree has it as `qcom,taiko-slim-pgd` with the elemental
address `00 00 a0 00 17 02`), then DAI links added to the machine driver in
`../drivers/audio/`.

Sony's stock tree supplies the micbias and routing configuration.

## Camera — the ISP is nearly solved, the sensors are not

**Correction, twice over.** First described as needing camss support for a SoC
it does not cover. Then revised to "add a variant". Both were wrong in the
same way: nobody looked for existing work.

msm8974 camera exists, on the Nexus 5, in `z3ntu/linux` branch
`flto-msm8974-5.17-camera`. It binds to `qcom,msm8916-camss` — no new variant
was written — plus a ~150 line camss patch, a CCI hack and a hand-written
sensor driver. The addresses in their device tree node are identical to the
ones derived independently from Sony's tree here.

What remains for this phone is the sensors: Sony hides the parts behind
`sony_camera_0` and `sony_camera_1`, and mainline has no driver for a 2014
Sony flagship sensor. Full detail, the hardware table and the device tree node
in `camera.md`.

## FM radio — written, compiles, cannot load yet

Nothing existed for this tuner anywhere, so `../drivers/fm/radio-wcnss-fm.c`
was written: a sibling of `btqcomsmd` on the WCNSS `APPS_FM` channel, protocol
from downstream `radio-iris`. It compiles against 6.16.12 with no warnings at
`W=1`.

It cannot load on the current kernel, which is built without
`CONFIG_MEDIA_SUPPORT`; everything else it needs is present. A kernel rebuild
with the five config lines in `../drivers/fm/README.md` is the next step,
followed by the checks listed there. The headphone lead is the aerial, and
audio needs the ADSP FM routing wired into the sound card separately.

## Order worth doing them in

1. **NFC** — a node and a flash.
2. **Modem** — try lk2nd, then the Z2's own ADSP firmware.
3. **Suspend** — investigate two drivers.
4. **Headphones** — forward-port an existing 7,000-line driver.
5. **Camera** — forward-port existing msm8974 camss and CCI work, then
   identify and drive the two sensors.
6. **FM** — rebuild the kernel with media support, load, and work down
   the list in `../drivers/fm/README.md`.
