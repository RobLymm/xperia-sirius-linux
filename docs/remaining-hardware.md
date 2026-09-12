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

Detail in `modem.md`. The check not yet done: the reserved regions came from
the Xperia Z3 while the firmware is the Z2's own, and `mpss` is 81 MB. If the
Z2's modem image exceeds that, a stall is the expected symptom.

    ls -l /lib/firmware/postmarketos/modem.* | awk '{s+=$5} END {print s/1024/1024 " MB"}'

Diffing Sony's tree does not help: downstream allocates modem memory at
runtime rather than declaring fixed regions.

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

## Camera — a variant to add, not an ISP driver

**Correction.** Previously described as needing camss support for a SoC it
does not cover, framed as a large project. The first half is true, the framing
was too pessimistic.

msm8974's CAMSS, from Sony's tree:

| block | count | addresses | size | IRQs | compatible |
|---|---|---|---|---|---|
| CSIPHY | 3 | fda0ac00, fda0b000, fda0b400 | 0x200 | 78, 79, 80 | qcom,csiphy |
| CSID | 4 | fda08000, fda08400, fda08800, fda08c00 | 0x100 | 51-54 | qcom,csid |
| ISPIF | 1 | fda0a000 (+ csi_clk_mux fda00020) | 0x500 | 55 | qcom,ispif-v3.0 |
| VFE | 2 | fda10000, fda14000 (+ vbif fda40000, tcsr fd4a8000) | 0x1000 | 57, 58 | **qcom,vfe40** |
| CCI | 1 | fda0c000 | 0x1000 | 50 | qcom,cci |

Two things follow. The topology — 3 CSIPHY, 4 CSID, 1 ISPIF, 2 VFE — is the
same shape as msm8996, which camss supports, so nothing structural is missing;
camss already handles multiple VFEs and CSIDs. And the VFE is `qcom,vfe40`,
the same generation camss implements as `camss-vfe-4-1.c` for msm8916, rather
than something camss has never seen.

So the realistic shape is: a `msm8974_resources` table in `camss.c` modelled
on the msm8996 one, using the addresses above; whatever deltas exist between
msm8974's VFE40 and msm8916's, which need measuring rather than assuming; and
the ISPIF, which msm8974 reports as v3.0.

That is still a substantial piece of work and it is only the ISP. The image
sensors need drivers of their own, and Sony hides the parts behind their own
abstraction — the tree says `sony_camera_0` and `sony_camera_1` rather than
naming the sensors, so identifying them is a prerequisite.

## FM radio — genuinely from scratch

No correction to make here; this one is as bad as it looked.

`drivers/media/radio/Makefile` contains nothing for IRIS, WCNSS or Qualcomm.
No out-of-tree port exists in the msm8974-mainline tree either. The tuner sits
inside the WCN3680 alongside Wi-Fi and Bluetooth, reached through the pronto
subsystem, and the downstream `radio-iris` driver was never upstreamed.

Sony's tree shows only the audio side: `qcom,msm-dai-q6-int-fm-rx` and `-tx`,
the ADSP routing that carries FM audio once a tuner exists. That routing is
useful — it means the audio path is already understood — but it is the easy
half.

Writing this means a V4L2 radio driver against an undocumented interface to a
coprocessor, with downstream `radio-iris` as the only reference.

## Order worth doing them in

1. **NFC** — a node and a flash.
2. **Modem** — one command, then diagnosis.
3. **Suspend** — investigate two drivers.
4. **Headphones** — forward-port an existing 7,000-line driver.
5. **Camera** — add a camss variant, then identify and drive the sensors.
6. **FM** — a new driver against an undocumented interface.
