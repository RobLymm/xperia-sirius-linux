# Auditing this port against Sony's published source

Sony runs an Open Devices programme and publishes the kernel its Xperias
shipped with. For this phone that source is the authority on values, sequences
and orderings: it is what the hardware was built and tested against. It is
**not** a model for structure — it is 3.4-era vendor code and the drivers here
target 6.16 — so every entry asks "is our number, order or timing the same as
Sony's, and if not, why", never "should we look more like this".

`prior-art.md` says how to read the source and which branch to read. Every
finding below cites a file, a branch and a commit, so it can be re-checked:

    tools/sony-src.sh drivers/misc/pn547.c

Outstanding work that comes out of these findings is in `whats-left.md`;
confirmed faults are in `known-problems.md`. This file is the evidence.

## Status of each finding

| | |
|---|---|
| **Confirmed on device** | reproduced by a measurement, before and after |
| **From source** | read in Sony's tree, not yet tested here |
| **Agrees** | checked and the same — recorded so it is not re-checked or "fixed" |

---

## NFC — the PN547

Audited 2026-09-19 against `drivers/misc/pn547.c` and
`arch/arm/boot/dts/msm8974pro-ab-shinano_sirius.dtsi`, branch
`aosp/LNX.LA.3.5.2.2-03010-8x74.0` at `ba9f9c5d`. Ours is the proposed node in
`nfc.md`, which has never been flashed.

### VEN polarity is the opposite of what our node says — From source

`nfc.md` records this as "the one thing to verify", and Sony's driver settles
it. The device tree says `nxp,ven = <&pm8941_mpps 2 0x01>`, and `0x01` is
`OF_GPIO_ACTIVE_LOW`. The driver reads that flag into `ven_gpio_flags` and then
**never uses it**: every access goes through the legacy `gpio_set_value()` and
`gpio_set_value_cansleep()`, which are raw and apply no polarity.

    gpio_direction_output(pn547_dev->ven_gpio, 0);   /* probe: held in reset */
    ...
    gpio_set_value(pn547_dev->ven_gpio, 1);          /* enable, then scan I2C */
    usleep_range(10000, 11000);
    for (addr = 0x28; addr < 0x2C; addr++) { ... }
    gpio_set_value(pn547_dev->ven_gpio, 0);          /* back to reset */

and the same in the `PN547_SET_PWR` ioctl: `1` to power on, `0` to power off.
So electrically **the line is driven high to enable the chip**, which is the
usual arrangement for a PN547 and the opposite of what the active-low flag
suggests. The flag is dead code.

Our node has `enable-gpios = <&pm8941_mpps 2 GPIO_ACTIVE_LOW>`, taken from
Sony's third cell. Under the gpiod API that inverts the line, so asserting the
descriptor would drive it low and hold the chip in reset.

**Change** `GPIO_ACTIVE_LOW` to `GPIO_ACTIVE_HIGH` before flashing. `nfc.md`
already says to try the inversion first if no chip appears; this says which way
round to start, and why.

### PVDD has no home in our node at all — From source

Sony's node has `nxp,pvdd_en = <&pm8941_gpios 34 0x01>`, and the driver
requests that GPIO. Our proposed node drops it, because the mainline
`nxp,nci.yaml` binding has no property for it — the binding has `enable-gpios`
and `firmware-gpios` and nothing else.

Dropping it leaves the chip with no supply. Sony's base configuration makes
this worse rather than better: in the family file
`msm8974pro-ab-shinano_common.dtsi`, PM8941 **GPIO_33 and GPIO_34 are both
commented `NC` and set `qcom,master-en = <0>`** — disabled. Only GPIO_35,
`NFC_CLK_REQ`, is enabled there. The pins are brought up per-variant.

**Write it as a regulator**, which is the mainline-shaped answer and gives the
node something to reference:

```dts
nfc_pvdd: regulator-nfc-pvdd {
	compatible = "regulator-fixed";
	regulator-name = "nfc_pvdd";
	gpio = <&pm8941_gpios 34 GPIO_ACTIVE_HIGH>;
	enable-active-high;
	regulator-always-on;
};
```

Polarity here is a guess and should be checked the same way VEN was, by reading
what drives the line rather than what the flag says.

### There are two board revisions, wired differently — From source

`dynamic_config` in Sony's node sends probe through
`board_nfc_hw_lag_check()`, and `configure_gpio = <&pm8941_gpios 33 ...>` and
`configure_mpp = <&pm8941_mpps 2 ...>` are the alternate wiring it selects.
So some Z2 hardware revisions drive NFC differently, and a node that works on
one unit is not proof for all of them. Worth knowing before this is published
as working.

---

## Speaker amplifiers — the two TFA9890s

Audited 2026-09-19 against `msm8974pro-ab-shinano_common.dtsi` at `ba9f9c5d`,
against `devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts`.

### The I2C addresses agree — Agrees

Sony has `tfa98xx_top@68` and `tfa98xx_bottom@6A` on `i2c@f9967000`; ours has
`amplifier@34` and `amplifier@35` on `blsp2_i2c5`. These are the same two
parts: Sony writes the 8-bit address including the read/write bit, mainline
writes the 7-bit address. `0x68 >> 1 = 0x34` and `0x6A >> 1 = 0x35`.

Recorded because the numbers look like a disagreement and are not. Do not
"fix" this.

### Sony calls them top and bottom; we call them left and right — From source

`0x34` is Sony's **top** and our `speaker_left`; `0x35` is Sony's **bottom**
and our `speaker_right`. The README already says the earpiece is the top
TFA9890, which is `0x34`.

Top-to-left is the right mapping for a phone rotated anticlockwise into
landscape, so this is probably correct, but it has never been checked by ear.
One stereo file with a known channel settles it.

---

## MHL — a SiI8620, and mainline has a driver for it

**From source.** `msm8974pro-ab-shinano_common.dtsi` at `ba9f9c5d`:

```dts
sii8620@72 {
	compatible = "qcom,mhl-sii8620";
	reg = <0x72>;
	mhl-intr-gpio = <&msmgpio 64 0>;
	mhl-pwr-gpio  = <&msmgpio 23 0>;
	mhl-rst-gpio  = <&msmgpio 16 0>;
	mhl-switch-sel-1-gpio = <&msmgpio 10 0>;
	mhl-switch-sel-2-gpio = <&msmgpio 11 0>;
	mhl-fw-wake-gpio = <&msmgpio 31 0>;
	qcom,hdmi-tx-map = <&mdss_hdmi_tx>;
};
```

on the same I2C bus as the amplifiers, with `&pm8941_mvs2` commented
`VREG_HDMI`.

This identifies the part, which was the thing blocking the item. Mainline has
a **Silicon Image SiI8620 DRM bridge driver**, `sil-sii8620`, so video out over
the micro-USB port is a device tree and bridge-plumbing job rather than a new
driver. Still large — it has to attach to the DSI output — but no longer
unknown. In `whats-left.md`.

---

## Thermal — Sony's trip points, and one that already agrees

**From source.** `qcom,msm-thermal` in `msm8974pro-ab-shinano_common.dtsi`:

| Sony | Value | Ours |
|---|---|---|
| `qcom,limit-temp` | 80 °C | 80 °C CPU trip, in `0014-ARM-dts-qcom-msm8974-cpu-trip-80C.patch` — **agrees** |
| `qcom,core-limit-temp` | 85 °C | nothing |
| `qcom,freq-mitigation-value` | 422400 kHz | nothing |
| `qcom,core-control-mask` | `0x6` — cores 1 and 2 | nothing |
| `qcom,freq-mitigation-control-mask` | `0x09` — cores 0 and 3 | nothing |

The 80 °C figure was chosen here independently and Sony picked the same one,
which is a useful confirmation. What is missing is everything that happens
*after* the trip: Sony throttles to 422.4 MHz on cores 0 and 3, and takes
cores 1 and 2 offline at 85 °C. `todo.md` leaves "whether thermal trips are
sensible under sustained load" open; this is the vendor's answer.

---

## FM — Sony's tree confirms it is not Qualcomm's, and has a driver for what it is

**From source.** Two separate confirmations, on two branches.

On `ba9f9c5d`, `msm8974pro-ab-shinano_common.dtsi` deletes the entire Qualcomm
wireless connectivity subsystem:

    /delete-node/ qcom,iris-fm;
    /delete-node/ qcom,pronto@fb21b000;
    /delete-node/ qcom,wcnss-wlan@fb000000;
    /delete-node/ &smdtty_apps_fm;

`qcom,iris-fm` is the Qualcomm FM tuner. Sony deleted it because this phone
does not have one — Wi-Fi and Bluetooth are Broadcom here, and so is FM. That
settles a question `fm-broadcom.md` answered by experiment: the device tree
agrees. It also confirms that `drivers/fm/radio-wcnss-fm.c` in this repository
is for other msm8974 phones and can never work on this one, which its own
README already says.

On `aosp/LA.UM.5.5.r1` at `4bc2f4cd` there is a complete Broadcom FM stack:

    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_main.c
    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_v4l2.c
    drivers/bluetooth/broadcom/v4l2_fm_driver/fmdrv_rx.c
    drivers/bluetooth/broadcom/include/fm.h
    drivers/bluetooth/broadcom/line_discipline_driver/brcm_hci.c

This is the vendor implementation of the tuner this phone actually has, driven
over HCI exactly as `fm-broadcom.md` describes doing from userspace. It has not
been read yet. The reason to read it is the 41.6 Hz sign-bit corruption on the
I2S capture, which is currently repaired by the `fmrepair` ALSA plugin rather
than fixed: if Sony's stack sets a clock role or a PCM configuration the
userspace HCI sequence here does not, the cause goes away. In `whats-left.md`.

---

## Panel — where the variant identifier comes from

**From source.** `msm8974pro-ab-shinano_common.dtsi` configures PM8941 MPP 6:

    /* MPP_6: LCD_ID_ADC */
    mpp@a500 {
    	qcom,mode = <4>;         /* AIN */
    	qcom,ain-route = <1>;    /* AMUX 6 */
    	qcom,master-en = <1>;
    };

The panel driver here selects between six variants from an `lcdid_adc=` value
on the kernel command line, put there by the bootloader. This is the hardware
it is read from: an analogue input on PM8941 MPP 6, routed to AMUX 6.

That matters for two reasons. It is an independent confirmation that the
variant really is identified by an ADC reading rather than by anything else,
and it is the route to reading the value in-kernel through
`qcom-spmi-vadc` instead of trusting a command line the bootloader writes.
Not a defect; a better foundation, if the command line ever proves unreliable.

---

## Codec — the microphone bias configuration

**From source.** `taiko_codec` under `&slim_msm`:

| Property | Value |
|---|---|
| `qcom,cdc-micbias-ldoh-v` | `0x3` |
| `qcom,cdc-micbias-cfilt1-mv` | 2700 |
| `qcom,cdc-micbias-cfilt2-mv` | 2700 |
| `qcom,cdc-micbias-cfilt3-mv` | 2700 |
| `qcom,cdc-micbias1-cfilt-sel` | `0x0` |
| `qcom,cdc-micbias2-cfilt-sel` | `0x1` |
| `qcom,cdc-micbias3-cfilt-sel` | `0x2` |
| `qcom,cdc-micbias4-cfilt-sel` | `0x0` |

All three filters at 2700 mV, and each of the four biases assigned to a
filter. Read against the WCD9320 audit when it is done: the secondary
microphone is on AMIC1, whose bias takes cfilt 0, shared with AMIC4 — the
handset microphone that does work. Shared bias is worth knowing about before
concluding that a silent microphone is unpowered.
