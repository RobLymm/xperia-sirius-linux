# What is left, and what each piece actually costs

Six subsystems are unfinished. They are not comparable in size: one needs a
device tree node and a flash, three need diagnosis on hardware, and two need
kernel drivers that do not exist anywhere upstream. This is the evidence for
each, so nobody starts the largest one expecting the smallest.

Checked against mainline at the time of writing.

## Needs a device tree node and a test — NFC

The chip is an NXP PN547 at 0x28 on `blsp1_i2c6`. Mainline has the driver
(`drivers/nfc/nxp-nci/i2c.c`) and the binding names `nxp,pn547` explicitly.
The GPIOs are known from Sony's tree: interrupt on TLMM 24, firmware download
on TLMM 57, VEN on PM8941 MPP 2.

Node, caveats and test procedure in `nfc.md`. One unknown: VEN polarity, which
is a one-line change to try both ways.

## Needs diagnosis on hardware

### Modem: boots, then stalls

Detail in `modem.md`. The specific check not yet done, and the reason it is
worth doing first:

The reserved regions this device tree uses came from the Xperia Z3, while the
firmware is the Z2's own. The regions are:

    mpss  0x08000000   81.00 MB
    mba   0x0d100000    1.00 MB

If the Z2's `modem.mdt` plus its `modem.bNN` segments exceed 81 MB, the load
cannot succeed and a stall is exactly the symptom. One command answers it:

    ls -l /lib/firmware/postmarketos/modem.* | awk '{s+=$5} END {print s/1024/1024 " MB"}'

Comparing against Sony's own tree does not help here: downstream allocates
modem memory at runtime rather than declaring fixed regions, so there is
nothing to diff.

### Suspend and resume: comes back without Wi-Fi or touch

Currently disabled in logind rather than fixed. Two devices fail
independently, which suggests two separate causes rather than one:

- **Touch.** The MAX1187x driver is out of tree and was written against a
  downstream kernel. Whether it has working `suspend`/`resume` callbacks at
  all is the first thing to check — if it has none, the controller is never
  told to sleep and is never reinitialised, and that alone explains it.
- **Wi-Fi.** brcmfmac on SDIO. Whether the SDIO host keeps power across
  suspend, and whether the card is re-probed on resume, is the question. The
  usual failure is the card losing power with no re-initialisation path.

Neither has been investigated. Both are tractable on hardware and neither
requires writing a driver from scratch.

## Needs a driver that does not exist upstream

These are not afternoon tasks, and saying so is more useful than optimism.

### Headphones, earpiece and microphones

Two separate pieces of new kernel work:

1. **A WCD9320 (Taiko) codec driver.** Mainline has `wcd9335`, `wcd934x`,
   `wcd937x`, `wcd938x` and `wcd939x` — no `wcd9320`. The nearest model is
   wcd9335, which is the same family and also SLIMbus attached, and is several
   thousand lines.
2. **msm8974 support in the SLIMbus NGD controller.** Mainline's
   `drivers/slimbus/qcom-ngd-ctrl.c` matches `qcom,slim-ngd-v1.5.0` and
   `qcom,slim-ngd-v2.1.0` only. msm8974's controller is an earlier generation,
   and on this SoC the SLIMbus master lives inside the ADSP.

Sony's stock tree has the micbias and routing configuration, which removes
some of the guesswork but none of the driver writing.

### FM radio

`drivers/media/radio/Makefile` contains nothing for IRIS, WCNSS or Qualcomm.
The downstream `radio-iris` driver was never upstreamed, and the tuner sits
inside the WCN3680 alongside Wi-Fi and Bluetooth, reached through the pronto
subsystem. Sony's tree shows only the audio side of it —
`qcom,msm-dai-q6-int-fm-rx` and `-tx`, the ADSP routing that carries FM audio
once a tuner exists.

So this is a new driver against an undocumented interface, for a feature few
people will use. It is the lowest value per unit of effort of anything here.

### Camera

`drivers/media/platform/qcom/camss` supports eighteen SoCs. msm8974 is not one
of them, and the oldest supported is msm8916, a later ISP generation. Beyond
the ISP itself, the image sensors need drivers of their own.

Camera is consistently the hardest subsystem to bring up on mainline Qualcomm
platforms, and on a SoC that camss does not cover at all this is a project
rather than a task.

## Summary

| Item | Blocking need | Realistic |
|---|---|---|
| NFC | a DT node and a flash | yes, next session |
| Modem | one firmware-size check, then diagnosis | yes |
| Suspend | investigate two drivers | yes |
| Headphones and mics | WCD9320 driver + SLIMbus NGD for msm8974 | substantial project |
| FM radio | a driver from scratch, undocumented interface | substantial, low value |
| Camera | camss support for msm8974 + sensor drivers | large project |
