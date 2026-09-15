# FM radio on the Xperia Z2: it is the Broadcom chip

The Z2's FM tuner is inside the **Broadcom BCM4335C0** combo chip that also
provides Bluetooth, and it is controlled over the Bluetooth HCI link. It is
not a Qualcomm WCNSS tuner: Sony's device tree has no WCNSS, pronto or riva
node, no FM tuner sits on any I2C bus, and Sony's own Android build for the
shinano family sets `BOARD_HAVE_BCM_FM := true`.

This corrects an earlier assumption. `../drivers/fm/radio-wcnss-fm.c` was
written for the WCN3680 tuner found on other msm8974 phones such as the
Fairphone 2. It compiles and may be useful there, but it does not apply to
this phone.

## Verified on the phone

With the mainline `hci_uart_bcm` driver already bringing Bluetooth up, the FM
core answers vendor command `0xFC15` from userspace:

    hcitool cmd 0x3f 0x15 0x00 0x01 0x01      read register 0x00 (RDS_SYS)
    > 01 15 FC 00 00 01 00                     status 00, value 00: FM off

Powered on by writing `0x01` to register `0x00`, which then reads back `01`.
Tuned to 98.8 MHz by writing `F0 87` to the frequency register `0x0a` and
preset mode `0x01` to `0x09`: the frequency reads back `F0 87` and the flag
register reports tune complete with no failure bit.

No signal yet — RSSI `0x80`, SNR `0x02`, unchanged by tuning — which is what
a phone with no headphones plugged in should show, since the headphone lead
is the aerial.

## Radio 1, identified

With headphones plugged in as the aerial, a sweep of the band found clear
stations, and `../tools/bcm-fm-rds.py` decoded their RDS:

    98.9 MHz   -68 dBm  SNR 29   PI C201   "BBC R1  "
    89.3 MHz   -65 dBm  SNR 37   PI C202   "BBC R2  "
   101.1 MHz   -60 dBm  SNR 36   PI C2A1   "Classic "

The tuner, the aerial path and RDS all work. What is missing is audio: see
step 2 below.

## The protocol

From Sony's kernel, `drivers/bluetooth/broadcom/v4l2_fm_driver/` in LineageOS
`android_kernel_sony_msm8974`:

- command: HCI vendor opcode `0xFC15`, payload `register, rw, data...` with
  `rw` 0 for write and 1 for read (a read's data is the byte count)
- response: command complete `01 15 FC <status> <register> <rw> <data...>`
- `0x00` RDS_SYS: `0x01` FM on, `0x02` RDS on
- `0x01` FM_CTRL: `0x02` stereo auto, `0x01` Japan band
- `0x05` AUD_CTL0 (2 bytes): bit1 manual mute, bit4 DAC out, bit5 I2S out,
  bit6 75 µs de-emphasis (clear for UK/Europe 50 µs)
- `0x09` SCH_TUNE: 0 scan, 1 tune to preset, 2 seek
- `0x0a` FM_FREQ (2 bytes LE): kHz − 64000
- `0x0f` RSSI, `0xdf` SNR, `0x12` flags (bit0 tune complete, bit1 failed)
- `0x4d` PCM_ROUTE, `0xf8` volume, `0x80` RDS data

`../tools/bcm-fm.sh` wraps these: `on`, `tune 98.8`, `status`, `sweep`.

## Remaining steps

1. **Reception.** Plug in headphones, `bcm-fm.sh on`, `bcm-fm.sh sweep`, and
   look for RSSI and SNR peaks. BBC Radio 1 is on 97.6–99.8 MHz depending on
   region. RDS would then confirm the station by name without any audio.
2. **Audio.** Sony's stock `/etc/mixer_paths.xml` answers the wiring question:
   FM audio enters the DSP digitally on the LPASS *internal FM* port, as
   `INTERNAL_FM_TX`, and stock Android plays it with
   `SLIMBUS_0_RX Port Mixer INTERNAL_FM_TX` (to the WCD9320) or records it with
   `MultiMedia1 Mixer INTERNAL_FM_TX`. So the Broadcom chip's I2S output
   (AUD_CTL0 bit5) is the one in use, not its DAC. Mainline q6afe has no
   internal FM port at all, so the step is to add `INT_FM_TX` (AFE port
   0x3005) to q6afe and q6routing; capturing it on MultiMedia1 and playing it
   through the working speaker path then needs no WCD9320.

   On the Broadcom side, Sony's `fm_rx_config_audio_path` shows the whole
   recipe: set `FM_AUDIO_I2S_ON` (bit 5) in AUD_CTL0, clear the manual mute
   (bit 1), and write PCM_ROUTE back unchanged; no extra vendor command is
   needed when FM I2S is not being redirected over the Bluetooth PCM pins.
   **Status, 2026-09-13 13:50: FM audio plays through the speaker.** The
   internal FM port turned out not to be where the audio arrives on Linux:
   captured from `INT_FM_TX` it is all zeros. The chip's PCM pads are wired
   to the SoC's secondary MI2S pads (Sony's `qcom,sec-auxpcm-gpio-*`: gpio79
   clock, gpio80 sync, gpio81 data in). The working route is:

   - vendor command `0xFC61` Write_PCM_Pins `05 19 18 18 18`: the PCM pads
     carry FM I2S with the chip as clock master (bit clock 1.53 MHz, word
     select 47.8 kHz, measured on the pads). In slave mode (`07 ..`) the chip
     sends no data even when LPASS clocks it.
   - `PCM_ROUTE` bit 7 clear, volume `0xf8` = 255, `AUD_CTL0` = `0x2c`: I2S
     on, left and right un-muted (bits 2 and 3, which Broadcom's own driver
     always sets; without them the samples are zero), 50 µs de-emphasis.
   - LPASS `SECONDARY_MI2S_TX` as a codec-less back end with the CPU DAI as
     clock consumer, captured on its own front end, MultiMedia2 (MultiMedia1
     is held by the sound server for the speaker, and a q6asm session serves
     one direction). Device tree: `int-fm`-style links in the FM variant of
     the board file; machine driver change in the working project's
     `drivers-wip/`.

   `sirius-fmd` does the chip side; `../tools/fm-play.sh 98.9` and the [Robwatts FM Radio](https://github.com/RobLymm/robwatts-fm-radio) app do
   the rest. Patch 0014 (`INT_FM_TX`) is not needed for audio and stays only
   as documentation of the port.

   Background "flicking" (a 41.6 Hz click train on every station): the
   tuner is a fixed I2S master on its own crystal and the LPASS MI2S
   receiver samples on its own bit clock, so once every ~24 ms the sign
   bit of a burst of ~30 samples is read at its transition. Not fixable at
   the link (chip sends nothing as a slave; the AFE has no bit-clock
   polarity field). Repaired at the ALSA device layer by the `fmrepair`
   plugin (`drivers/audio/fmrepair`, device `sirius_fm`), which interpolates
   the burst windows; see `drivers/audio/README.md`. Weak-signal stereo
   hiss is a separate thing: the optional Mono / Noise-reduction modes sum
   to mono and low-pass at 12 kHz (the app's `sirius-fm-downmix` helper),
   as a hardware receiver does.

   Two dead ends on the way, worth knowing: the phone's kernel is the
   msm8974-mainline fork, so a device tree compiled from vanilla sources
   differs from the running one in thousands of properties; and the kernel is
   built with clang and kCFI, so GCC-built modules must not be loaded.

3. **A kernel driver.** Mainline has no Broadcom FM driver. Userspace control
   is enough to prove and use the tuner; a V4L2 driver over `btbcm` vendor
   commands, modelled on Sony's, is the eventual form.
