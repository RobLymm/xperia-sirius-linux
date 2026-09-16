# WCD9320 (Taiko) codec driver — work in progress, does not work yet

**This does not produce audio yet and must not be treated as finished.** The
codec side works (see below); the ADSP still refuses to start the SLIMbus
port that would carry the audio to it.

## What this codec provides on this phone

The Xperia Z2's headphone jack and all of its microphones are wired to a
WCD9320 ("Taiko") audio codec, which sits on SLIMbus. The earpiece is not
affected by any of this: it is a separate NXP TFA9890 amplifier driven over
MI2S, the same kind of amplifier used for the loudspeakers, and it already
works. Until this driver works, the phone has speakers but no headphone
output and no microphones.

## Where this driver came from

There is no mainline WCD9320 driver. This one was originally written for
this phone by Craig Tatlor, on the `old-4.18.0/qcom-audio-wip` branch of
`msm8974-mainline/linux`, against Linux 4.18. It has been forward-ported to
6.16.

## What the port to 6.16 needed

- `snd_soc_component_read32()` was renamed; the driver now calls
  `snd_soc_component_read()`.
- The platform driver's `remove` callback changed from returning `int` to
  returning `void`.
- The DAI ops for setting a channel map now take a `const` channel map
  argument.
- SLIMbus streaming moved to the `slim_stream_allocate(name)` /
  `slim_stream_prepare(rt, cfg)` API.
- The original code registered the SLIMbus driver and the platform driver
  from two separate module entry points; this port registers both from a
  single module entry point.
- The kernel this is built against does not have `CONFIG_REGMAP_SLIMBUS`
  enabled, so `wcd9320-regmap-slimbus.c` is a local regmap-over-SLIMbus shim
  that stands in for it.

## Four bugs found and fixed on the device

These were found by getting the driver onto the phone and working through
the failures one at a time, not by inspection. They are recorded in detail
so the same mistakes are recognisable if they show up in a similar port.

1. **A zeroed mux control oopsed DAPM and took the whole sound card down.**
   `slim_rx_mux[WCD9320_RX_MAX]` was filled positionally, but the DAPM
   widgets index it by `WCD9320_RXn`, and that enum starts at 1, not 0. So
   the widget named "SLIM RX2 MUX" indexed an array slot that had never been
   written and pointed at a zeroed `snd_kcontrol_new`. Its `private_value`
   was `NULL`, and `dapm_connect_mux()` (called from
   `snd_soc_dapm_add_path()`) dereferenced it as a `soc_enum`, which oopsed
   the kernel. Because this happened during card registration, it took the
   entire sound card down with it — even the speakers, which have nothing to
   do with this codec, stopped working. Fixed by filling all seven port
   entries with designated initialisers instead of positionally.

2. **Every SLIMbus channel reported channel number 0.** The `WCD_SLIM_CH()`
   macro set only `.port` and `.shift` on each channel, and never set
   `.ch_num`. The only code that did set `.ch_num`, `set_channel_map()`,
   wrote it into a different array from the one the DAI actually walks when
   it builds the channel list, so the value never took effect. The ADSP
   received channel number 0 for every channel and rejected the port. Fixed
   so `.ch_num` equals the port number (16 upward).

3. **An off-by-one in the RX port mapping.** The SLIM RX muxes were indexed
   one higher than the ports they should have driven, so only "SLIM RX2
   MUX" ever configured an interpolator (RX2), and "SLIM RX1 MUX" configured
   nothing. Re-based the muxes to 0 so "SLIM RX1 MUX" and "SLIM RX2 MUX"
   drive ports 16 and 17 and configure interpolators RX1 and RX2
   respectively.

4. **The SLIMbus interface device never obtained a logical address.** The
   status callback fetched the interface device with `of_slim_get_device()`
   and immediately built a regmap on top of it, without ever calling
   `slim_get_logical_addr()`. Every register write to that device was
   therefore sent without a valid logical address and was NACKed by the bus
   master (visible as an NGD error interrupt, `TX_NACKED_2`). Mainline's
   `wcd9335` driver calls `slim_get_logical_addr()` in the equivalent place;
   this port now does too.

## Current state

The codec enumerates on SLIMbus, probes, registers its 10 DAIs, its mixer
controls appear, register reads and writes work, and it prepares and enables
its SLIMbus stream cleanly with a correct configuration. The sound card
registers and the speakers work.

The remaining blocker is on the ADSP side: `AFE_PORT_CMD_DEVICE_START`
(`0x000100E5`) for `SLIMBUS_0_RX` (port `0x4000`) returns error 1, and
`prepare` fails with `-EINVAL`. There is still no headphone or microphone
audio. This has been checked against several possible causes and none of
them explain it:

- Not the port configuration: the ADSP receives version 1, device ID 0, bit
  width 16, format 0, rate 48000 Hz, 2 channels, channel map 16/17, and
  accepts that `SET_PARAM`.
- Not AP-side channel allocation: the failure is identical when channel
  allocation is skipped entirely.
- Not controller power state: the failure is identical with the NGD
  controller forced runtime-active.
- Not a firmware problem in general: the same ADSP firmware drives MI2S
  ports without any trouble — the speakers and the FM capture path both work
  through it.

## Two open leads

- z3ntu's `linux` branch `flto-msm8974-5.11` reportedly got headphones
  working on other msm8974 devices (hammerhead, Fairphone 2) using the
  apq8096 machine driver. Worth comparing against what this port does.
- SLIMbus bandwidth reservation is stubbed out in this port:
  `wcd9320_codec_slim_reserve_bw()` returns 0 unconditionally, with a FIXME
  where the downstream driver votes a clock gear instead.
