# PM8941 clock divider (codec master clock)

The WCD9320 codec takes a 9.6 MHz master clock on a dedicated pin. On the
Xperia Z2 that clock comes out of the PM8941: the PMIC divides its 19.2 MHz
crystal and puts the result on PMIC GPIO 15, whose alternate function 1 is the
divider output. Two things therefore have to be set up before the codec can
make any sound:

1. the divider has to be told to divide by two, and
2. GPIO 15 has to be switched from plain input to alternate function 1.

Mainline already has a driver for the divider,
`drivers/clk/qcom/clk-spmi-pmic-div.c`, but it cannot be used on msm8974 as
written. It names its outputs `div_clk1`, `div_clk2`, `div_clk3`, and the RPM
clock controller on this SoC has already registered clocks under exactly those
names. The second registration fails with `-EEXIST` and the whole controller
fails to probe.

This directory holds that driver with one change: it takes the output names
from the `clock-output-names` device tree property when it is present, and
falls back to the old hard-coded names when it is not. The change is in
`0001-clk-qcom-spmi-pmic-div-take-names-from-the-device-tre.patch`, which
applies to mainline.

The second part, the pin, is device tree only and lives in
`devicetree/qcom-msm8974pro-sony-xperia-sirius-codec.dts`.

## Build

    make -s LLVM=1 -C /path/to/linux M=$PWD

## Device tree

    pm8941_clk_divs: clock-controller@5b00 {
        compatible = "qcom,spmi-clkdiv";
        reg = <0x5b00>;
        #clock-cells = <1>;
        qcom,num-clkdivs = <3>;
        clocks = <&xo_board>;
        clock-names = "xo";
        clock-output-names = "pm8941_div_clk1", "pm8941_div_clk2",
                             "pm8941_div_clk3";
    };

The codec then asks for `<&pm8941_clk_divs 1>` and calls `clk_set_rate()` with
9600000.

## Checking it on hardware

The divider registers are readable through the PMIC's regmap. Divider 1 sits at
0x5b00; offset 0x43 holds the divide factor and offset 0x46 the enable bit:

    sudo cat /sys/kernel/debug/regmap/0-00/registers | grep -E '^5b4[36]:'

A factor of 0 or 1 means divide by one (19.2 MHz), 2 means divide by two
(9.6 MHz). The pin state is in pinctrl debugfs; before this change GPIO 15 read
`(MUX UNCLAIMED)`:

    sudo grep 'pin 14' \
      '/sys/kernel/debug/pinctrl/fc4cf000.spmi:pm8941@0:gpio@c000/pinmux-pins'
