# Upstream submission material

Work in this directory targets mainline Linux, not this phone. Nothing here
should be flashed: the device tree below deliberately leaves out the display,
audio and touch support that the working images carry, because those depend on
drivers that are not upstream yet. Flashing it would give you a phone with a
blank screen and no touch.

## Why sirius is worth submitting

Mainline already carries the other three shinano devices:

    qcom-msm8974pro-sony-xperia-shinano-aries.dtb    Xperia Z3 Compact
    qcom-msm8974pro-sony-xperia-shinano-castor.dtb   Xperia Z2 Tablet
    qcom-msm8974pro-sony-xperia-shinano-leo.dtb      Xperia Z3

sirius, the Xperia Z2, is the one sibling with no device tree in mainline under
any name. Checked against `arch/arm/boot/dts/qcom/Makefile`; both plausible
filenames return 404.

Everything the four devices share is already in
`qcom-msm8974pro-sony-xperia-shinano-common.dtsi`, which is why leo.dts is only
44 lines. That common file currently enables touch, eMMC and SD, Wi-Fi,
modem and ADSP remoteproc, keys, vibrator, USB and the regulators. It has **no
display, no GPU, no audio and no sensors**, so those four gaps are open for the
whole family, not just the Z2.

## Relationship to the board file

`../devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` is the tree the phone
actually boots — 455 lines, everything working, built on the same
`shinano-common.dtsi`. The file in this directory is a deliberate **subset**
of that: only the parts whose drivers and bindings are already upstream, so
that it compiles against mainline and can be submitted without depending on
anything out of tree.

Do not submit the board file. It references `sony,sirius-panel`, the MAX1187x
touch controller and the msm8974 sound card, none of which exist upstream yet.
Do not flash this one. Keep both, and keep the distinction explicit, or
somebody will send the wrong file.

Where the two disagree the board file wins: it has been booted and this has
not.

## qcom-msm8974pro-sony-xperia-shinano-sirius.dts

Written in the same shape as leo.dts: include the common dtsi, then only the
differences.

What it contains, and where each part came from:

| part | source | tested |
|---|---|---|
| model, compatible, chassis-type | convention from leo/aries | n/a |
| camera snapshot and focus keys | Sony's stock FOTA device tree, pm8941 gpio 3 and 4 | yes, on the working tree |
| `gpio_keys_pin_a` pins gpio2-5 | Sony's tree: vol_dn gpio2, snapshot 3, focus 4, vol_up 5 | yes |
| five sensors on `blsp2_i2c6` | our working tree, converted from phandles to labels | yes, all five read |
| `pm8941_lvs1` declaration | required by the sensors; common declares only lvs3 | yes |
| smbb charging values | Sony's stock tree for board-id `<0x08 0x00>` | not verified against a charge cycle |
| synaptics touchscreen disabled | the Z2 has a Maxim MAX1187x instead | yes |

Deliberately left out, with the reason:

- **Display.** Needs the panel driver, which is not upstream. Six panel
  variants exist for this device; see ../panel-variants/. This is the largest
  piece of follow-up work and benefits the whole family.
- **Touch.** The Z2's MAX1187x has no mainline driver and Sony's binding is
  nothing like a mainline one (see `touchscreen@48` in
  ../devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts: dozens
  of vendor properties, a nested wakeup-gesture tree). Submitting the DTS with
  an undocumented compatible would be rejected, so the inherited Synaptics node
  is simply disabled.
- **Audio.** Needs the msm8974 ASoC machine driver
  (../drivers/audio/msm8974-sndcard.c) and the q6afe change. Both are
  upstream candidates in their own right; see the submission order below.
  Headphones additionally need the WCD9320 codec, forward-ported from
  z3ntu's 5.11 driver, plus the PM8941 clock divider that feeds the codec
  its master clock.
- **GPU.** Works here only with a VRAM carveout on the kernel command line,
  because msm8974 has no GPU IOMMU support. Not a device tree matter alone.
- **Battery percentage.** Depends on two out-of-tree patches (0005, 0006 in
  ../drivers/battery/), which are themselves upstream candidates.

### How it was validated

Compiled against genuine mainline sources, not approximations. The include
chain was fetched from torvalds/linux master (22 files: the common dtsi,
qcom-msm8974pro.dtsi, qcom-msm8974.dtsi, pm8841/pm8941.dtsi and the
dt-bindings headers), then:

    cpp -nostdinc -I include -I arch/arm/boot/dts/qcom -undef \
        -x assembler-with-cpp qcom-msm8974pro-sony-xperia-shinano-sirius.dts
    dtc -I dts -O dtb -o sirius.dtb sirius.pre

It compiles. Every dtc warning comes from the existing mainline files
(pm8941.dtsi, qcom-msm8974.dtsi); none refers to this file. Decompiling the
result confirms the sensors land on i2c@f9968000 with `status = "okay"`,
interrupts 73, 66 and 74, supplies resolving to l17 and the newly declared
lvs1, the Synaptics node disabled, and the charging values applied.

What that does **not** prove: this exact tree has never been booted. The
working images use the decompiled leo tree, where the same sensor nodes are
verified on hardware. Before sending the patch, build it in a real kernel tree
and boot it, accepting that the screen will stay dark until the panel driver
lands.

### What needed no upstream change at all

The modem and GNSS work on a stock mainline kernel. Sony's modem firmware
stalls in its own initialisation until the application processor answers its
TA (trim area) requests over QMI, and the daemon that answers them,
`ta-service`, is userspace; a one-line fix to it is what finishes the job.
Nothing in the kernel had to change, and nothing about it is Z2 specific:
every Sony msm8974 phone carries the same TA partition. See ../modem/.

The FM tuner is likewise driven from userspace over Bluetooth HCI, and the
periodic corruption in its capture is repaired by an ALSA plugin
(../drivers/audio/fmrepair/). Neither belongs in the kernel.

### Submission order

Smallest and most independent first. The first item is not a kernel patch and
can go immediately:

1. **`ta-service` TA-block fix** to github.com/andersson/ta-service and to
   the pmaports package. One line, and it is what lets the modem finish
   starting on any Sony msm8974 phone. See ../modem/.
2. **q6afe fix** (both LPAIF clocks in one command) to alsa-devel. A bug fix,
   no new bindings, stands alone.
3. **This DTS** to linux-arm-msm, plus the Makefile line. Needs no new
   bindings: every compatible it uses is already documented.
4. **msm8974 sound card machine driver** to alsa-devel. It gives the Nexus 5
   and the Fairphone 2 the same speaker path, not only the Z2.
5. **PM8941 clock divider fix** to linux-arm-msm: one patch making
   `clk-spmi-pmic-div.c` take its output names from the device tree. Without
   it the driver cannot be used on msm8974 at all, because the RPM has
   already claimed the names it generates. See ../drivers/clk/pmic-clkdiv/.
6. **WCD9320 codec driver**, after the machine driver: headphones and
   microphones for the whole shinano family, and for every other msm8974
   phone carrying this codec. Playback works on hardware; capture is
   untested.
7. **Panel driver and binding** to dri-devel. See ../panel-variants/ and ../drivers/panel/.
8. **Display, audio and sensor nodes for the rest of the family** into
   shinano-common.dtsi, once the drivers they need are in.

The postmarketOS side is separate and can move immediately: `device-sony-sirius`
already exists in pmaports at `device/testing/device-sony-sirius` and already
depends on `linux-postmarketos-qcom-msm8974`, the exact kernel package this
phone runs.
