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

## Blocking: it cannot be used as a phone yet

**Two-way calls.** A call connects, the caller is heard, and audio generated
on the phone reaches the far end through the DSP's in-call playback. The
microphone does not: nothing it picks up is transmitted. The whole
application-processor side has been eliminated by measurement — the codec's
capture chain powers up and the AFE port is configured identically to an
ordinary recording that captures real audio at the same moment — so the fault
is inside the DSP's voice processor. `../drivers/audio/q6voice/README.md`
lists what has been ruled out and what to try next; the cheapest untried
thing is four transmit topologies this DSP accepts but that have never been
tried for audio. Days, and it may need calibration data.

**Suspend and resume.** Does not work: resume loses Wi-Fi and the
touchscreen, so suspend is disabled. Without it the battery lasts hours
rather than days. The kernel offers s2idle and the wake path works — the
phone resumes on the power key — so this is driver resume paths rather than
anything fundamental. On the evidence this is now the largest single obstacle
to daily use. Days.

**Touchscreen reliability.** The controller can stop responding entirely at
the greeter, with no unlock gesture possible; a reboot recovers it. A phone
that cannot be unlocked is unusable regardless of what else works. Suspected
to be the s2idle resume path, so it may fall out of the suspend work. Days.

## Blocking for some people, not for all

**Camera.** Still the largest single piece of work here, but two of its six
stages are done and neither of them turned out to be the expensive part. The
control bus works and both sensors identify themselves from the silicon; the
V4L2 media core is built and loaded, and that needed no kernel rebuild and no
flash, because every part of the media stack is a module and the three things
it needs built in were already there for the GPU.

The ISP is done too, and took a day rather than the weeks estimated: msm8974
CAMSS is re-expressed for 6.16, probes clean, and captures frames from its
test pattern generator. Every address, interrupt and clock it needed was
already known from three independent sources, and mainline's `mmcc-msm8974`
already had every clock.

**What is left is the two sensor drivers.** The obstacle was expected to be
the register tables, which are in no kernel, Sony's included, and turn out not
to be in the stock camera HAL either — it computes the writes at run time, so
there is nothing to extract. What replaces them is better than it sounds: both
sensors' power-on defaults were read over CCI and describe a complete
full-resolution mode, 5248 x 3936 for the rear and 1976 x 1144 for the front,
matching Sony's device tree exactly. A driver starts from those rather than
from nothing.

Still weeks rather than days — exposure, gain, link frequency, the smaller
preview modes and the autofocus all have to be worked out on hardware — but
the part that looked like blind reverse-engineering is now mostly a matter of
reading registers. See `camera-plan.md` and `camera.md`.

**NFC.** Not working, and not merely untested: there is no NFC node in the
booted device tree and no driver loaded. The mainline driver supports the
PN547 and a device tree node is written, so this is a flash away from being
testable. Hours, once something else needs flashing anyway.

**Automatic brightness.** The light sensor works and reports the room, and
proximity now reaches userspace. Whether anything drives the backlight from
the light level is a separate question and untested.

**Headphone jack detection.** Nothing notices a jack being plugged in, so
output does not switch by itself. Needs `CONFIG_REGMAP_IRQ` and the jack and
button handling. Hours to days, but it needs a kernel rebuild.

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

**The modem's registration module can wedge.** One cause is fixed — the
trim-area message the modem sends at the end of every answered call was going
unanswered, starving its storage task until the watchdog reloaded the
firmware. Whether it was the only cause is unknown.

**The secondary microphone reads nothing.** Needed for noise suppression and
for room pickup at any distance.

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

## Untested, and cheap to find out

Full-disk encryption, and USB host mode — the controller sits in gadget mode,
which is what USB networking runs on, and no USB role switch is exposed in
sysfs, so host mode needs checking rather than assuming.

## Not a capability, but it decides whether any of this lasts

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
