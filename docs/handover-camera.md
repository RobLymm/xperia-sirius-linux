# Camera: handover

Last verified against the device on 2026-09-19.

Where the camera work has got to, what is proven, and what to do next. Read
`where-work-goes.md` first — it says which document holds which kind of fact,
and the rule about device trees that cost an evening.

## What is proven

**Both sensors are identified, from the silicon.** This was the open question
and it is closed:

| | rear | front |
|---|---|---|
| sensor | **Sony IMX200** | **Sony IMX132** |
| CCI master | 0 | 1 |
| I2C address | 0x10 | 0x36 |
| model ID register | 0x0016 -> 0x0200 | 0x0000 -> 0x0132 |
| EEPROM | 0x50, `"SOI20BS0"` `"IMX2000A"` | 0x50, `"SEM02BN1"` `"IMX132"` |
| autofocus | 0x0c, Rohm **BU64296G** | none |

Three independent sources agree for the rear: the stock tuning filename, the
sensor's own ID register, and the module EEPROM in ASCII. **The published
Xperia Z2 specifications say IMX220 and are wrong for this device.** Do not
"correct" this back without reading the EEPROM yourself.

**The CCI control bus works.** Mainline's `i2c-qcom-cci` binds to
`qcom,msm8974-cci`, both masters enumerate as ordinary i2c adapters, the
interrupt fires on every transfer, and the sensors answer.

## Reproducing it

The device tree needs: `cci@fda0c000` status okay; `CAMSS_MCLK0_CLK` and
`CAMSS_MCLK2_CLK` appended to that node's `clocks`; `cam_mclk0` on gpio15 and
`cam_mclk2` on gpio17 in its pinctrl state; and `regulator-always-on` on
`l3`, `l17`, `l23` **and `lvs2`**. `images/boot-cam-v2.img` has all of it.

The driver is built out of tree and installed at
`/lib/modules/6.16.12/updates/cci/`; its source is `~/cci-mod` on the phone,
copied from the kernel tree with a one-line Makefile.

Then:

    sudo modprobe i2c-qcom-cci
    echo on | sudo tee /sys/bus/platform/devices/fda0c000.cci/power/control
    sudo gpioset --hold-period=60s --chip gpiochip0 94=1 18=1 &
    B=$(ls -d /sys/bus/platform/devices/fda0c000.cci/i2c-* | head -1 | sed 's|.*/i2c-||')
    sudo i2ctransfer -y -f $B w2@0x10 0x00 0x16 r2     # -> 0x02 0x00

## Four traps, each of which makes a working bus look dead

1. **`lvs2` is not optional.** It is `vio`, the sensors' I/O supply, and it
   powers their I2C. Without it every transfer returns `Operation timed out`
   and the bus looks broken; with it you get a normal NAK. It has no node of
   its own in the tree and must be given one.
2. **The master clocks must already be running.** The CCI driver uses runtime
   PM and only enables its clocks for the duration of a transfer, but a
   sensor needs INCK running *before* it will answer. Hence pinning
   `power/control` to `on`. The clocks work when hung off the CCI node
   because the driver calls `devm_clk_bulk_get_all()` and enables everything
   it finds, ignoring names and order.
3. **The CCI adapter numbers move between boots** of the same image,
   depending on when the module loads relative to the QUP buses. Resolve them
   from sysfs every time. Scanning the wrong adapter finds the speaker
   amplifiers and the accelerometer and looks like a camera result.
4. **`i2cdetect` cannot probe these sensors.** One-byte read against 16-bit
   addressing: it reports an empty bus. Use `i2ctransfer` with an explicit
   register. It does find the EEPROMs and the actuator, which are byte
   addressed. Block reads (`r16`) are rejected by the controller — read two
   bytes at a time.

One more, for whoever writes the sensor driver: **the rear sensor reads zero
across the whole SMIA identity block at 0x0000-0x0004.** Code that checks the
conventional model-ID location will conclude the sensor is absent.

## The media core is built and loaded

**This needed no kernel rebuild and no flash**, which is the opposite of what
this page said before. Every piece of the media stack is tristate, and the
three things it needs built in — `DMA_SHARED_BUFFER`, `CMA`, `DMA_CMA` — the
running kernel already has, because the GPU carveout put them there. So it
builds out of tree against the kernel the phone is running, like the CCI
module.

Eleven modules are installed in `/lib/modules/6.16.12/updates/media/`: `mc`,
`videodev`, `v4l2-async`, `v4l2-fwnode`, `v4l2-dv-timings` and six
`videobuf2-*`. Six of them load on 2026-09-19 — the rest have no consumer yet
and `modprobe` will pull them in when one appears. The kernel reports

    mc: Linux media interface: v0.10
    videodev: Linux video capture interface: v2.00

and no unresolved symbol versions. There is still no `/dev/video*`, because
nothing registers one yet.

### Building them again

The kernel tree is `~/kbuild/linux-6.16.12` on the phone, prepared by
`~/kbuild/prep.sh`. The three tools are in `tools/` in this repository; copy
them to the phone's home directory. **Every make needs `LLVM=1`**: without it
`olddefconfig` re-detects GCC and silently drops `CONFIG_CFI_CLANG`, and GCC
modules will not load into this kernel. `kbuild-mod.sh` passes it for you.

    K=$HOME/kbuild/linux-6.16.12
    VB="CONFIG_VIDEOBUF2_CORE=m CONFIG_VIDEOBUF2_V4L2=m CONFIG_VIDEOBUF2_MEMOPS=m
        CONFIG_VIDEOBUF2_DMA_CONTIG=m CONFIG_VIDEOBUF2_DMA_SG=m
        CONFIG_VIDEOBUF2_VMALLOC=m"

    J=3 ./kbuild-mod.sh drivers/media/mc
    EXTRA_SYMVERS="$K/drivers/media/mc/Module.symvers" \
        J=3 ./kbuild-mod.sh drivers/media/v4l2-core
    EXTRA_SYMVERS="$K/drivers/media/mc/Module.symvers
                   $K/drivers/media/v4l2-core/Module.symvers" \
        J=3 ./kbuild-mod.sh drivers/media/common/videobuf2 $VB

`kbuild-mod.sh` builds one kernel directory as an external module, in two
passes: the first collects unresolved symbols with `KBUILD_MODPOST_WARN=1` and
fills them, the second is strict. Keep the order — mc, v4l2-core, videobuf2 —
because each uses the previous one's exports. The `videobuf2` config values
have to be given on the command line: they have no Kconfig prompt, and nothing
selects them until camss exists.

The `.ko` files land in the source directories. Copy them to
`/lib/modules/6.16.12/updates/media/` and run `depmod -a`.

Build at `J=3`. At `-j4` the phone reaches 83 °C, and the thermal governor
then clamps all four cores to 300 MHz, so the higher job count produces more
heat and no more speed.

### Module.symvers, which nothing else on this phone had

That kernel tree was never fully built, so it has no `Module.symvers`, and
`CONFIG_MODVERSIONS=y` means modpost rejects every vmlinux symbol an
out-of-tree module uses. Two tools solve it, and they are needed for **any**
module work on this phone, not just the camera:

- `~/harvest-symvers.py` reads the CRC every installed module recorded for the
  symbols it imports (`modprobe --dump-modversions`) and writes them as
  vmlinux entries. 564 modules give about 5,300 symbols. Symbols a module
  exports rather than vmlinux are dropped, found by reading
  `__ksymtab_strings` — `nm` does not show them.
- `~/fill-symvers.py <symbol>...` covers the rest: genksyms in this tree
  produces the same CRCs, so it builds whichever object exports the symbol and
  reads the `#SYMVER` lines out of its `.o.cmd`.

**The two agree exactly** — 18 symbols in common, 18 matches, no mismatch —
which is what makes the harvested file trustworthy.

**The trap that actually bit**, and the reason `fill-symvers.py` is more than
a grep: a symbol is often exported from several files, only one of which this
configuration builds, and taking the first is wrong *silently*.
`clk_round_rate` is exported by both `drivers/clk/clk.c` and
`drivers/sh/clk/core.c`; the SuperH one sorts first, `make` will build any
object you name whether or not the config wants it, and its CRC is
`0xca8cae5d` against the real `0x43f81957`. Nothing complains until the module
is loaded and the kernel says `disagrees about version of symbol
clk_round_rate`. `mm/nommu.c` against `mm/vmalloc.c` is the same shape.

So the tool accepts a candidate only if kbuild would build it: it walks from
the file's directory up to the tree root and checks each parent Makefile pulls
the child in with an `obj-` rule whose CONFIG is set — `drivers/Makefile` has
`obj-$(CONFIG_SUPERH) += sh/`, and that is what rejects the SuperH file. If
two accepted candidates still disagree it prints `AMBIG` and adds nothing.

One more: namespaced exports (`EXPORT_SYMBOL_NS_GPL(dma_buf_fd, "DMA_BUF")`)
must keep their namespace, or modpost stops checking `MODULE_IMPORT_NS`.

## The ISP works

msm8974 CAMSS was re-expressed for 6.16 and it runs: the driver probes clean,
enumerates 3 CSIPHY, 4 CSID, 4 ISPIF lines, 2 VFE and six video nodes, and
captures correct frames from CSID0's test pattern generator. The patches, the
reasoning, the two bugs that only showed up when it ran, and the capture
recipe are in `../drivers/camera/README.md`.

The phone is running `images/boot-camss-v1.img`, which is `boot-cam-v2.img`
plus the camss node. `qcom-camss.ko` is in
`/lib/modules/6.16.12/updates/media/` and `v4l-utils` is installed.

## What remains, in order

1. **An IMX132 driver** for the front camera, and then **an IMX200 driver**
   for the rear. This is the gate now, and the hard part is the data rather
   than the code: Sony's published kernel gives the power sequences exactly
   and no register or mode tables at all, because in this generation they
   lived in the userspace HAL. They have to come off the stock system
   partition. `prior-art.md` has what is and is not in Sony's source, and
   `camera.md` the sequences taken from it.
2. **Device tree for the sensor nodes** — supplies, clocks, resets and the
   CSI endpoints into CSIPHY 0 and 2. The ISP half is already in the tree.
   Add `vdda-supply = <&pm8941_l12>` to the camss node at the same time: the
   CSIDs fall back to a dummy regulator without it, which the test pattern did
   not care about and a real sensor will.
3. **The BU64296G actuator**, a simple I2C part and the smallest of the
   three drivers.

libcamera 0.7.2 and Snapshot are installed on the phone, and libcamera's
`simple` pipeline handler lists `qcom-camss`, so the userspace half is in
place; Phosh's camera app reports "No Camera Found" simply because there is
no `/dev/video*` for it to open.

## Before flashing anything

This is not optional, and ignoring it cost an evening with an unusable phone:

    strings -a boot.img | grep -c msm.vram=512m   # 1, else the boot stalls blank
    strings -a boot.img | grep -c max1187x        # 1, else there is no touchscreen
    tools/dt-equiv.py boot.img live.dtb           # differences must be only what you intended

Get `live.dtb` with `sudo cat /sys/firmware/fdt > live.dtb`. Never build a
device tree from anything in `/boot`, and never from
`dtc -I fs -O dts /proc/device-tree`; extract the DTB from a boot image known
to start. `images/boot-voice-v1.img` is a good base and
`images/boot-cam-v2.img` is that base plus the camera changes.
