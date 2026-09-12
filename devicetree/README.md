# The device tree that runs

`qcom-msm8974pro-sony-xperia-sirius.dts` is what the phone boots: 455 lines of
labelled source, built on mainline's `qcom-msm8974pro-sony-xperia-shinano-common.dtsi`
with the Z2's own peripherals described on top. Display, audio, Bluetooth,
touch, sensors, battery and GPU all work on it.

It replaced a 3,423 line decompiled tree that used raw phandle numbers
(`<0x9a>`, `<0xcd>`) instead of labels. Beyond being unreadable, the flattened
form had a specific trap: changes to the SoC `.dtsi` could not reach it,
because nothing was referenced by label. Three real defects were hiding in it:

- `pm8941_lvs1` was never declared, though the sensors take their I/O supply
  from it and `shinano-common` does not define it
- the charging limits were the Xperia Z3's — 2150 mA into a 3200 mAh cell with
  a 3000 mA ceiling, where Sony specifies 1600/1600
- two multi-reference properties carried stale phandle numbers, which is what
  broke audio and Bluetooth on the first boot of the converted tree

The charging values were derived twice independently, once from Sony's stock
FOTA device tree and once during the conversion, and agree on all seven.

## The remaining Z3-ness

The `#include` itself. Everything board-specific is now the Z2's; the base is
Shinano because the Z2's own Rhine tree (`qcom-msm8974-sony-xperia-sirius.dts`
in the msm8974-mainline fork) does not boot — it hangs before USB comes up and
nobody has found out why. That is stated in the file header rather than hidden.

Thermal was checked and is not a concern: `shinano-common` has no thermal
nodes at all, the trip points come from the SoC dtsi and are generic msm8974
(75 °C passive, 110 °C critical), and both are more conservative than Sony's
own 80 °C throttle and 115 °C reset.

## Kernel command line

The GPU needs a VRAM carveout, because msm8974 has no GPU IOMMU support:

    cma=768M msm.vram=512m msm.allow_vram_carveout=1

With a smaller carveout the screen freezes once it fills. Applications
rendering on the GPU currently hang it; the compositor is stable.

## Building and flashing

`../tools/phone-build-img.sh` packs a kernel and a compiled device tree into a
boot image with `mkbootimg-osm0sis`. `../tools/update-latest.sh` compiles a
`.dts` on the phone, which has `dtc`, and brings the results back.

The phone boots the image flashed to the boot partition with fastboot.
Installing a kernel package updates `/boot/vmlinuz` and regenerates
`/boot/boot.img` and changes nothing about what the phone actually boots.
Rebuild and reflash after any kernel update.
