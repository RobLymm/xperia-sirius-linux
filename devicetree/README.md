# The device tree that runs

`qcom-msm8974pro-sony-xperia-sirius.dts` is what the phone boots: 529 lines of
labelled source, built on mainline's `qcom-msm8974pro-sony-xperia-shinano-common.dtsi`
with the Z2's own peripherals described on top. Display, audio, Bluetooth,
touch, sensors, battery and GPU all work on it.

The file follows the kernel's device tree coding style: `&label` overrides in
alphabetical order, bus children in unit-address order, lowercase hex, and
`dt-bindings` macros (`IRQ_TYPE_EDGE_FALLING`, `GPIO_ACTIVE_LOW`,
`QUATERNARY_MI2S_RX`, `VADC_VBAT_SNS`) in place of the raw numbers the
decompiler produced. The style pass changed no hardware description. The
compiled result was compared with the tree that booted: the same 509 nodes,
the same property names and the same values, with phandle cells matched by the
node they point to. The comparison was tested by changing one interrupt number
and one phandle target, and it reported both. The tidied file was then booted on
the phone (2026-09-13). The tree the kernel received matched the build, apart
from the `chosen` properties and the memory size that the bootloader writes,
and display, speakers, Bluetooth, touch, sensors, battery, GPU and Wi-Fi all
came up.

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

The board file `#include`s kernel headers, so it has to go through the C
preprocessor with the kernel's include paths before `dtc` sees it.
`../tools/build-board-dtb.sh <kernel tree> <name>` does that: `cpp` on the
build machine, `dtc` on the phone, which has it. An unpatched
`v6.16.12-msm8974` tree is enough for the board file; SoC-level patches, such
as CPU frequency scaling, are picked up if they are applied to that tree.
`../tools/update-latest.sh` runs `dtc` without `cpp`, so it only works on
self-contained trees and cannot build this file.

`../tools/phone-build-img.sh` then packs the kernel and the DTB into a boot
image with `mkbootimg-osm0sis`.

The phone boots the image flashed to the boot partition with fastboot.
Installing a kernel package updates `/boot/vmlinuz` and regenerates
`/boot/boot.img` and changes nothing about what the phone actually boots.
Rebuild and reflash after any kernel update.

## Keeping it in line

### One source

This file is the only source for the board. Anything specific to the Z2
goes in it.

- **SoC-level changes** go into the kernel tree's `qcom-msm8974.dtsi` as a
  kernel patch, not into this file. This file receives them through its
  `#include` chain when it is built from the patched tree. CPU frequency
  scaling is done this way.
- **Experimental variants** should `#include
  "qcom-msm8974pro-sony-xperia-sirius.dts"` and change things through labels,
  not copy the file. A copy stops receiving fixes made here. Once a variant
  is proven on the phone, fold it into this file and delete the variant.
- **`../upstream/`** holds the subset intended for mainline and has never been
  booted. When a change here uses a mainline binding, make it there as well.
- **Decompiled trees** (phandle numbers instead of labels) are obsolete. SoC
  `.dtsi` changes cannot reach them. Do not edit them or build new work on
  them.

### Style

The kernel's `Documentation/devicetree/bindings/dts-coding-style.rst`, applied
as follows:

- the root node first, then `&label` overrides in alphabetical order of label
- children of a bus in ascending unit address; nodes without an address in
  alphabetical order
- inside a node: `compatible`, `reg`, `ranges`, standard properties, vendor
  properties (`qcom,`, `maxim,`), then a blank line and `status` last; a blank
  line before each child node
- lowercase hex for addresses and register values; decimal for quantities
  (microvolts, microamps, the capacity table)
- a `dt-bindings` macro wherever a header defines one: `IRQ_TYPE_*`,
  `GPIO_ACTIVE_*`, `PM8941_GPIO_S3`, `KEY_*`, `APR_DOMAIN_*`, `APR_SVC_*`, the
  q6afe and q6asm DAI ids, `VADC_*`. Check the macro's value in the header
  before substituting it; a macro with the wrong value compiles silently.
- nodes named by function, with the unit address equal to `reg`:
  `touchscreen@48`, `amplifier@34`, `accelerometer@18`, `dai@22` for
  `reg = <QUATERNARY_MI2S_RX>`
- each reference in a multi-reference property in its own brackets:
  `<&speaker_left>, <&speaker_right>`. Writing `<&speaker_left 0xce>` compiles
  and breaks audio; that mistake cost a flash during the conversion.
- properties of an out-of-tree driver that break the naming rules, such as the
  touch driver's underscores, stay as the driver expects, with a comment
  saying why
- a value taken from Sony's stock tree gets a comment or a note in this README
  saying where it came from

The build currently gives 14 `dtc` warnings, all from the included SoC and
common files. A change that adds a warning should say why.

### Checking a change

1. **A change that should not alter the hardware description** (reordering,
   macros, renaming labels): build the DTB before and after, then

       ../tools/dt-equiv.py before.dtb after.dtb

   It must report 0 differences. Pass `--rename after=before` for a node you
   renamed on purpose. A byte comparison is the wrong tool here, because
   reordering renumbers phandles; `dt-equiv.py` matches a phandle by the node
   it points to.
2. **A change to the hardware description**: the same command must list the
   nodes and properties you meant to change and nothing else.
3. **After flashing**: confirm the phone runs what you built.

       ssh phone 'sudo cat /sys/firmware/fdt' > live.dtb
       ../tools/dt-equiv.py boot.img live.dtb

   It must report 0 differences. `/chosen` and `/memory` are ignored because
   the bootloader writes them.
4. **Then the hardware**: display connected, the sound card present, `hci0`
   up, the touchscreen input device, the IIO sensors, battery capacity,
   `/dev/dri/renderD128`, Wi-Fi connected.

The comparison in step 1 cannot prove a new mistake is harmless; it can only
show that nothing changed. The first conversion of this tree passed every
static check and still broke audio and Bluetooth. Treat the boot in steps 3
and 4 as the test. If you change `dt-equiv.py`, first make a copy of the
source with one number changed and one phandle pointed at a different node,
and confirm it reports both.
