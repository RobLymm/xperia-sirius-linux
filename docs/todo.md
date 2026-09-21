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

Staged in `camera-plan.md`. **Phosh's camera app takes a photo with the front
camera.** What is left is the rear sensor, and quality.

- [x] **Stage 1** — the media core. It needed no kernel rebuild and no flash:
      every part of the media stack is tristate, and `DMA_SHARED_BUFFER`,
      `CMA` and `DMA_CMA` are already built in for the GPU carveout, so it
      builds out of tree against the running kernel. Eleven modules are
      installed, six of them loaded; the rest load when something needs them.
- [x] **Stage 2** — the CCI bus and sensor identification. Both sensors
      answer and identify themselves from the silicon: rear **IMX200**, front
      **IMX132**. The phone runs `images/boot-cam-v2.img`, which carries the
      tree this needs.
- [x] **Stage 3** — msm8974 CAMSS for 6.16. **Done.** Two patches in
      `../drivers/camera/`. The driver probes clean, enumerates 3 CSIPHY,
      4 CSID, 4 ISPIF lines, 2 VFE and six video nodes, and captures ten
      correct 1920x1080 SRGGB10 frames from CSID0's test pattern generator.
      Two bugs surfaced only by running it, both recorded in that README: an
      unreachable VFE clock rate, and a `switch` on SoC version in
      `vfe_src_pad_code` that a grep for `==` does not find.
- [x] **Stage 4** — IMX132 front sensor. **Working.** Captures 1976x1144
      SBGGR10, and libcamera lists the camera and produces correct colour
      images through its software ISP. `../drivers/camera/imx132.c`.
      Three things had to be right before anything streamed, all recorded in
      that directory's README: the MIPI registers are in Sony's vendor range
      and not where an imx219 keeps them, the D-PHY global timing is zero at
      reset, and the PLL has to match the rate those timings were derived for.
- [ ] **Stage 5** — IMX200 rear sensor, plus the BU64296G autofocus actuator
      and the flash. Same method as the front, larger: 20.7 MP over four
      lanes. **Harder than the front was**, because no equivalent of Intel's
      atomisp tables has been found for this part, and the vendor MIPI
      registers cannot be read out of a sensor that is not streaming.

      Searched on 2026-09-21, and there is **no prior art at all**:

      - Sony's own released kernel driver is
        `drivers/media/platform/msm/camera_v2/sensor/sony_camera_v4l2.c`, and
        it contains no register tables. It reads `i2c_addr`, the power
        sequence and module names from the device tree and the rest from the
        module EEPROM at runtime, so the sequences are not in any GPL source.
      - There is **no `libchromatix_imx200`** on the stock partition. The rear
        camera goes through Sony's own ISP path, not Qualcomm's — only
        `vendor/camera/SOI20BS0_IMX200.dat` (34460 bytes, same header format
        as the front: I2C `0x10`, model `0x0200`), whose body is not a
        register table in any obvious encoding.
      - Nothing in mainline, nothing on GitHub, nothing in libcamera.

      So the register sequences exist only inside Sony's closed camera stack.

      **Searched further on 2026-09-21, and there is now a named target.**
      `/system/lib/libcammw.so` (253 KB, Sony's camera middleware) exports
      **`imx200_get_sensor_param`**, and knows IMX132, IMX134, IMX135 and
      IMX200. That is the place to look. Ruled out along the way, so nobody
      repeats the search:

      - `libmmcamera2_sensor_modules.so` (Qualcomm's sensor modules, 139 KB)
        knows imx132, imx134, imx135, imx214 and the generic `sony_camera_0`
        and `sony_camera_1` -- but **not** imx200.
      - `SOI20BS0_IMX200.dat` is not a register table in any encoding. Tested
        by taking the 66 vendor-init register/value pairs the front sensor is
        known to need and looking for them in `SEM02BN1_IMX132.dat`, whose
        sensor those pairs belong to: 25 of 66 addresses appear as bare 16-bit
        words and only 9 as address+value, which is coincidence rate for a
        5.8 KB file. If the format held register tables the front file would
        have matched almost all of them.
      - Sony's own kernel driver, `sony_camera_v4l2.c`, has no tables.

      For the driver skeleton, the closest mainline Sony sensors in the modern
      `v4l2-cci` style are `imx214.c` (1404 lines) and `imx258.c` (1563); our
      `imx132.c` was built from `imx219.c` the same way. There is no mainline
      driver for imx135, imx220 or imx230.

      A caution against assuming near neighbours are close enough: the front
      sensor's README records that an imx219's MIPI registers did **not**
      work on the IMX132 -- "this part has no 0x0114 lane mode, no 0x0128
      D-PHY control and no 0x012a input clock register: writes are accepted
      and discarded". The SMIA standard registers (mode, PLL, crop, exposure,
      gain) do transfer between Sony parts. The vendor 0x3xxx block does not,
      and that is the half that decides whether the sensor streams.
- [x] **Stage 6** — the camera app. **Snapshot takes a photo**, 1920x1080
      JPEG into `~/Pictures/Camera/`. Two things were needed beyond the
      driver: pipewire runs libcamera itself and needs
      `LIBCAMERA_SOFTISP_MODE=cpu` in *its* environment, which
      `/etc/environment.d` cannot give an already-running user manager —
      hence `../userspace/pipewire.service.d/`.
- [ ] **Quality.** Three faults, all still open as of 2026-09-21, reported by
      Rob after the pkgrel 18 flash. **The prior art for all of this is on the
      stock system partition** — mount it and look there first:

      | where | what |
      |---|---|
      | `vendor/lib/libchromatix_imx132_{common,preview,snapshot,default_video,liveshot}.so` | Qualcomm chromatix tuning for this exact sensor, ~35 KB each, one static struct behind `load_chromatix()`. This is the colour calibration. Needs the `chromatix.h` struct layout from Code Aurora's msm8974 sources, matched to the right version |
      | `vendor/camera/SEM02BN1/color_ctrl.dat` | Sony's own colour data for this unit's front module, 4140 bytes, `excal` magic, fixed-point. A *different* format, feeding Sony's ISP rather than Qualcomm's — all three module directories differ, so it is genuinely per-module |
      | `vendor/camera/SEM02BN1_IMX132.dat` | header decoded: u16@0 file size, u16@8 I2C address `0x36`, u16@10 model `0x0132` |

      - [ ] **Rotation is wrong, and now wrong the other way.** The preview was
            on its side; `sensor-nodes.dtsi` now sets `rotation = <270>` from
            Sony's `qcom,mount-angle`, and it comes out **upside down**, which
            is 180 degrees from right. So the value wanted is **90**, not 270 —
            Sony's mount angle and the V4L2 `rotation` property evidently do
            not share a sign convention. Change it, rebuild the board device
            tree, reflash, and check before believing it.
      - [ ] **Colour.** `userspace/libcamera/imx132.yaml` now exists and is
            installed, with the colour matrices copied from `imx363.yaml` —
            the same borrowing libcamera's own `imx371.yaml` does, and it
            carries the comment saying so. It adds a little saturation. It is
            a guess, not a measurement.

            **Two things were checked first and are not wrong**, so nobody
            re-checks them:

            - *The Bayer phase is right.* The two green positions of the raw
              frame read 275.5 and 276.7 — 1.2 counts apart. A wrong phase
              would put red or blue in one of them and they would differ by
              tens. No channel swap.
            - *The black level is right.* The sensor's own SMIA
              `data_pedestal` register (0x0008) reads `0x0040` = 64, which is
              exactly what libcamera assumes (`blackLevel: 4096`, 64 scaled to
              16 bits). Read over CCI while streaming; the model id at 0x0000
              read `0x0132` in the same dump, so it was the right chip.

            What is still missing is a *measured* matrix, and an entry in
            libcamera's `CameraSensorHelper` — without one it logs "Failed to
            create camera sensor helper for imx132" and AGC's gain model is
            wrong, which affects exposure as well as colour. The measured
            matrix is in `libchromatix_imx132_*.so`, above.
      - [x] **Frame rate — fixed 2026-09-21, 4.1 fps to 50.9 fps.** The
            setting was reaching `pipewire` and not `wireplumber`, and it is
            **wireplumber** that runs libcamera for the camera portal. A
            drop-in on `pipewire.service` alone changes nothing the app sees,
            and `/etc/environment.d` only reaches processes started after the
            user manager read it. `userspace/wireplumber.service.d/` fixes
            that.

            Measured in the app itself, from libcamera's own log line
            (`journalctl --user -n 300 | grep "Debayer processed"`):

                cpu   244209 us/frame    4.1 fps
                gpu    19647 us/frame   50.9 fps

            Do not benchmark this with `cam`. On an idle GPU `cam` flatters
            the GPU path (56 against 8–14); under a synthetic GPU hammering it
            flatters the CPU path (9.7 against 10.9). Neither is the app,
            which is 12x faster on the GPU.

**Done when** a still is captured from each camera and a video is recorded
from the rear one. Half of that is done: the front camera works end to end.

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
