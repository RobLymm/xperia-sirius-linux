# Sony Xperia Z2 (sirius) on a mainline Linux kernel

Drivers, device trees and extracted hardware data for the Sony Xperia Z2
(D6503, codename sirius, msm8974pro-AB), for anyone running a mainline-based
Linux kernel on one.

Calls, SMS, mobile data, Wi-Fi, Bluetooth, GNSS, FM radio, the display,
touch, the sensors and the full 300 MHz to 2265.6 MHz range of all four CPU
cores work. Audio works for the loudspeakers, earpiece, headphones and
recording, and a call carries the caller's voice and anything the phone
plays, but not yet what its microphone hears. The front camera takes pictures;
the rear one has no driver yet. Suspend is off because resume loses Wi-Fi and
touch.

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

**Working on this port?** Read [docs/where-work-goes.md](docs/where-work-goes.md) first. It says which files are canonical, which are stale or deprecated, and how to check what the phone is actually running.

## State of each subsystem

Last verified against the device on 2026-09-19.

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
| 3D | Partly working | Adreno 330 through freedreno runs the compositor, on mesa 26.2.2. Applications rendering on the GPU hang it, so GTK applications use the cairo renderer, set in `/etc/sirius-renderer`. Needs a VRAM carveout. `docs/known-problems.md` |
| IMU | Working | Accelerometer, gyroscope, magnetometer and barometer all read. Light and proximity are an APDS-9930 and both work. Part by part in the table below |
| Audio | Mostly working | Speakers, earpiece, headphones and the handset microphone all work, and call audio works in the downlink direction only, and the radio app can switch between speaker, headphones and Bluetooth. Speakers are QDSP6 to Quaternary MI2S to two TFA9890 amplifiers; the earpiece is the top TFA9890. Headphones and microphones are a WCD9320 codec on SLIMbus with its 9.6 MHz master clock from the PM8941 divider on PMIC GPIO 15. Roughly every other capture returns silence, and the secondary microphone is not reading yet. `drivers/audio/wcd9320/`, `drivers/clk/pmic-clkdiv/` |
| Bluetooth | Working | Broadcom BCM4335C0 over UART, in-tree driver |
| Camera | **The front camera works** | The IMX132 captures 1976x1144 Bayer through the msm8974 CAMSS driver, and libcamera's software ISP turns it into colour: `cam -l` lists it and `cam --capture` produces correct images. What is left is auto-exposure and white balance, Phosh's camera app, which has not been tried yet, and the whole of the rear IMX200. `docs/camera.md`, `drivers/camera/` |
| GPS | Working | The modem's GNSS engine, over QMI LOC on `/dev/wwan0qmi0`: a standalone session streams NMEA at 1 Hz (GGA, RMC, GSA, VTG, GSV) and tracks satellites. ModemManager can enable it directly with `--location-enable-gps-nmea`. A fix needs sky. `modem/` |
| Mobile data | Working | A bearer comes up through NetworkManager and `wwan0` gets an address; verified by pinging and fetching a page bound to that interface rather than trusting the default route. On GPRS it is slow, and the modem has not yet been persuaded to carry data on anything faster. `modem/`, `docs/modem.md` |
| SMS | Working | Sending and receiving both tested |
| Calls | Working except the microphone | A call can be placed and answered, the caller's voice comes out of the phone, and audio generated on the phone reaches the far end through the DSP's in-call playback, with the phone's own loudspeaker silent (`tools/call-say.sh`). What does not work is the microphone: nothing it picks up reaches the far end. Voice audio goes through the DSP, and mainline has no driver for its voice services, so one is ported and adapted here; it needs no calibration data, which was the surprise. The uplink is unsolved, and `drivers/audio/q6voice/README.md` records what has been eliminated. `drivers/audio/q6voice/` |
| USB-OTG | Not tested | The controller sits in gadget mode and is what USB networking runs on. No USB role-switch is exposed in sysfs, so host mode would need checking rather than assuming |
| NFC | Not working | NXP PN547. The mainline driver supports it and a device tree node is written, but it has never been flashed: there is no NFC node in the booted device tree and no driver loaded. `docs/nfc.md` |
| CPU frequency and voltage scaling | Working | The full rated range, 300 MHz to 2265.6 MHz, on `cpufreq-dt` with the schedutil governor. All four cores reach 2265.6 MHz under load and drop to 300 MHz idle; a timed workload runs 5.5 times faster at the top than at the bottom, so the rate is real and not just reported. Nothing in mainline instantiates the Krait clock controller, so this needs the patches in `drivers/cpufreq/`: HFPLL data, a krait-cc fix, the device tree and OPP table, and the CPU supply, which is ganged PM8841 phases reached over the L2 SAW rather than per-core regulators |
| Suspend and resume | Partly working | Suspend and resume themselves work; they had been masked, not broken. Wi-Fi comes back through a sleep hook that rebinds the SDIO host, and the touch driver can now be re-probed because its teardown was never wired to `remove`. The touchscreen's own resume path is still unsolved, and nothing but the power key can wake the phone — not Wi-Fi, not mobile data, not an incoming call. `docs/known-problems.md`, `userspace/systemd/` |
| FM radio | Working | The tuner is inside the Broadcom Bluetooth chip, driven over HCI from userspace; audio arrives on the secondary MI2S port. The I2S link corrupts the sign bit of a burst of samples 41.6 times a second (chip and SoC bit clocks are independent); the `drivers/audio/fmrepair` ALSA plugin repairs it at the device layer. The app, [Robwatts FM Radio](https://github.com/RobLymm/robwatts-fm-radio), is published separately. `docs/fm-broadcom.md`, `drivers/audio/README.md` |

`docs/whats-left.md` is the gap between this and a phone someone could use as
their only phone, ordered by what blocks that. `docs/todo.md` is the same
work as an ordered list, with the capability checklist to run once it is
done.

## Individual components

The table above follows the postmarketOS device table, which groups things
like "IMU" together. This is the part-by-part view, so a component that works
is not hidden inside a row about something else. Readings below were taken
from the device on 2026-09-19.

| Component | Part | State | Detail |
|---|---|---|---|
| Accelerometer | Bosch BMA255 | Working | `iio:device2`, reads live |
| Gyroscope | Bosch BMG160 | Working | `iio:device4`, reads live |
| Magnetometer | AKM AK8975 | Working | `iio:device3`, reads live. Not calibrated as a compass |
| Barometer | Bosch BMP280 | Working | `iio:device0`, 100.4 kPa |
| Ambient light | Avago APDS-9930 | Working | `iio:device1`, tracks the room. Nothing drives the backlight from it yet |
| Proximity | Avago APDS-9930 | Working | Needed one `PROXIMITY_NEAR_LEVEL` udev property, not calibration; `userspace/udev/`. The near threshold has not been checked against a real face |
| Touchscreen | Maxim MAX1187x | Working | Out-of-tree driver. Does not survive suspend without the resume hook |
| Power key | PM8941 | Working | Also the only thing that can wake the phone from suspend |
| Volume keys | gpio-keys | Working | |
| Vibrator | PM8941 | Present, untested | `pm8xxx_vib_ffmemless` input device exists |
| Notification LED | Qualcomm LPG | Present, untested | `rgb:status` |
| Backlight | | Working | 4096 levels |
| Battery gauge | PM8941 VADC | Working | Percentage from VBAT_SNS and an OCV table |
| Charger | PM8941 SMBB | Working | USB and DC inputs both present |
| Rear camera sensor | **Sony IMX200** | Detected, no driver | Answers on CCI, identifies itself. No driver exists for this part anywhere |
| Front camera sensor | **Sony IMX132** | Working | `drivers/camera/imx132.c`. Captures 1976x1144 SBGGR10, and libcamera's software ISP turns it into colour. No auto-exposure or white balance yet, and libcamera needs the CPU debayer because the GPU one fails on this Adreno |
| Autofocus actuator | Rohm BU64296G | Detected, no driver | At 0x0c on the rear camera bus |
| Camera module EEPROMs | | Readable | Both, at 0x50. Carry the module and sensor part numbers as ASCII |
| Camera control bus | Qualcomm CCI | Working | Both masters enumerate; mainline `i2c-qcom-cci` binds |
| Camera ISP | Qualcomm CAMSS | Working | msm8974 support re-expressed for 6.16: 3 CSIPHY, 4 CSID, 4 ISPIF lines, 2 VFE, six video nodes. Probes clean and captures frames from the CSID test pattern generator. No sensor attached yet. `drivers/camera/` |
| V4L2 media core | | Working | `mc`, `videodev` and the `videobuf2` set built out of tree and loaded. No `/dev/video*` yet: nothing registers one |

Read `docs/known-problems.md` before relying on any of this. The one that
matters most: applications rendering on the GPU hang it and can eventually
deadlock the compositor, so GTK applications are run with the cairo
renderer.

## Layout

    drivers/panel/      display: DRM panel driver, all six Z2 panel variants
    drivers/audio/      speakers and earpiece: ASoC machine driver for the
                        msm8974 sound card
    drivers/audio/wcd9320/   headphones and microphones: the WCD9320 codec on
                        SLIMbus, forward-ported to 6.16
    drivers/audio/q6voice/   call audio: the DSP's voice services, which
                        mainline has no driver for
    drivers/audio/fmrepair/  FM radio: ALSA plugin repairing the capture's
                        periodic sign-bit bursts (device sirius_fm)
    drivers/clk/pmic-clkdiv/ the PM8941 divider that makes the codec's
                        9.6 MHz master clock; headphones need it
    drivers/battery/    battery: VADC scaling and OCV capacity estimation
    drivers/cpufreq/    CPU scaling: Krait clocks, OPP tables and the shared
                        supply, 300 MHz to 2265.6 MHz
    drivers/fm/         V4L2 driver for WCNSS FM tuners (other msm8974 phones,
                        not the Z2)
    drivers/touch/      touchscreen: the MAX1187x driver the phone runs,
                        with two fixes; not yet in the kernel package
    drivers/camera/     camera ISP: msm8974 support for mainline's CAMSS
                        driver, and its device tree node. Captures frames
                        from the CSID test pattern generator. Also the
                        IMX132 front sensor driver, written but not yet
                        bound to the sensor
    panel-variants/     the six panel configurations extracted from stock,
                        and a generated DRM driver for each
    devicetree/         the board device tree the phone actually runs
    upstream/           a mainline-style device tree, for submission
    userspace/          the ALSA UCM profile, the proximity udev rule, the
                        resume hook, and fixes for 32-bit ARM that are not
                        Z2 specific
    tools/call-say.sh   play audio into a voice call, through the DSP
                        rather than a microphone
    tools/              build the board device tree and a boot image, check that
                        two device trees describe the same hardware, drive the
                        Broadcom FM tuner
    tools/sensor-dump.sh read a camera sensor's registers over CCI
    tools/kbuild-mod.sh build a kernel directory as an out-of-tree module
                        against the running kernel, with harvest-symvers.py
                        and fill-symvers.py to give it the symbol versions
                        CONFIG_MODVERSIONS wants
    tools/fm-diag/      how the FM "flicking" was found: capture analysers and a pad clock timer
    modem/              what Sony's modem firmware needs from the AP (TA services),
                        the ta-service fix, and a QMI service honeypot
    docs/               identification, extraction, prior art, and what each
                        unfinished part costs. Start with where-work-goes.md,
                        which says which document holds which kind of fact

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
