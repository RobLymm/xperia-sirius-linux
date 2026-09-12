# Xperia Z2 (sirius) panel data — extracted from Sony's stock kernel

Source: FOTAKernel partition (mmcblk0p16) on the device, ELF, three embedded DTBs.
Matching DTB: model "SoMC Sirius ROW", qcom,board-id = <0x08 0x00>.

## CORRECTED: this unit is JDI glass on a NOVATEK driver IC

An earlier version of this file said "jdi renesas". That was wrong, and the
mistake is worth recording because it is easy to repeat:

**`lcdid_adc` from the bootloader is in ADC units. Sony's `somc,lcd-id-adc`
ranges are in MICROVOLTS, three times larger.** Compare them directly and you
get a false match.

    bootloader passes   lcdid_adc = 384552   (ADC units)
    scaled              x3        = 1153656  (microvolts)

Sony ships SIX variants here, two driver ICs x three glass vendors:

    somc,renesas_sharp_1080p_panel   ic=0x00          0 -    57,000
    somc,renesas_auo_1080p_panel     ic=0x00    215,000 -   256,000
    somc,renesas_jdi_1080p_panel     ic=0x00    353,000 -   414,000   <-- raw 384,356 falsely lands here
    somc,novatek_jdi_1080p_panel     ic=0x01  1,087,000 - 1,231,000   <-- CORRECT: scaled 1,153,656
    somc,novatek_sharp_1080p_panel   ic=0x01  1,236,000 - 1,395,000
    somc,novatek_auo_1080p_panel     ic=0x01  1,420,000 - 1,594,000

Confirmed on hardware. The working driver logs:

    panel-sony-sirius: lcdid_adc=384552 (1153656 uV): jdi novatek 1080p video
    panel-sony-sirius: panel ID bytes: 1a a1 00

Implication for anyone porting this: a hardcoded panel compatible is wrong for
most of these devices. Mainline's qcom-msm8974-sony-xperia-sirius.dts declares
`sharp,ls052t3sx02`, which matches none of the six by name. Runtime detection
from lcdid_adc is the only correct approach.

## Init sequences

The two driver ICs need completely different init:

  - Renesas (ic=0x00): 9 commands, 174 bytes, bulk gamma writes via 0x29 long
    writes to registers C7/C8/C9/D3. See jdi-panel-node.dts.
  - Novatek (ic=0x01): ~380 commands, almost all 0x23 two-byte indexed register
    writes, with page switching via FF. See novatek-jdi-panel-node.dts.
    THIS IS THE ONE THIS DEVICE USES.

Both share the same on/off commands:

    on:  DCS 0x11 EXIT_SLEEP_MODE  delay 120ms ; DCS 0x29 SET_DISPLAY_ON
    off: DCS 0x28 SET_DISPLAY_OFF  delay 20ms  ; DCS 0x10 ENTER_SLEEP_MODE delay 80ms

Qualcomm command format: dtype, last, vc, ack, wait_ms, dlen_hi, dlen_lo, payload.
dtype 0x05 = DCS short (0 param), 0x15 = DCS short (1 param),
0x23 = generic short (2 param), 0x29 = generic long.

## Mode (identical for the two JDI variants — but NOT across all six)

All six variants have since been extracted and compared; see
variants/README.md. The mode below is the JDI one. It is identical on both
driver ICs because the *glass vendor* sets the timings, not the IC — so the
statement here holds for renesas_jdi and novatek_jdi, and does not generalise.
Sharp glass uses 128/76/4 horizontal and 2/5/2 vertical, AUO uses 104/56/20.
Reusing this mode for all six gives a rolling image on four of them.

    1080 x 1920 @ 60Hz, 24bpp, DSI video mode, non-burst sync event, 4 lanes
    physical size 64 x 114 mm        (somc,mdss-phy-size-mm = <0x40 0x72>)

    h-front-porch 112 (0x70)   h-back-porch 76 (0x4c)   h-pulse-width 4
    v-front-porch  27 (0x1b)   v-back-porch   4         v-pulse-width 4
    h-sync-pulse 1, borders all 0

    qcom,mdss-dsi-panel-timings = <0xe6382600 0x686c2a3c 0x2c030400>
    t-clk-pre = 0x2b (43)   t-clk-post = 0x02
    qcom,mdss-dsi-lp11-init          (needs LP11 before init)
    qcom,mdss-dsi-tx-eot-append
    backlight: bl_ctrl_wled, min 1, max 0xfff

## Renesas init sequence (NOT used by this device - kept for the other variants)

Qualcomm format per command: dtype, last, vc, ack, wait_ms, dlen_hi, dlen_lo, payload.
dtype 0x05 = DCS short (0 param), 0x23 = generic short (2 param), 0x29 = generic long.

    0x05  [01]                                          delay 5ms   # SOFT RESET
    0x23  [b0 00]                                                   # MCAP unlock
    0x23  [00 00]
    0x23  [00 00]
    0x29  [c7 08 12 1b 24 31 48 40 52 5e 61 6a 76
               08 12 1b 24 31 48 40 52 5e 61 6a 76]                 # gamma A (24 bytes)
    0x29  [c8 08 12 1b 24 31 49 41 53 5e 61 6a 76
               08 12 1b 24 31 49 41 53 5e 61 6a 76]                 # gamma B
    0x29  [c9 08 12 1b 24 31 47 3f 52 5e 61 6a 76
               08 12 1b 24 31 47 3f 52 5e 61 6a 76]                 # gamma C
    0x29  [d3 1b 33 bb cc c4 33 33 33 00 01 00 a0 d8 a0 06
               2b 33 33 22 70 02 2b 43 3d bf 99]                     # power/timing
    0x23  [d6 01]

Then on-command (qcom,mdss-dsi-on-command = <0x5010000 0x78000111 0x5010000 0x129>):

    DCS 0x11  EXIT_SLEEP_MODE   delay 120ms (0x78)
    DCS 0x29  SET_DISPLAY_ON

Off (qcom,mdss-dsi-off-command = <0x5010000 0x14000128 0x5010000 0x50000110>):

    DCS 0x28  SET_DISPLAY_OFF   delay 20ms (0x14)
    DCS 0x10  ENTER_SLEEP_MODE  delay 80ms (0x50)

Command state: on = dsi_lp_mode (send init in LP), off = dsi_hs_mode.
somc,mdss-dsi-disp-on-in-hs = <0x01>

## Power / reset timing

    somc,pw-on-rst-seq  = <0x01 0x0a>     # reset level 1, 10ms
    somc,pw-off-rst-seq = <0x00 0x00>
    somc,disp-en-on-pre  = <0x05>   # 5ms
    somc,disp-en-on-post = <0x0f>   # 15ms
    somc,disp-en-off-post= <0x46>   # 70ms
    somc,dric-gpio = <TLMM 26>             # driver-IC GPIO

## GPIOs (phandles resolved against Sony's DTB)

From the mdss_dsi controller node:

    qcom,platform-reset-gpio  = <PM8941_GPIO 19>   panel reset
    qcom,platform-enable-gpio = <PM8941_GPIO 20>   panel enable
    qcom,platform-te-gpio     = <TLMM 12>          tear effect
    somc,dric-gpio            = <TLMM 26>          driver IC

phandle 0x28 = PM8941 gpio block (compatible "qcom,qpnp-pin")
phandle 0x05 = SoC TLMM        (compatible "qcom,msm-gpio", gpio@fd510000)

Current state on the running device (leo tree, display off), for reference:

    pm8941 gpio19  in  low   vin-0  pull-down    (reset, unconfigured)
    pm8941 gpio20  out low   vin-2  no pull      (enable, held off)
    tlmm   gpio12  in  low   func0  pull down    (TE)
    tlmm   gpio26  in  high  func0  pull up      (driver IC)

NOTE on PM8941 GPIO power-source: pins driven by this board use vin-2, which is
PM8941_GPIO_S3 and encodes as power-source = <0x02> in a decompiled DTB. Getting
this wrong is not theoretical - the Bluetooth BT_REG_ON pin sat on vin-0 and the
chip never powered on until a pinctrl state put it on vin-2.

## DSI lane config (somc,mdss-dsi-lane-config, 45 bytes)

    00 c2 ef 00 00 00 00 01 75   x4   (one per lane)
    00 02 45 00 00 00 00 01 97        (clock lane)

## Regulators (from mainline sirius.dts, already correct)

    vdda-supply = pm8941_l2 ; vdd-supply = pm8941_lvs3 ; vddio-supply = pm8941_l12

## Raw files

    fota-dtb0.dts                 full decompiled Sony DTB (10159 lines)
    novatek-jdi-panel-node.dts    THIS DEVICE's panel node (71 lines)
    jdi-panel-node.dts            the renesas variant (70 lines), not used here
