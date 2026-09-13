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

The first eighteen rows are the columns of the postmarketOS device table, in
the same order, so this phone can be compared directly with the Xperia Z3 and
the others listed there.

| Feature | State | Detail |
|---|---|---|
| USB networking | Working | SSH over USB at 172.16.42.1 |
| Flashing | Working with `fastboot flash boot` | Sony's S1 bootloader boots the boot partition directly. Build the image with `tools/`; see `devicetree/README.md` |
| Touch | Working | Maxim MAX1187x, out-of-tree driver, Sony's binding. Reports multitouch in evdev protocol A; see `docs/known-problems.md` |
| Screen | Working | Six panel variants, selected at runtime, with a generated driver for each. `drivers/panel/`, `panel-variants/` |
| Wi-Fi | Working | Broadcom brcmfmac over SDIO, in-tree driver |
| FDE | Not tested | The test install is unencrypted |
| Battery | Working | Percentage from VADC VBAT_SNS and an OCV table; charging limits are Sony's Z2 values. `drivers/battery/` |
| 3D | Partly working | Adreno 330 through freedreno runs the compositor. Applications rendering on the GPU hang it, so GTK applications use the cairo renderer. Needs a VRAM carveout. `docs/known-problems.md` |
| IMU | Working | Accelerometer, gyroscope, magnetometer and barometer. The light sensor binds but reads 0 lux; proximity responds but is uncalibrated |
| Audio | Partly working | Both loudspeakers work: QDSP6 to Quaternary MI2S to two TFA9890 amplifiers. Headphones, earpiece and microphones need the WCD9320 codec, for which a 4.18-era out-of-tree driver exists to port. `drivers/audio/`, `docs/remaining-hardware.md` |
| Bluetooth | Working | Broadcom BCM4335C0 over UART, in-tree driver |
| Camera | Not working | msm8974 camera support exists out of tree for the Nexus 5; the two Sony sensors have no drivers. `docs/camera.md` |
| GPS | Not working | Depends on the modem |
| Mobile data | Not working | The modem loads its firmware and then stalls during initialisation. `docs/modem.md` |
| SMS | Not working | Depends on the modem |
| Calls | Not working | Depends on the modem |
| USB-OTG | Not tested | The USB controller is in OTG mode and the PM8941 ID detection is present; host mode has not been tried |
| NFC | Not tested | NXP PN547. The mainline driver supports it and a device tree node is written but has not been flashed. `docs/nfc.md` |
| CPU frequency and voltage scaling | Not working, in progress | All four cores run at a fixed 960 MHz of the rated 2265.6 MHz, because there is no cpufreq driver. Clock patches and an OPP table from Sony's factory data are prepared for 300–960 MHz at the present voltage. Frequencies above 960 MHz need higher CPU voltage, which needs a driver for the Krait per-core regulators on PM8841 that mainline does not have |
| Suspend and resume | Not working | Resume loses Wi-Fi and touch, so suspend is turned off. `docs/known-problems.md` |
| FM radio | Partly working | The tuner is inside the Broadcom Bluetooth chip; it powers on and tunes from userspace. Reception and audio routing are not yet tested. `docs/fm-broadcom.md` |

Read `docs/known-problems.md` before relying on any of this. The two that
matter most: applications rendering on the GPU hang it and can eventually
deadlock the compositor, so GTK applications are run with the cairo renderer;
and the light sensor reports 0 lux although the device is present and its
registers read fine.

## Layout

    drivers/panel/      DRM panel driver, all six Z2 panel variants
    drivers/audio/      ASoC machine driver for the msm8974 sound card
    drivers/battery/    VADC scaling and OCV capacity estimation patches
    drivers/fm/         V4L2 driver for WCNSS FM tuners (other msm8974 phones,
                        not the Z2)
    panel-variants/     the six panel configurations extracted from stock,
                        and a generated DRM driver for each
    devicetree/         the board device tree the phone actually runs
    upstream/           a mainline-style device tree, for submission
    userspace/          the ALSA UCM profile, and fixes for 32-bit ARM
                        that are not Z2 specific
    tools/              build a boot image, compile a device tree on the phone,
                        drive the Broadcom FM tuner
    docs/               identification, extraction, prior art, and what each
                        unfinished part costs

## Two device trees, and why

`devicetree/qcom-msm8974pro-sony-xperia-sirius.dts` is what runs: 529 lines of
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
