# Sony Xperia Z2 (sirius) on a mainline Linux kernel

Drivers, device trees and extracted hardware data for the Sony Xperia Z2
(D6503, codename sirius, msm8974pro-AB), for anyone running a mainline-based
Linux kernel on one.

**If you have a Z2 and want it running like this one, start with
[docs/from-stock-to-this.md](docs/from-stock-to-this.md).** It is the whole
route from the Android the phone came with: unlocking the bootloader, what to
hold and when to plug the cable in, where to get the unlock code, what to type
to flash, and how to take the firmware off your own phone. It assumes you have
nothing installed.

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

The first twenty-two rows are the columns of the postmarketOS device table, in
the same order, so this phone can be compared directly with the Xperia Z3 and
the others listed there.

| Feature | State | Detail |
|---|---|---|
| Device | Sony Xperia Z2 | Model tested: D6503 |
| Codename | sony-sirius | Sony's codename is sirius; `sony-sirius` is the postmarketOS device name |
| Architecture | armv7 | 32-bit ARM. Qualcomm MSM8974AB (Snapdragon 801) with four Krait cores |
| Release year | 2014 | Announced 24 February 2014 |
| USB networking | Working | SSH over USB at 172.16.42.1 |
| Flashing | Working with `fastboot flash boot` | Sony's S1 bootloader boots the boot partition directly. Build the image with `tools/`; see `devicetree/README.md` |
| Touch | Working | Maxim MAX1187x, out-of-tree driver, Sony's binding. Reports multitouch in evdev protocol A; see `docs/known-problems.md` |
| Screen | Working | Six panel variants, selected at runtime, with a generated driver for each. `drivers/panel/`, `panel-variants/` |
| Wi-Fi | Working | Broadcom brcmfmac over SDIO, in-tree driver |
| FDE | Not tested | The test install is unencrypted |
| Battery | Working | Percentage from VADC VBAT_SNS and an OCV table; charging limits are Sony's Z2 values. `drivers/battery/` |
| 3D | Partly working | Adreno 330 through freedreno runs the compositor. Applications rendering on the GPU hang it, so GTK applications use the cairo renderer. Needs a VRAM carveout. `docs/known-problems.md` |
| IMU | Working | Accelerometer, gyroscope, magnetometer and barometer. The light sensor binds but reads 0 lux; proximity responds but is uncalibrated |
| Audio | Mostly working | Speakers, earpiece, headphones and the handset microphone all work, and call audio works in the downlink direction only, and the radio app can switch between speaker, headphones and Bluetooth. Speakers are QDSP6 to Quaternary MI2S to two TFA9890 amplifiers; the earpiece is the top TFA9890. Headphones and microphones are a WCD9320 codec on SLIMbus with its 9.6 MHz master clock from the PM8941 divider on PMIC GPIO 15. Roughly every other capture returns silence, and the secondary microphone is not reading yet. `drivers/audio/wcd9320/`, `drivers/clk/pmic-clkdiv/` |
| Bluetooth | Working | Broadcom BCM4335C0 over UART, in-tree driver |
| Camera | Not working | msm8974 camera support exists out of tree for the Nexus 5; the two Sony sensors have no drivers. `docs/camera.md` |
| GPS | Working | The modem's GNSS engine, over QMI LOC on `/dev/wwan0qmi0`: a standalone session streams NMEA at 1 Hz (GGA, RMC, GSA, VTG, GSV) and tracks satellites. `modem/` |
| Mobile data | Not tested | The modem starts, reports its IMEI, registers on a network and appears in ModemManager, and calls work. Data has not been tried. `modem/`, `docs/modem.md` |
| SMS | Working | Sending and receiving both tested |
| Calls | Working except the microphone | A call can be placed and answered, the caller's voice comes out of the phone, and audio generated on the phone reaches the far end through the DSP's in-call playback, with the phone's own loudspeaker silent (`tools/call-say.sh`). What does not work is the microphone: nothing it picks up reaches the far end. Voice audio goes through the DSP, and mainline has no driver for its voice services, so one is ported and adapted here; it needs no calibration data, which was the surprise. The uplink is unsolved, and `drivers/audio/q6voice/README.md` records what has been eliminated. `drivers/audio/q6voice/` |
| USB-OTG | Not tested | The USB controller is in OTG mode and the PM8941 ID detection is present; host mode has not been tried |
| NFC | Not tested | NXP PN547. The mainline driver supports it and a device tree node is written but has not been flashed. `docs/nfc.md` |
| CPU frequency and voltage scaling | Not working, written but never booted | All four cores run at a fixed 960 MHz of the rated 2265.6 MHz, because nothing in mainline instantiates the Krait clock controller. A complete patch series is in `drivers/cpufreq/`: HFPLL data, a krait-cc fix, the device tree with an OPP table generated from Sony's factory data, and the supply. The four cores share one supply — ganged PM8841 phases reached over the L2 SAW, not per-core regulators — and a driver for it is written. Operating points above 960 MHz are present but disabled until the supply path is proven. It compiles; it has never been booted |
| Suspend and resume | Not working | Resume loses Wi-Fi and touch, so suspend is turned off. `docs/known-problems.md` |
| FM radio | Working | The tuner is inside the Broadcom Bluetooth chip, driven over HCI from userspace; audio arrives on the secondary MI2S port. The I2S link corrupts the sign bit of a burst of samples 41.6 times a second (chip and SoC bit clocks are independent); the `drivers/audio/fmrepair` ALSA plugin repairs it at the device layer. The app, [Robwatts FM Radio](https://github.com/RobLymm/robwatts-fm-radio), is published separately. `docs/fm-broadcom.md`, `drivers/audio/README.md` |

`docs/whats-left.md` is the gap between this and a phone someone could use as
their only phone, ordered by what blocks that. `docs/todo.md` is the same
work as an ordered list, with the capability checklist to run once it is
done.

Read `docs/known-problems.md` before relying on any of this. The two that
matter most: applications rendering on the GPU hang it and can eventually
deadlock the compositor, so GTK applications are run with the cairo renderer;
and the light sensor reports 0 lux although the device is present and its
registers read fine.

## Layout

    drivers/panel/      DRM panel driver, all six Z2 panel variants
    drivers/audio/      ASoC machine driver for the msm8974 sound card
    drivers/audio/fmrepair/  ALSA plugin: repairs the FM capture's periodic sign-bit bursts (device sirius_fm)
    drivers/battery/    VADC scaling and OCV capacity estimation patches
    drivers/cpufreq/    Krait clocks, OPP tables and the shared CPU supply
    drivers/fm/         V4L2 driver for WCNSS FM tuners (other msm8974 phones,
                        not the Z2)
    panel-variants/     the six panel configurations extracted from stock,
                        and a generated DRM driver for each
    devicetree/         the board device tree the phone actually runs
    upstream/           a mainline-style device tree, for submission
    userspace/          the ALSA UCM profile, and fixes for 32-bit ARM
                        that are not Z2 specific
    tools/call-say.sh   play audio into a voice call, through the DSP
                        rather than a microphone
    tools/              build the board device tree and a boot image, check that
                        two device trees describe the same hardware, drive the
                        Broadcom FM tuner
    tools/fm-diag/      how the FM "flicking" was found: capture analysers and a pad clock timer
    modem/              what Sony's modem firmware needs from the AP (TA services),
                        the ta-service fix, and a QMI service honeypot
    docs/               identification, extraction, prior art, and what each
                        unfinished part costs

## Two device trees, and why

`devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` is what runs: 529 lines of
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

There are two places this work can land, and they answer different questions.

**postmarketOS is how someone runs a Z2 today.** pmaports already has a
`device-sony-sirius` package depending on `linux-postmarketos-qcom-msm8974`,
the kernel this was built against, so anything that is not a kernel change
belongs there and can ship immediately: the `ta-service` fix that gets the
modem through its initialisation, and the `fmrepair` ALSA plugin. Both apply
to every Sony msm8974 phone, not only this one.

**Mainline is how the work outlives this repository.** Code in mainline
reaches every distribution without anyone compiling a module. Most of the
kernel work here is not Z2 specific either: the q6afe clock fix is a bug fix
affecting every Qualcomm QDSP6 board, and the sound card and WCD9320 codec
would give audio to the Nexus 5 and the Fairphone 2 as much as to this phone.

The age of the hardware is not an obstacle. The Xperia Z3 was added to
mainline in March 2024, ten years after it shipped, and a DRM panel driver for
a 2015 Sony panel was merged in June 2026.

`upstream/README.md` says what is ready to submit and in what order.
