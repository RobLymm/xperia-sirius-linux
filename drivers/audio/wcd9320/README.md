# WCD9320 (Taiko) codec driver

The Xperia Z2's headphone jack and all of its microphones are wired to a
WCD9320 ("Taiko") audio codec on SLIMbus. The earpiece is not part of this: it
is a separate NXP TFA9890 amplifier on MI2S, the same kind used for the
loudspeakers, and it works without this driver.

There is no WCD9320 driver in mainline. This is z3ntu's driver from the
`flto-msm8974-5.11` branch of `z3ntu/linux`, forward-ported to 6.16.

## State

The codec enumerates, probes, registers its DAIs and controls, and the whole
playback path powers up: `AIF1 PB` through `SLIM RX1/RX2 MUX`,
`RX1/RX2 MIX1`, `CLASS_H_DSM MUX` and `HPHL/HPHR DAC` to `HPHL`/`HPHR`. The
charge pump, the class-H block and both headphone amplifiers switch on, and
the amplifier status registers respond to the signal while a tone plays.

Capture is untested. There are no microphone links in the device tree yet, no
jack detection, and no UCM profile.

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

## Still to do

- Microphones: SLIMBUS_0_TX links in the device tree, the ADC and decimator
  paths, and micbias.
- Jack detection, which needs regmap-irq or an equivalent, and the MBHC
  hardware driven rather than `set_jack` merely storing the pointer.
- A UCM profile so the audio server can select the jack.
- SLIMbus bandwidth reservation, stubbed here as `return 0` where the
  downstream driver votes a clock gear.
- The debug prints and the `unimplemented` stubs inherited from the 5.11
  branch.
