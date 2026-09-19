# What is left

Last verified against the device on 2026-09-19.

The gap between this phone and one someone could use as their only phone,
ordered by whether it blocks that.

Effort figures are rough and mean developer time. Where something needs a
driver that does not exist anywhere, that is said.

**Re-read the device before trusting this file.** Several entries here were
wrong for weeks, in both directions: CPU frequency scaling was recorded as
broken and needing an unwritten driver while the phone was already running
the full rated range, and the light sensor was recorded as reading 0 lux
while it was reporting the room correctly. A state table is a claim about a
device, and claims go stale. `docs/todo.md` has the checks.

Several entries below say what Sony's published kernel gives. Sony ran an Open
Devices programme and published the source this phone shipped with, which is
the authority on values and sequences for this hardware; `prior-art.md` says
how to read it and `sony-source-audit.md` holds the findings.

## Blocking: it cannot be used as a phone yet

**Two-way calls.** A call connects, the caller is heard, and audio generated
on the phone reaches the far end through the DSP's in-call playback. The
microphone does not: nothing it picks up is transmitted. The whole
application-processor side has been eliminated by measurement — the codec's
capture chain powers up and the AFE port is configured identically to an
ordinary recording that captures real audio at the same moment — so the fault
is inside the DSP's voice processor. `../drivers/audio/q6voice/README.md`
lists what has been ruled out and what to try next.

**Sony's source changed the order of what to try, on 2026-09-19.** Their driver
sends three commands between creating the voice session and enabling it —
`REGISTER_DEVICE_CONFIG`, `REGISTER_CALIBRATION_DATA_V2` and
`REGISTER_VOL_CALIBRATION_DATA` — and this port sends none of them. All three
bail out early when the calibration memory is unmapped, and **the caller
ignores their return values**, so a call still connects and still enables with
no calibration at all. That is our exact situation, and it means a call that
carries downlink and transmits nothing is not evidence that calibration is
unnecessary — it is what Sony's own code does without it.

The transmit topology, meanwhile, already matches Sony's no-calibration
fallback (`TX_SM_ECNS`), and Sony only selects the other topologies when the
calibration database names them. So the four untried topologies are worth an
hour because it is an hour, not because they are likely. The calibration path
is the main line: map memory with `VSS_IMEMORY_CMD_MAP_PHYSICAL`, then send
the three registrations in Sony's order. Days, and now with a worked example
to follow rather than a guess.

**Suspend and resume.** Does not work: resume loses Wi-Fi and the
touchscreen, so suspend is disabled. Without it the battery lasts hours
rather than days. The kernel offers s2idle and the wake path works — the
phone resumes on the power key — so this is driver resume paths rather than
anything fundamental. On the evidence this is now the largest single obstacle
to daily use. Days.

**Touchscreen reliability.** The controller can stop responding entirely at
the greeter, with no unlock gesture possible; a reboot recovers it. A phone
that cannot be unlocked is unusable regardless of what else works.

**There are two separate faults here, and only one of them is the
touchscreen.** `known-problems.md` records that on a phone left running, the
MSM GEM shrinker can crash inside the GEM locks and leave the compositor in
uninterruptible sleep — drawing nothing and reading no input, which looks
exactly like a dead touchscreen and is not one.

The touchscreen's own fault is the resume path, and its cause was found on
2026-09-19 by diffing this driver against Sony's published original, which is
the same file. The reset GPIO is parsed from the wrong element of the device
tree property, so it is never claimed and the controller can never be
power-on-reset. Since `enable_resume_por` is set, that is the whole of what
resume does. The device confirmed it at every boot: `GPIO request failed for
gpio reset (-2)`.

**Fixed the same day.** With the index corrected the hardware reset works —
`hw reset occured` where it used to report `irq reset timeout`, with the chip
raising its reset interrupt. What is not yet re-tested is a real suspend and
resume, which needs someone at the phone because waking it needs the power key.
Whether the greeter symptom was ever this fault, the shrinker crash, or both at
different times is not settled. `known-problems.md` and
`sony-source-audit.md` have the before and after.

## Blocking for some people, not for all

**Camera.** Phosh's camera app takes a photo with the front camera. The
IMX132 captures 1976x1144 Bayer through the msm8974 CAMSS driver, libcamera's
software ISP turns it into colour, and Snapshot writes a JPEG. Five of the six
stages are done — media core, control bus, ISP, front sensor and userspace.

What is left is the **rear IMX200**, and it is harder than the front turned
out to be. The front was unlocked by Intel's old atomisp driver, which
published the vendor MIPI registers and D-PHY timings that are zero at reset
and exist in no other source. Nothing equivalent has been found for the
IMX200, and those registers cannot be read out of a sensor that is not already
streaming. Add to that the autofocus actuator. Weeks, unless a table turns up.

**The flash does not wait for any of that.** Sony's device tree has the LED as
`somc,leds@d300`, `compatible = "somc,pm8941-flash"`, with its supplies
(`pm8941_boost` for torch, `pm8941_chg_boost` for flash), a 13 mA clamp, a
3-step startup delay and `hw-strobe-config = <3>` on PM8941 GPIO 27 in
alternate function 1. None of that needs CAMSS, a sensor driver or a
`/dev/video*`: it is a `/sys/class/leds` entry, and it would also give the
phone a torch, which it does not have. Mainline appears to have no flash-LED
driver for this PMIC generation — `leds-qcom-flash` covers the later
PM8350-class parts — so this is a small new driver whose whole register and
supply plan is published. Days, and independent of everything else here.

Then **quality**, which is two separate bounded jobs. Colour is poor because
libcamera has no tuning file for this sensor and no entry for it in its sensor
properties database, so there is no white balance and no colour matrix; that
is worth writing and sending upstream. The frame rate is low because the
debayer runs on the CPU — libcamera's EGL debayer fails every frame on this
Adreno, probably the same fault that makes the GPU renderer unusable
elsewhere on this phone.

**NFC.** Not working, and not merely untested: there is no NFC node in the
booted device tree and no driver loaded. The mainline driver supports the
PN547 and a device tree node is written, so this is a flash away from being
testable. Hours, once something else needs flashing anyway.

Two corrections to that node came out of Sony's source before it was ever
flashed, both in `sony-source-audit.md`. Its `enable-gpios` is marked
`GPIO_ACTIVE_LOW`, copied from Sony's device tree — but Sony's driver never
applies that flag and drives the line **high** to enable the chip, so the node
as written would hold it in reset. And the node has nothing for PVDD, the
chip's supply on PM8941 GPIO 34, because the mainline binding has no property
for it; it needs a `regulator-fixed`. Fix both before flashing, or the test
will fail for a reason that has nothing to do with NFC.

**Automatic brightness.** The light sensor works and reports the room, and
proximity now reaches userspace. Whether anything drives the backlight from
the light level is a separate question and untested.

**Headphone jack detection, and the buttons on the cable.** Nothing notices a
jack being plugged in, so output does not switch by itself. Needs
`CONFIG_REGMAP_IRQ` and the jack and button handling. Hours to days, but it
needs a kernel rebuild.

Sony declares the socket as a `"5-pole-jack"` and their `wcd9320.c` carries
the whole MBHC block — insertion and removal detection, impedance measurement
and the three-button remote decode. The thresholds are the part that is
otherwise guesswork, and they are published, so this is a port rather than a
piece of research.

## Reliability faults, each with a known symptom

**Applications rendering on the GPU hang it.** The compositor runs on the
Adreno 330 through freedreno on mesa 26.2.2, but applications that render on
it hang the GPU and can eventually deadlock the compositor, so GTK
applications are run with the software renderer, set in `/etc/sirius-renderer`.
The cost is a slower interface everywhere. Days, in someone else's driver.

**The dialler crashes.** `gnome-calls` segfaults whenever its window is
destroyed. Diagnosed: a NULL toplevel dereferenced in GTK's
`gdk_wayland_toplevel_remove_from_session`, fixed upstream in GTK 4.22.5.
This phone has 4.22.4 and Alpine has not packaged the fix. Waiting on a
package, or a cross build of GTK.

**Capture returns exact zeros on some attempts.** Recording from the handset
microphone gives silence on every other attempt, deterministically, with
identical driver and SLIMbus logs on good and silent runs. Two explanations
have been tested and disproved; see `known-problems.md`. Unfixed.

**FM audio is repaired rather than fixed.** The I2S link from the Bluetooth
chip corrupts the sign bit of a burst of samples 41.6 times a second, because
the chip's bit clock and the SoC's run independently, and the `fmrepair` ALSA
plugin patches the samples up at the device layer. It works, and it is a
filter over a fault rather than the fault fixed. Sony publishes the vendor
Broadcom FM stack — `drivers/bluetooth/broadcom/v4l2_fm_driver/`, on a later
branch than the rest of this — which drives the same tuner over HCI the same
way. If it sets a clock role or a PCM configuration that the userspace
sequence here does not, the cause goes away and the plugin can be deleted.
Unread so far.

**The modem's registration module can wedge.** One cause is fixed — the
trim-area message the modem sends at the end of every answered call was going
unanswered, starving its storage task until the watchdog reloaded the
firmware. Whether it was the only cause is unknown.

**The secondary microphone reads nothing.** Needed for noise suppression and
for room pickup at any distance.

Checked against Sony's routing on 2026-09-19, and **the routing is not the
fault**: the live device tree on the phone has AMIC1 to the secondary
microphone, AMIC2 to the headset microphone and AMIC4 to the handset
microphone, each through the right bias, matching Sony exactly.

Four routes Sony has are missing here, and they were tested as an explanation
and are not one. `LDO_H`, the supply behind every microphone bias, comes up for
every capture on both microphones without Sony's route to `MCLK`. Measured:
AMIC1 returns peaks of 4 and 6 while its bias is powered and its route is
right, against 2083 rms from AMIC4 in the same room seconds earlier. **The
fault is below the routing** — the ADC, the decimator, or the microphone — and
Sony's source has nothing further to say about it. The two routes still missing
that are real features are the noise-cancelling pair, `AMIC3` to `ANCLeft
Headset Mic` and `MIC BIAS2 External` to `ANCRight Headset Mic`.
`sony-source-audit.md` has the measurement.

## Individual components that are not finished

The part-by-part state of everything is in the component table in
`../README.md`. These are the entries in it that are not "Working", with what
each would take.

**The camera parts are identified and driverless.** Rear **IMX200**, front
**IMX132**, autofocus a Rohm **BU64296G**, all three answering on the CCI
bus, none with a driver. The IMX200 has no driver anywhere; the IMX132 exists
only in the Intel-coupled `staging/atomisp`, which is not usable here. Sony's
published kernel gives both power sequences exactly but no register or mode
tables, so those have to come off the stock system partition. The actuator is
a simple I2C part and the smallest of the three.

**The battery reports no current, and charges with no protection.** Capacity
comes from a voltage reading and an OCV table, which works but is the crude
method: there is no current measurement and no coulomb counting.

Checked on the device on 2026-09-19, and this is smaller than it was recorded
as. `BACKLOG.md` has it stopping for want of the sense resistor value; the
value was never missing. Sony's 10 mΩ is already in the live device tree as
`qcom,external-resistor-micro-ohms = <10000>`, from mainline's own
`pm8941.dtsi`, `qcom_spmi_iadc` is loaded and bound, and the phone already
exposes `in_current0_raw` and `in_current1_raw` on `iio:device5`. What is
missing is only the plumbing from that channel into the battery power supply,
which has `capacity`, `voltage_now` and no `current_now`. No flash needed.

Separately, the charging limits here are Sony's four headline values and
nothing else. Sony's charger node also carries a thermal mitigation ladder
(`1600 1600 1100 900 700 500 300 200 100 0` mA), a warm-battery limit of
4200 mV at 900 mA, a cool-battery limit of 4350 mV at 900 mA, a 3200 mV weak
-battery threshold and a 512-minute charge timeout. This phone charges a
3200 mAh cell at 1600 mA whatever its temperature. How much of that mainline's
`qcom_smbb` implements needs checking; what it implements should be set.

**Video out over the micro-USB port is not built at all.** The Z2 does MHL,
and Sony's device tree identifies the part: a **Silicon Image SiI8620** at
`0x72`, with its interrupt, power, reset, two switch-select and firmware-wake
GPIOs all named. Mainline has a DRM bridge driver for that chip,
`sil-sii8620`, so this is device tree and bridge plumbing rather than a new
driver — but it still has to be attached to the display pipeline, and nothing
here has been near it. The chip identity was the thing blocking an estimate.

**The two noise-cancelling microphones are unused.** The Z2 has them, on AMIC2
and AMIC3. Nothing in this port acknowledges they exist. Not needed for a
usable phone; worth knowing the hardware is there.

**The speaker amplifiers run on mainline's minimal driver.** Sound comes out,
through `tfa989x`, which sets registers and no more: no DSP container, no
speaker model, no excursion protection, no calibration. Sony ships the
complete NXP stack for these parts — `sound/soc/codecs/tfa/`, with an
initialisation table written for the TFA9890 specifically. What that would buy
is loudness without risking the drivers, which is the usual reason a phone's
speakers sound better than the same parts on a generic driver.

**Two of the six panels have no runtime support.** The panel driver detects
the variant from `lcdid_adc` and handles four: both Renesas Sharp and AUO, the
Renesas JDI, and the Novatek JDI this unit has. A phone with Sharp-on-Novatek
(ADC 1236000-1395000) or AUO-on-Novatek (1420000-1594000) falls through to
`matches no known panel` and gets no display. Both were extracted from Sony's
tree long ago and exist as standalone drivers in `panel-variants/generated/`;
folding them into the runtime driver is transcription, not research, and
cannot be tested here without one of those panels.

**Proximity's near threshold is a guess.** It is set to 250 against a far
reading of 93-122, which is clear of the noise but has never been checked
against an actual face. One measurement settles it.

**Nothing drives the backlight from the light sensor.** The sensor works and
reports the room; no automatic brightness consumes it.

**The magnetometer is not calibrated as a compass.** It reads, but heading
needs calibration and a consumer.

**The vibrator and the notification LED are present and untested.** Both
appear — `pm8xxx_vib_ffmemless` and `rgb:status` — and neither has been made
to buzz or light up. Minutes of work each, but until someone does it they
are claims rather than capabilities.

Sony's values for both, when someone does: the vibrator runs at **2900 mV**
(`qcom,qpnp-vib-vtg-level-mV`), and the LED is three PWM channels at 12 mA and
a 1000 µs period with **different maxima per colour** — red on channel 6 to
200, green on channel 5 to 511, blue on channel 4 to 341, with `rgb_sync`
set. Those three maxima are this LED's white balance; equal values give a
colour cast.

**Nothing notices the SIM tray.** Sony has a `sim-detection` switch on
`msmgpio 9`, debounced 10 ms and marked as a wake source. Our `gpio-keys` node
has only the two camera stages. It is four lines of device tree, it gives hot
SIM-removal detection, and — since nothing but the power key can currently
wake this phone — it is one more GPIO worth testing as a wake source. One
complication: Sony reports it as `EV_SW` code 7, which is
`SW_JACK_PHYSICAL_INSERT`; the kernel has no switch code for a SIM tray, so
what to report needs deciding rather than copying.

## Untested, and cheap to find out

Full-disk encryption, and USB host mode — the controller sits in gadget mode,
which is what USB networking runs on, and no USB role switch is exposed in
sysfs, so host mode needs checking rather than assuming.

## Not a capability, but it decides whether any of this lasts

**Nothing happens when the phone gets hot except one trip point.** A CPU trip
at 80 °C is configured here, and Sony picked the same 80 °C, which is a useful
confirmation. What is missing is everything after the trip: Sony throttles to
**422.4 MHz** on cores 0 and 3, and takes cores 1 and 2 **offline at 85 °C**.
This phone does neither. Under a sustained load it has nowhere to go.

Almost everything here is carried out of tree. Until it is upstream, every
kernel update is a merge. The q6afe clock fix, the sound card and the WCD9320
codec driver are not Z2 specific and would benefit other Qualcomm devices;
`../upstream/README.md` says what is ready to submit and in what order.

## Shortest route to a phone someone could carry

1. Suspend and resume, which probably also fixes the touchscreen.
2. The microphone on calls.
3. Proximity's near threshold checked against a real face, so a call survives
   contact with one.

That is days rather than weeks, and it would leave a phone that makes and
takes calls both ways, texts, carries data, lasts a day and can be unlocked.
The camera would still be missing.
