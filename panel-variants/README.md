# All six Xperia Z2 panel variants, extracted

Sony shipped the Z2 with six panel configurations: two driver ICs by three
glass vendors. A device only ever runs one of them, but the stock kernel
carries all six and picks between them at boot, so the data for panels you do
not own is sitting on your phone. That is what this directory holds.

Source: the FOTAKernel partition (mmcblk0p16), an ELF with three embedded
DTBs. The matching one is model "SoMC Sirius ROW", `qcom,board-id = <0x08 0x00>`.
Decompiled as ../fota-dtb0.dts; the six nodes are extracted here verbatim.

| file | driver IC | glass | lcd-id-adc window (uV) |
|---|---|---|---|
| renesas_sharp_1080p_panel.dts | 0x00 Renesas | Sharp | 0 - 57,000 |
| renesas_auo_1080p_panel.dts | 0x00 Renesas | AUO | 215,000 - 256,000 |
| renesas_jdi_1080p_panel.dts | 0x00 Renesas | JDI | 353,000 - 414,000 |
| novatek_jdi_1080p_panel.dts | 0x01 Novatek | JDI | 1,087,000 - 1,231,000 |
| novatek_sharp_1080p_panel.dts | 0x01 Novatek | Sharp | 1,236,000 - 1,395,000 |
| novatek_auo_1080p_panel.dts | 0x01 Novatek | AUO | 1,420,000 - 1,594,000 |

## Identifying which one a phone has

The bootloader passes `lcdid_adc` on the kernel command line. It is in **ADC
units; Sony's windows above are in microvolts, three times larger**. Compare
them directly and you get a confident wrong answer:

    lcdid_adc = 384552          raw, lands inside the renesas_jdi window
    x3        = 1,153,656       correct, lands inside novatek_jdi

This unit is novatek_jdi, confirmed on hardware:

    panel-sony-sirius: lcdid_adc=384552 (1153656 uV): jdi novatek 1080p video
    panel-sony-sirius: panel ID bytes: 1a a1 00

A hardcoded panel compatible is therefore wrong for most of these devices.
Runtime detection from lcdid_adc is the only correct approach, which is what
../driver/panel-sony-sirius.c does.

## What differs between them

All six are 1080x1920, 24bpp, DSI video mode, non-burst sync event, 4 lanes,
64 x 114 mm. Everything else splits along two independent axes:

**The glass vendor sets the display mode.**

| glass | h front / back / pulse | v front / back / pulse | PHY timing word 2 |
|---|---|---|---|
| Sharp | 128 / 76 / 4 | 2 / 5 / 2 | 0x686c2a3a |
| JDI | 112 / 76 / 4 | 27 / 4 / 4 | 0x686c2a3c |
| AUO | 104 / 56 / 20 | varies, see below | 0x686c2a3c |

AUO is the exception that does not factor cleanly: renesas_auo is 24/20/10
vertical, novatek_auo is 6/46/2. The other two glass vendors give identical
timings on both ICs. The remaining PHY timing words are
`0xe6382600 ... 0x2c030400` for all six.

**The driver IC sets the init sequence.**

| variant | init sequence, 32-bit cells |
|---|---|
| renesas_auo | 44 |
| renesas_jdi | 136 |
| renesas_sharp | 140 |
| novatek_auo | 286 |
| novatek_sharp | 3115 |
| novatek_jdi | 3223 |

Renesas init is short: bulk gamma writes via 0x29 long writes to C7/C8/C9/D3.
Novatek init is long: hundreds of 0x23 two-byte indexed register writes with
page switching via FF.

Power-on reset sequences differ too, and not by IC: Sharp glass uses
`<0x00 0x3c 0x01 0x0a>`, JDI and novatek_auo use `<0x01 0x0a>`, and renesas_auo
uses a six-value sequence `<0x01 0x0a 0x00 0x01 0x01 0x0a>`.

CABC (content-adaptive backlight) properties appear only on the two Sharp
variants.

Each variant also carries its own `somc,mdss-dsi-pcc-table`, Sony's per-panel
colour correction data — a few hundred entries mapping panel calibration
readings to RGB gain triples. Nothing in mainline consumes it today, but it is
the only source of factory colour calibration for these screens, and it is
per-variant, so it is worth keeping with the rest rather than discarding.

### Correction to ../PANEL-HANDOVER.md

That file says the mode is "IDENTICAL for both driver IC variants - verified".
That is true for the pair it was comparing, renesas_jdi and novatek_jdi, and
the table above shows why: the glass sets the mode, so two JDI panels match
whichever IC drives them. It does not generalise. Sharp and AUO panels have
different porches, and anyone reusing one mode for all six will get a rolling
or mispositioned image on four of them.

## Shared by all six

    on:  DCS 0x11 EXIT_SLEEP_MODE   delay 120ms
         DCS 0x29 SET_DISPLAY_ON
    off: DCS 0x28 SET_DISPLAY_OFF   delay 20ms
         DCS 0x10 ENTER_SLEEP_MODE  delay 80ms

Qualcomm command encoding: dtype, last, vc, ack, wait_ms, dlen_hi, dlen_lo,
then payload. dtype 0x05 = DCS short no param, 0x15 = DCS short one param,
0x23 = generic short two param, 0x29 = generic long.

GPIOs, from the mdss_dsi controller node: reset pm8941 gpio19, enable pm8941
gpio20, tear effect tlmm gpio12, driver IC tlmm gpio26.

Regulators, two levels and easy to confuse. The DSI host takes vdda
pm8941_l2, vdd pm8941_lvs3 and vddio pm8941_l12. The panel node itself takes
vddio pm8941_lvs3 and vsp from vreg_vsp, the LCD DCDC regulator that pm8941
gpio20 enables. See ../sirius-display.dtsi.

## Wider than the Z2

The shinano family (Z2 sirius, Z3 leo, Z3 Compact aries, Z2 Tablet castor)
shares this panel arrangement and these suppliers, and none of the four has
display support in mainline. Extraction from another device's FOTAKernel
follows the same method, and the resulting table can be compared against this
one to see how much is common across the family.

## Reproducing the extraction

    # on the phone, from the FOTAKernel partition
    dd if=/dev/mmcblk0p16 of=fota.elf
    # carve out the embedded DTBs (look for d00dfeed magic), then
    dtc -I dtb -O dts -o fota-dtb0.dts fota-dtb0.dtb

The six nodes were then cut out by brace matching on
`somc,*_1080p_panel`, and the comparison tables above generated from the
property values rather than read by eye.
