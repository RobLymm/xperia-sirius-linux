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

## What remains, in order

1. **Kernel rebuild for the media stack.** `MEDIA_SUPPORT`, `VIDEO_DEV`,
   `V4L2_FWNODE`, `VIDEOBUF2_DMA_CONTIG`, `I2C_QCOM_CCI`. None are set. This
   gates everything else, including any V4L2 device for the camera app to
   open, and it is the only item here needing a kernel build rather than a
   module.
2. **msm8974 CAMSS for 6.16.** The only existing work is a 5.17-era Nexus 5
   patch; camss has since moved to per-SoC `camss_subdev_resources` tables,
   so it must be re-expressed rather than applied. msm8974 has no camera
   IOMMU, so `videobuf2-dma-contig` replaces the sg variant.
3. **An IMX200 driver.** None exists anywhere. **Start from Sony's published
   kernel**, `sonyxperiadev/kernel`,
   `drivers/media/platform/msm/camera_v2/sensor/` — see `prior-art.md`. The
   recent mainline IMX300 and IMX111 submissions are the closest structural
   templates in style.
4. **An IMX132 driver.** Exists only in the Intel-coupled
   `staging/atomisp`, which is not usable here.
5. **Device tree for the sensors and the ISP blocks** — CSIPHY, CSID, ISPIF,
   VFE, with the sensor nodes carrying supplies, clocks, resets and CSI
   endpoints. The hardware map is in `camera.md`.
6. **The BU64296G actuator**, a simple I2C part and the smallest of the
   three drivers.

libcamera is already installed on the phone, so the userspace half is in
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
