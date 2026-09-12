# Sony Xperia Z2 (sirius) on a mainline Linux kernel

Drivers, device trees and extracted hardware data for the Sony Xperia Z2
(D6503, codename sirius, msm8974pro-AB), for anyone running a mainline-based
Linux kernel on one.

Nothing here is tied to a particular distribution. It was developed and tested
on postmarketOS because that is the easiest way to get a mainline kernel onto
this phone, but the drivers are ordinary kernel code and the device tree is an
ordinary device tree.

The Z2 is the only member of Sony's shinano family without a device tree in
mainline Linux. Its three siblings — Xperia Z3 (leo), Z3 Compact (aries) and
Z2 Tablet (castor) — are all supported, and share
`qcom-msm8974pro-sony-xperia-shinano-common.dtsi`. That common file has no
display, GPU, audio or sensor support, so several parts of this repository
apply to all four devices, not only the Z2.

## State of each subsystem

| Subsystem | State | Where |
|---|---|---|
| Display | Working. Six panel variants, selected at runtime | `drivers/panel/`, `panel-variants/` |
| Touch | Working. Maxim MAX1187x, out-of-tree driver, Sony's binding | `devicetree/` |
| Wi-Fi, Bluetooth | Working | in-tree drivers, device tree only |
| GPU | Working for the compositor. Adreno 330 via freedreno, needs a VRAM carveout | `docs/known-problems.md` |
| Audio, speakers | Working. QDSP6 to Quaternary MI2S to two TFA9890 amplifiers | `drivers/audio/` |
| Audio, headphones and microphones | Not working. Needs a WCD9320 codec driver and SLIMbus on msm8974 | `drivers/audio/README.md` |
| Battery percentage | Working, via VADC VBAT_SNS and an OCV table | `drivers/battery/` |
| Sensors | Working. Accelerometer, gyroscope, magnetometer, barometer, light and proximity | `upstream/` |
| Modem | Boots, then stalls during initialisation | `docs/modem.md` |
| Suspend and resume | Not working. Resume loses Wi-Fi and touch | `docs/known-problems.md` |
| NFC | Not tested. NXP PN547; mainline driver exists, node written | `docs/nfc.md` |
| Camera | Not attempted. ISP support exists for msm8974 elsewhere; the sensors need drivers | `docs/camera.md` |
| FM radio | Driver written, never run. Nothing else exists anywhere for this tuner | `drivers/fm/` |

Read `docs/known-problems.md` before relying on any of this. The two that
matter most: applications rendering on the GPU hang it and can eventually
deadlock the compositor, so GTK applications are run with the cairo renderer;
and the light sensor reports 0 lux although the device is present and its
registers read fine.

## Layout

    drivers/panel/      DRM panel driver, all six Z2 panel variants
    drivers/audio/      ASoC machine driver for the msm8974 sound card
    drivers/battery/    VADC scaling and OCV capacity estimation patches
    drivers/fm/         V4L2 radio driver for the WCNSS tuner, untested
    panel-variants/     the six panel configurations extracted from stock
    devicetree/         the board device tree the phone actually runs
    upstream/           a mainline-style device tree, for submission
    userspace/          fixes needed on 32-bit ARM that are not Z2 specific
    tools/              build a boot image, compile a device tree on the phone
    docs/               identification, extraction, prior art, and what each
                        unfinished part costs

## Two device trees, and why

`devicetree/qcom-msm8974pro-sony-xperia-sirius.dts` is what runs: 455 lines of
labelled source on top of mainline's `shinano-common.dtsi`, with every
peripheral the Z2 differs on described properly. It replaced a 3,423 line
decompiled tree, and converting it exposed three real defects the flattened
form had been hiding — an undeclared regulator the sensors depend on, the
Z3's charging limits, and two stale phandles. See `devicetree/README.md`.

`upstream/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` is the same device
expressed the way mainline wants it: include the shared dtsi, add only the
differences, use labels. It compiles against current mainline sources but has
never been booted, and it deliberately leaves out display, touch and audio
because those depend on drivers that are not upstream. Do not flash it
expecting a working phone.

## Provenance and licensing

Kernel code here is GPL-2.0, carries SPDX headers, and is intended for
eventual submission to the relevant mainline subsystem.

Panel initialisation sequences, timings and colour calibration tables were
obtained by decompiling the device tree in the stock Sony FOTA kernel on the
device. That is stated plainly rather than obscured; mainline panel drivers
are routinely written this way.

**No firmware binaries are included and none should be added.** The modem,
ADSP and Adreno firmware on this device is proprietary and not redistributable.
`docs/extracting-from-stock.md` explains how to take the files off your own
phone, which is the only supported route.

## Relationship to postmarketOS and to mainline

postmarketOS already has a `device-sony-sirius` package in pmaports which
depends on `linux-postmarketos-qcom-msm8974`, the kernel this work was done
against. That is the fastest route to a Z2 someone can actually install and
use.

Mainline is the slower route and the more durable one: code in mainline
reaches every distribution without anyone compiling a module. The Xperia Z3
was added to mainline in March 2024, ten years after the hardware shipped, and
a DRM panel driver for a 2015-era Sony panel was merged in June 2026. The age
of this hardware is not an obstacle to upstreaming it.

See `upstream/README.md` for what is ready to submit and in what order.
