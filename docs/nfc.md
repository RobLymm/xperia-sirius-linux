# NFC: ready to try, driver already in mainline

Not yet tested — the device tree node below has never been flashed. It is
written out because everything needed to test it is known, and because NFC is
the one unimplemented subsystem on this phone that needs no new driver.

## The hardware

Sony's stock device tree has, on `i2c@f9928000` (`blsp1_i2c6`):

    nfc@28 {
        compatible = "nxp,pn547";
        reg = <0x28>;
        interrupt-parent = <&tlmm>;
        interrupts = <0x18 0x01>;          /* GPIO 24, edge rising */
        nxp,irq_gpio  = <&tlmm 0x18 0>;    /* GPIO 24 */
        nxp,dwld_en   = <&tlmm 0x39 0>;    /* GPIO 57, firmware download */
        nxp,ven       = <&pm8941_mpps 2 1>;
        nxp,pvdd_en   = <&pm8941_gpios 34 1>;
    };

An NXP PN547. Mainline supports it directly: `drivers/nfc/nxp-nci/i2c.c`, and
the binding at `Documentation/devicetree/bindings/net/nfc/nxp,nci.yaml` lists
`nxp,pn547` by name as a valid compatible with `nxp,nxp-nci-i2c` as fallback.
Required properties are compatible, `enable-gpios`, `interrupts` and `reg`;
`firmware-gpios` is optional and maps onto Sony's `dwld_en`.

There is a second node, `nfc-nci@e` with `compatible = "qcom,nfc-nci"`, which
is Qualcomm's own NCI shim. Ignore it; the NXP driver talks to the chip
directly.

## Proposed node

```dts
&blsp1_i2c6 {
	status = "okay";

	nfc@28 {
		compatible = "nxp,pn547", "nxp,nxp-nci-i2c";
		reg = <0x28>;

		interrupt-parent = <&tlmm>;
		interrupts = <24 IRQ_TYPE_EDGE_RISING>;

		enable-gpios = <&pm8941_mpps 2 GPIO_ACTIVE_LOW>;
		firmware-gpios = <&tlmm 57 GPIO_ACTIVE_HIGH>;
	};
};
```

`pm8941_mpps` is already a label in mainline's `pm8941.dtsi`, with eight MPPs,
so MPP 2 needs nothing added.

## The one thing to verify

**VEN polarity.** Sony's `nxp,ven = <0x57 0x02 0x01>` has a third cell of 1,
which in their downstream format means active low. That is not the usual
arrangement for a PN547, where VEN high enables the chip and low holds it in
reset, and the mainline driver requests the line with `GPIOD_OUT_LOW` and then
drives it to its active state to enable.

If the node above produces no chip, invert it to `GPIO_ACTIVE_HIGH` before
looking anywhere else. Both polarities are one line apart and cost nothing to
try.

`nxp,pvdd_en` on PM8941 GPIO 34 has no equivalent in the mainline binding. If
the chip does not answer on either polarity, that rail is the next thing to
look at: it may need to be driven, or described as a fixed regulator that the
node consumes.

## Testing it

Needs `CONFIG_NFC_NXP_NCI` and `CONFIG_NFC_NXP_NCI_I2C`, plus `CONFIG_NFC`
and the NCI core. Check with `zcat /proc/config.gz | grep NXP_NCI` before
building a device tree, because if they are not set the node will bind to
nothing and look like a hardware failure.

After flashing:

    i2cdetect -y -r 5              # bus number varies; find blsp1_i2c6 first
    dmesg | grep -i nci
    ls /sys/class/nfc/

A bound chip appears as `nfc0`. `neard` or `nfctool` from `neard-tools` will
then talk to it.
