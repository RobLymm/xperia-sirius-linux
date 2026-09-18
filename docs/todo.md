# To do, and how each is proven done

The ordered work, then the capability checklist to run once it is finished.
`whats-left.md` says how big each item is and why; this file says what to do
next and what "done" means.

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

- [x] Bring up a bearer and configure the interface. Done through
      NetworkManager: `wwan0` gets an address and carries traffic, verified
      bound to that interface. It runs on GPRS, which is slow; the modem
      accepted a request to prefer faster modes but had not moved off GPRS
      when last looked at.
- [ ] Check it survives a modem restart and a reboot.

**Done when** a browser loads a page over the cellular interface with Wi-Fi
switched off, and DNS resolves.

## 3. Camera

Staged in `camera-plan.md`. The stages are unchanged; the first needs a
kernel rebuild, which nothing else here does.

- [ ] **Stage 1** — build the media stack into the kernel
      (`MEDIA_SUPPORT`, `VIDEO_DEV`, `V4L2_FWNODE`, `I2C_QCOM_CCI`,
      `VIDEOBUF2_DMA_CONTIG`). None are set today.
- [ ] **Stage 2** — enable the `cci@fda0c000` node, already complete and
      merely disabled in mainline's `qcom-msm8974.dtsi`, and read the chip ID
      of each sensor over the bus. This settles whether the rear sensor is an
      IMX200 or an IMX220, which the stock tuning files and the public
      specifications disagree about.
- [ ] **Stage 3** — re-express the msm8974 CAMSS support for 6.16. The
      Nexus 5 patch is from 5.17 and its structure has since changed to
      per-SoC resource tables, so it cannot be applied as it stands.
- [ ] **Stage 4/5** — sensor drivers for the rear part and the IMX132 front,
      from Sony's downstream register and power sequences.

**Done when** a still is captured from each camera and a video is recorded
from the rear one.

## 4. CPU frequency scaling

Two separate pieces, and the useful half does not need the hard one.

The series is written and compiles; see `../drivers/cpufreq/`. It has never
been booted, and it is not loadable modules, so it needs a boot image and a
flash.

- [ ] **Boot it and confirm the basics**: a `cpufreq` directory appears,
      `scaling_available_frequencies` lists the points up to the cap, and
      `scaling_cur_freq` drops at idle. That last one is the battery result
      and is worth having on its own.
- [ ] **Check the core really runs at the rate claimed**, with a timed
      workload rather than by trusting the file, and that temperature under
      sustained load stays sane.
- [ ] **Then raise the cap.** Every operating point up to 2457.6 MHz is
      already generated; those above 960 MHz carry `status = "disabled"`.
      Enabling them depends on the supply patch doing what it says, which is
      why the cap exists.
- [ ] The patch header refers to a `gen-krait-opp.py` for regenerating the
      table. It is not in the repository and was not on the device; either
      write it or edit the operating points directly.

**Done when** `scaling_cur_freq` moves with load, drops at idle, and the
phone is measurably cooler and longer-lived at rest.

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
