# To do, and how each is proven done

Last verified against the device on 2026-09-19.

The ordered work, then the capability checklist to run once it is finished.
`whats-left.md` says how big each item is and why; this file says what to do
next and what "done" means.

**Check the device before believing any of this.** Two entries were wrong for
weeks in opposite directions — CPU scaling recorded as broken while the phone
ran its full rated range, and the light sensor recorded as dead while it was
reporting the room. The capability checklist at the end is also the way to
re-verify the state table, and it is worth running periodically rather than
only at the end.

## 0. Before anything else

- [x] **Revert `voicehold.sh` to open both directions of the voice PCM.** It
      was left opening playback only, for an experiment that failed, and the
      driver waits for both by default — so as it stands a call has no
      downlink audio. One line: `sed -i 's|3600 playback|3600|'`, then
      restart `sirius-voicehold`.
- [x] **Get the CPU frequency work into the repository.** The state table
      says clock patches and an OPP table are prepared; they exist only on
      the phone. Anything not in the repository does not survive the phone.

## 1. The microphone on calls

The phone can be listened to and can speak; it cannot be spoken into.
Everything ruled out so far is in `../drivers/audio/q6voice/README.md`.

Cheapest first, and the first two need no call:

- [x] **Scan the transmit topologies the DSP will accept.** Done: it accepts
      `0x10F70` to `0x10F75` and rejects everything above, including
      `0x10F77`, which confirms `RX_DEFAULT` is receive-only. So there are
      **four accepted transmit topologies never tried for audio** —
      `0x10F72` (`TX_DM_FLUENCE`), `0x10F73`, `0x10F74`, `0x10F75`. If any is
      a pass-through it would need no calibration. Testing them for audio
      needs a call, and that is the first thing to do with the next one.
- [ ] **Send `VSS_IVOCPROC_CMD_SET_DEVICE_V2`** (`0x000112C6`) after enable.
      Untried; the structure is known.
- [ ] **Ask whether the transmit leg works for any source at all.** Point the
      voice processor at `SEC_MI2S_TX` (`0x1003`) during a call with the FM
      radio playing. The radio is a known-good source on a transmit port that
      is not the microphone's. This is the test that halves the search:
      - far end hears the radio → the leg works, and the fault is the
        microphone reaching `SLIMBUS_0_TX`: a codec and SLIMbus problem;
      - far end hears nothing → the leg is broken whatever feeds it, and
        calibration is the explanation.
- [ ] **Then one of:** fix the codec's transmit path, or do the calibration
      work — map DSP-visible memory with `VSS_IMEMORY_CMD_MAP_PHYSICAL`,
      take the vocproc and vocstrm blocks out of Sony's ACDB data on the
      stock partition, and send the four registration commands.
- [ ] **Speakerphone.** Once the microphone reaches the far end at all, raise
      the analogue gain for room pickup and check it does not clip; a close
      talker at the gain used for testing clipped at full scale.
- [ ] **The secondary microphone reads nothing.** Needed for noise
      suppression and for room capture at any distance.

**Done when** someone at the far end can hear a person speaking into the
handset, and can hear a person a metre away with the phone on a table.

## 2. Mobile data

**Done.** A bearer comes up through NetworkManager, `wwan0` gets an address,
and traffic was verified bound to that interface rather than by trusting the
default route, which was still Wi-Fi. It reconnected by itself after a
reboot, so autoconnect works.

- [ ] It runs on GPRS, which is slow. The modem accepted a request to prefer
      faster modes but had not moved off GPRS. Worth finding out whether that
      is coverage, the SIM, or the modem.

## 3. Camera

Staged in `camera-plan.md`. Two stages are done, and the gate has moved.

- [x] **Stage 1** — the media core. It needed no kernel rebuild and no flash:
      every part of the media stack is tristate, and `DMA_SHARED_BUFFER`,
      `CMA` and `DMA_CMA` are already built in for the GPU carveout, so it
      builds out of tree against the running kernel. Eleven modules are
      installed and loaded.
- [x] **Stage 2** — the CCI bus and sensor identification. Both sensors
      answer and identify themselves from the silicon: rear **IMX200**, front
      **IMX132**. The phone runs `images/boot-cam-v2.img`, which carries the
      tree this needs.
- [ ] **Stage 3** — re-express the msm8974 CAMSS support for 6.16. **This is
      now the gate**: no `/dev/video*` can exist until it lands, so neither
      sensor driver nor libcamera nor the camera app can be tested. The Nexus
      5 patch is from 5.17 and its structure has since changed to per-SoC
      resource tables, so it cannot be applied as it stands. `camera.md` has
      the implementation spec, the clock map and the four places camss
      branches on SoC version.
- [ ] **Stage 4/5** — sensor drivers for the IMX132 front and the IMX200
      rear. Sony's published kernel gives the power sequences exactly and
      **no register or mode tables at all**; those were in the userspace HAL
      and have to come off the stock system partition.

**Done when** a still is captured from each camera and a video is recorded
from the rear one.

## 4. CPU frequency scaling

**Done.** The full rated range works: 300 MHz to 2265.6 MHz on `cpufreq-dt`
with schedutil, all four cores, 36-45 C under load. Verified with a timed
workload — 25.2 s at the bottom against 4.6 s at the top — rather than by
trusting `scaling_cur_freq`.

Nothing needed flashing: the kernel already carried the patches, as
`linux-postmarketos-qcom-msm8974` r10, and the device tree already had the
nodes. This repository had it recorded as broken and as needing a driver that
turned out to be written and working.

- [ ] Worth a look anyway: whether the thermal trips are sensible under a
      long sustained load, rather than the short one measured here.

## 5. Proximity

- [x] Characterise the sensor. It is an **APDS-9930**, not the TSL2772
      whose module is also loaded. It reads 93-122 with nothing in front.
- [x] Make something able to consume it. It needed no calibration: what was
      missing was a `PROXIMITY_NEAR_LEVEL` udev property, without which
      iio-sensor-proxy reported `HasProximity=false`. With
      `userspace/udev/90-sirius-proximity.rules` it reports true, and near
      and far both propagate, proved by moving the threshold below the noise
      floor and watching "near" assert.
- [ ] Check the threshold against a real face. It is set to 250 against a far
      reading of 93-122, which is a guess until someone covers the sensor.
- [ ] Check the screen actually blanks during a call.

**Done when** the screen blanks when the phone is held to a face during a
call and comes back when it is moved away, without dropping the call.

## Quick wins, minutes each

Small enough to do while waiting for something else, and each turns a claim
into a capability.

- [ ] **Make the vibrator buzz.** `pm8xxx_vib_ffmemless` exists; nothing has
      driven it.
- [ ] **Light the notification LED.** `rgb:status` exists; same.
- [ ] **Check the proximity threshold against a face**, rather than against
      the noise floor it was set from.
- [ ] **Drive the backlight from the light sensor**, which works and is
      currently consumed by nothing.

## Also outstanding

Not on the priority list above, but each has a known symptom.

- [ ] **Suspend and resume.** Resume loses Wi-Fi and the touchscreen, so
      suspend is off and the battery lasts hours rather than days. On the
      evidence this is the single largest obstacle to using the phone daily,
      and it may take the touchscreen fault with it.
- [ ] **The touchscreen can go dead at the greeter**, needing a reboot.
- [ ] **Applications rendering on the GPU hang it**, so everything runs on
      the software renderer and is slower than it should be.
- [x] **The dialler segfaults** — diagnosed: an upstream GTK bug, a NULL
      toplevel dereferenced in `gdk_wayland_toplevel_remove_from_session`,
      fixed in GTK 4.22.5. The phone has 4.22.4 and Alpine has not packaged
      4.22.5 yet, so this is waiting on a package or a cross build of GTK.
- [ ] **Capture returns exact zeros on some attempts**, with an identical
      driver trace on good and bad runs: a race in SLIMbus channel
      activation.
- [ ] **The modem's registration module can still wedge.** One cause is
      fixed; whether it was the only one is unknown.
- [x] **~~Automatic brightness: the light sensor reads 0 lux~~** — it does
      not. The APDS-9930 reports 48-86 lux and follows the room. The zero was
      SensorProxy's `LightLevel` property, which reads 0 until a client
      claims the sensor. Whether anything actually drives the backlight from
      it is a separate question, and untested.
- [ ] **Headphone jack detection**: needs `CONFIG_REGMAP_IRQ` and the jack
      and button handling.
- [ ] **Upstream what is not Z2 specific.** The q6afe clock fix, the sound
      card and the WCD9320 codec would benefit other Qualcomm devices, and
      until they are upstream every kernel update is a merge.

## Capability checklist

To run once the list above is done. Each line is a thing the phone either
does or does not do, in the order someone would find out.

**Telephony**
- [ ] Place a call; both people can hear each other
- [ ] Receive a call, including while the screen is locked
- [ ] Speakerphone: someone a metre away is heard at the far end
- [ ] Earpiece, loudspeaker and headphones each carry call audio
- [ ] Mute silences the microphone and unmute restores it
- [ ] The screen blanks against a face and comes back
- [ ] A call ends cleanly and the next incoming call arrives
- [ ] Twenty consecutive calls with no modem restart
- [ ] Send and receive SMS

**Data and networking**
- [ ] A page loads over cellular with Wi-Fi off, and DNS resolves
- [ ] Data survives a reboot and a modem restart
- [ ] Wi-Fi connects, and reconnects after suspend
- [ ] Bluetooth pairs, and carries audio to headphones
- [ ] GNSS gets a fix outdoors

**Camera**
- [ ] A still from the rear camera, in focus
- [ ] A still from the front camera
- [ ] A video from the rear camera, with sound
- [ ] The flash fires

**Audio**
- [ ] Loudspeaker, earpiece and headphones all play
- [ ] Plugging in headphones switches output on its own
- [ ] The microphone records
- [ ] Twenty consecutive recordings, none of them silent
- [ ] FM radio tunes and plays
- [ ] Audio keeps working across a call and after it

**Power and responsiveness**
- [ ] Core frequency rises under load and falls at idle
- [ ] The phone suspends and resumes with Wi-Fi and touch intact
- [ ] Overnight on standby costs a small fraction of the battery
- [ ] It charges, and reports a sensible percentage
- [ ] Applications render on the GPU without hanging it

**Sensors and input**
- [ ] The screen rotates
- [ ] Brightness follows the room
- [ ] Compass points north
- [ ] The touchscreen works after a resume, every time
- [ ] The phone can be unlocked, every time, without a reboot
