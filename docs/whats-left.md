# What is left

An honest account of the gap between this phone and one someone could use as
their only phone. Ordered by whether it blocks that, not by how interesting
it is.

Effort figures are rough and mean developer time, not calendar time. Where
something needs a driver that does not exist anywhere, that is said.

## Blocking: it cannot be used as a phone yet

**Two-way calls.** A call connects, the caller is heard, and audio generated
on the phone reaches the far end. The microphone does not: nothing it picks
up is transmitted. So the phone can be listened to and can speak, but cannot
be spoken into. The whole application-processor side has been eliminated by
measurement — the codec's capture chain powers up and the AFE port is
configured identically to an ordinary recording that captures real audio at
the same moment — so the fault is inside the DSP's voice processor. See
`../drivers/audio/q6voice/README.md` for the five causes already ruled out.
Days, and it may need a DSP-side command nobody has identified yet.

**Suspend and resume.** Does not work: resume loses Wi-Fi and the
touchscreen, so suspend is disabled. Without it the battery lasts hours
rather than days and the phone cannot idle. This is the single largest
obstacle to daily use. The wake path itself works (s2idle resumes on the
power key), so this is driver resume paths rather than anything fundamental.
Days.

**CPU frequency and voltage scaling.** All four cores run at a fixed 960 MHz
of the rated 2265.6 MHz. That is both a speed problem and a power problem:
nothing ever clocks down when idle. An OPP table and clock patches for
300–960 MHz at the present voltage are prepared. Going above 960 MHz needs
higher CPU voltage, which needs a driver for the Krait per-core regulators on
the PM8841 that mainline does not have. Days for the low half; weeks and a
new regulator driver for the full range.

**Touchscreen reliability.** The controller can stop responding entirely at
the greeter, with no unlock gesture possible; a reboot recovers it. A phone
that cannot be unlocked is unusable regardless of what else works. Suspected
to be the s2idle resume path plus a libinput double-tracking-id bug, so it
may fall out of the suspend work. Days.

**Proximity sensor calibration.** The sensor responds but is uncalibrated, so
nothing can reliably blank the screen during a call. Without it a cheek
presses buttons mid-call. Hours to days.

## Blocking for some people, not for all

**Camera.** Does not work at all. The msm8974 camera subsystem exists out of
tree for the Nexus 5; the Z2's two Sony sensors have no drivers anywhere.
This is the largest single piece of work in the repository and the one least
likely to be finished by adapting something. Weeks, and two new sensor
drivers.

**Mobile data.** Untested. The modem registers, reports its IMEI, appears in
ModemManager and carries calls, so the remaining work is a data bearer and
routing rather than bring-up. Probably hours.

**SMS.** Untested, on a modem that is otherwise up. Probably hours.

**Automatic brightness.** The light sensor binds and its registers read
correctly but it reports 0 lux, so brightness cannot follow the room.
Hours to days.

**Headphone jack detection.** Nothing notices a jack being plugged in, so
output does not switch by itself. The codec's interrupt support needs
`CONFIG_REGMAP_IRQ`, which is a kernel configuration change plus the jack
and button handling. Hours to days.

## Reliability faults, each with a known symptom

**Applications rendering on the GPU hang it.** The compositor runs on the
Adreno 330 through freedreno, but applications that render on it hang the GPU
and can eventually deadlock the compositor, so GTK applications are run with
the software renderer. A divide-by-zero in the driver's submit retirement
path has also been seen. The cost is a slower interface everywhere. Days, in
someone else's driver.

**The modem's registration module can wedge.** After some calls the modem
stops answering, and incoming calls are refused while ModemManager still
reports it registered. Its own watchdog reloads the firmware and it recovers.
One cause of this has been fixed — the trim-area message the modem sends at
the end of every answered call was going unanswered, starving its
non-volatile storage task — and there may be more. Unknown.

**Capture returns exact zeros on some attempts.** Recording from the handset
microphone succeeds most of the time and occasionally returns the right
number of frames of pure silence, with an identical driver trace on good and
bad runs. A race in SLIMbus channel activation. Days.

**The dialler crashes.** `gnome-calls` segfaults inside a GObject signal
emission, repeatedly and reproducibly. Not audio and not kernel. Unknown,
probably hours once traced.

**The secondary microphone reads nothing.** The handset microphone works; the
second one is not wired up correctly yet. Hours to days.

## Untested, and cheap to find out

Full-disk encryption, USB host mode (the controller is in OTG mode and the
PM8941 identification pin works, but nothing has been plugged in), and NFC
(the NXP PN547 has a mainline driver and a device tree node is written but
has never been flashed).

## Not a capability, but it decides whether any of this lasts

Almost everything here is carried out of tree. Until it is upstream, every
kernel update is a merge. The q6afe clock fix, the sound card and the WCD9320
codec driver are not Z2 specific and would benefit other Qualcomm devices;
`../upstream/README.md` says what is ready to submit and in what order.

## Shortest route to a phone someone could carry

1. Suspend and resume, which probably also fixes the touchscreen.
2. The microphone on calls.
3. CPU scaling up to 960 MHz, for battery life.
4. Proximity calibration, so calls survive contact with a face.
5. Mobile data and SMS, which are likely near-complete already.

That set is days rather than weeks and would leave a phone that makes and
takes calls, texts, carries data, lasts a day and can be unlocked. The camera
and the full CPU range would still be missing.
