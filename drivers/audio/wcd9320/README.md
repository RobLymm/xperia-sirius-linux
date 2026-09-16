# WCD9320 (Taiko) codec driver

The Xperia Z2's headphone jack and all of its microphones are wired to a
WCD9320 ("Taiko") audio codec on SLIMbus. The earpiece is not part of this: it
is a separate NXP TFA9890 amplifier on MI2S, the same kind used for the
loudspeakers, and it works without this driver.

There is no WCD9320 driver in mainline. This is z3ntu's driver from the
`flto-msm8974-5.11` branch of `z3ntu/linux`, forward-ported to 6.16.

## State

Headphone playback works and has been heard. The codec enumerates, probes,
registers its DAIs and controls, and the whole playback path powers up:
`AIF1 PB` through `SLIM RX1/RX2 MUX`, `RX1/RX2 MIX1`, `CLASS_H_DSM MUX` and
`HPHL/HPHR DAC` to `HPHL`/`HPHR`.

Capture does not work at all. Every callback on the capture side is still the
`unimplemented` stub this branch shipped: microphone bias, the ADCs, the
decimators, the digital microphones, the SLIMbus TX ports and the `LDO_H`
supply. There is also no jack detection.

## What the port to 6.16 needed

- `regmap_init_slimbus()` is not available: this kernel is built without
  `CONFIG_REGMAP_SLIMBUS`. `wcd9320-regmap-slimbus.c` is a local shim that
  stands in for it.
- `regmap-irq` is likewise absent. The interrupt block is skipped, which costs
  jack detection and headset buttons; the driver says so when it probes.
- `.type_base` was removed from `struct regmap_irq_chip`.
- `get_channel_map()` and `set_channel_map()` now take const arguments.
- Three enums declared more items than they had texts (`dec8_mux_enum` said 7
  for 5, `anc1_mux_enum` and `anc2_mux_enum` said 16 for 15). Reading the
  control list walked off the end of the text array and oopsed in `strlen`,
  which hung anything that enumerated mixer controls. They now use
  `ARRAY_SIZE()`.

## Three crashes fixed here

All three were reached by `alsactl` restoring the mixer at boot, which meant
the phone oopsed on every cold boot, wedged `alsa-restore.service`, and with
`sound.target` waiting on it never finished starting the graphical session.

1. **A mux control deleted itself from a list that had never been
   initialised.** The channel descriptions are copied from a static template
   whose list heads are zero, and only `set_channel_map()` initialised them,
   which does not run until a stream does. Writing a SLIM RX or TX mux before
   then reached `list_del_init()` on an empty `list_head` and wrote through a
   NULL pointer. They are now initialised when the component registers.

2. **A mux handler read its widget from the wrong field.**
   `snd_kcontrol_chip()` on a DAPM control returns that control's
   `dapm_kcontrol_data`, not a widget list, and the `widget` member of it is
   only filled in for switches and mixers, not muxes. The decimator handler
   cast it to a widget list and dereferenced the NULL. It now uses
   `snd_soc_dapm_kcontrol_widget()`.

3. **A plain control was treated as a DAPM one.** "ANC Function" is a
   `SOC_ENUM_SINGLE_EXT`, so `snd_soc_dapm_kcontrol_dapm()` does not apply to
   it. The context now comes from the component.

## Two fixes carried over from the earlier 4.18 port

Both were found on hardware, and both apply to this driver too.

1. **SLIMbus shared channel numbers are not port numbers.** The codec's slave
   port numbers and the shared channel numbers the ADSP is told to use are
   different things. Sony's downstream machine driver hands the ADSP 144
   upward for RX and 128 upward for TX, while the codec's own ports are 16
   upward for RX and 0 upward for TX: a fixed offset of 128. Given a channel
   number it does not own, the ADSP refuses the port and
   `AFE_PORT_CMD_DEVICE_START` returns error 1.

2. **The SLIMbus interface device needs a logical address.** The driver built
   a regmap on the interface device without first calling
   `slim_get_logical_addr()`, so every write to it went out without a valid
   address and was NACKed by the bus master. Mainline's wcd9335 driver makes
   that call in the equivalent place.

## The master clock is the thing that makes or breaks it

The codec needs 9.6 MHz on its MCLK pin. Nothing in this driver can create
that: the clock is made by a divider inside the PM8941 and leaves the PMIC on
PMIC GPIO 15, whose alternate function 1 is the divider output. Both the
divider and the pin have to be set up in the device tree, and the PMIC's
divider needs the driver in `../../clk/pmic-clkdiv/`.

Without that clock the codec still answers on SLIMbus, every register write
lands, and every DAPM widget reports itself powered, so the driver looks
healthy while producing silence. The way to tell the difference is the
headphone amplifier status registers, 0x9b3 and 0x9b9, read while a tone
plays: they stay at 0x04 when there is no master clock and move to 0x08 when
there is.

Do not substitute the RPM's `div_clk1`. Mainline models it as a crystal
buffer fixed at 19.2 MHz, so it cannot be set to 9.6, and it does nothing
about the pin.

## The gain has to be told to come from the gain register

Downstream applies a second register table, `taiko_codec_reg_init_val`, after
the power-on defaults. This branch never had it, and one pair of entries in it
is the difference between silence and sound: bit 5 of `RX_HPH_L_GAIN` and
`RX_HPH_R_GAIN` selects the gain held in the register rather than the one the
compander drives. With the companders off and that bit clear the amplifiers
power up, the status registers move, the jack clicks as the amplifier ramps,
and nothing comes out. The same table sets the wave-generator time to 20 ms,
which is what stops the amplifier popping when it powers up.

## Still to do

- Microphones: six callbacks to port from Sony's driver (`micbias`, `adc`,
  `dec`, `dmic`, `slimtx`, `ldo_h`), plus SLIMBUS_0_TX links and
  `audio-routing` in the device tree.
- Jack detection, which needs regmap-irq or an equivalent, and the MBHC
  hardware driven rather than `set_jack` merely storing the pointer.
- A UCM profile so the audio server can select the jack.
- SLIMbus bandwidth reservation, stubbed here as `return 0` where the
  downstream driver votes a clock gear.
- The debug prints and the `unimplemented` stubs inherited from the 5.11
  branch.
