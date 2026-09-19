# Camera: the ISP is nearly solved, the sensors are not

The staged bring-up plan is in `camera-plan.md`.

Not working, not attempted on this device. But the hard-looking part turns out
to have been done by other people on other msm8974 phones, and the remaining
work is narrower and better defined than it first appears.

## The hardware

From Sony's stock device tree, the msm8974 CAMSS:

| block | count | addresses | size | IRQs | Sony's compatible |
|---|---|---|---|---|---|
| CSIPHY | 3 | fda0ac00, fda0b000, fda0b400 | 0x200 | 78, 79, 80 | qcom,csiphy |
| | | clk mux fda00030, fda00038, fda00040 | 0x4 | | |
| CSID | 4 | fda08000, fda08400, fda08800, fda08c00 | 0x100 | 51, 52, 53, 54 | qcom,csid |
| ISPIF | 1 | fda0a000, csi_clk_mux fda00020 | 0x500 / 0x10 | 55 | qcom,ispif-v3.0 |
| VFE | 2 | fda10000, fda14000 | 0x1000 | 57, 58 | **qcom,vfe40** |
| | | vbif fda40000, tcsr fd4a8000 | | | |
| CCI | 1 | fda0c000 | 0x1000 | 50 | qcom,cci |

Two sensors on the CCI bus:

    qcom,camera@20   rear   csiphy 0   4 rails inc. cam_vaf (autofocus)
    qcom,camera@6c   front  csiphy 2   3 rails, no autofocus

both with `qcom,mount-angle = <0x10e>`, 270 degrees.

Sony's stock system partition narrows the parts down: tuning files
`vendor/camera/SOI20BS0_IMX200.dat` (a 20 MP module) and
`LGI02BN1_IMX132.dat` / `SEM02BN1_IMX132.dat` (2 MP modules from two
suppliers), plus a leftover `libchromatix_imx135_liveshot.so`.

**Rear part number: settled, it is an IMX200.** Read over CCI from the sensor
itself on 2026-09-19, five identical reads:

    rear  0x10  register 0x0016  ->  0x0200   IMX200
    front 0x36  register 0x0000  ->  0x0132   IMX132

Confirmed independently by the module EEPROMs, which carry the part numbers
as plain ASCII:

    rear  0x50:  "SOI20BS0" ... "IMX2000A" ... "BU64296G"
    front 0x50:  "SEM02BN1" ... "IMX132"

Three lines of evidence agree for the rear — the stock tuning file name, the
sensor's own ID register, and the module EEPROM — and on both cameras the
module part number in the EEPROM matches the tuning filename exactly. The
public Xperia Z2 specifications, which say IMX220, are wrong for this device.
The `0A` after IMX200 is a revision suffix.

`BU64296G` is the Rohm voice-coil driver for the autofocus, which is the chip
answering at 0x0c on the rear bus.

Note the two sensors keep their model ID in different registers: the rear at
0x0016 reads 0 at 0x0000, and the front at 0x0000 reads 0 at 0x0016. The
rear also reads 0 across the whole SMIA identity block at 0x0000-0x0004, so
a driver that checks the standard location will conclude the sensor is
absent.

What else is on the two buses, from the same scan:

| | rear bus (CCI master 0) | front bus (CCI master 1) |
|---|---|---|
| sensor | 0x10 | 0x36 |
| EEPROM | 0x50-0x57 | 0x50-0x57 |
| autofocus actuator | 0x0c, a Rohm BU64296G | none, as expected |

No mainline driver exists for IMX200, IMX220, IMX132 or IMX135 (IMX132 appears
only in the old `staging/media/atomisp`, Intel-coupled and unusable here). The
nearest prior art is the IMX300 driver written by reverse-engineering Sony
Xperia userspace, and the msm8996 IMX318 "first photo" write-up. Register and
power sequences come from Sony's downstream CAF Android sensor driver
(`drivers/media/platform/msm/camera_v2/sensor/…`), with `imx258` or the 2024
20 MP `imx283` as the closest structural templates. Sony's device tree itself
only says `sony_camera_0` / `sony_camera_1`; each module has an EEPROM at 0xa0.

## What already exists

`z3ntu/linux`, branch `flto-msm8974-5.17-camera`, has msm8974 camera work for
the Nexus 5:

    a990f998b  [WIP] dts msm8974: add CAMSS         Luca Weiss, 2022
    051d2027d  [WIP] dts msm8974: add CCI bus
    be6e07586  [HACK] CCI driver for msm8974        (now obsolete, see below)
    5a36d2a51  media: camss: HACK for msm8974       Jonathan Marek, 2019
    2f441d048  media: imx179 HACK driver
    2a54eac74  [WIP] dts hammerhead configure rear camera

The two things worth knowing from it:

**They bind msm8974 to the msm8916 driver.** The device tree node uses
`compatible = "qcom,msm8916-camss"`. No new camss variant was written. That
matches the hardware: msm8974's VFE is `qcom,vfe40`, the generation camss
implements as `camss-vfe-4-1.c` for msm8916.

**CCI is no longer a hack — it is upstream in this kernel.** The
msm8974-mainline fork's `qcom-msm8974.dtsi` already has
`cci: cci@fda0c000 { compatible = "qcom,msm8974-cci"; ... status = "disabled";
}` with clocks, pinctrl and both `cci_i2c0` / `cci_i2c1` buses, and
`i2c-qcom-cci.c` matches `qcom,msm8974-cci` to its v1.5 data (line ~790).
So the be6e07586 CCI hack above is obsolete: CCI needs only `status = "okay"`
and the sensor child nodes, no driver work.

**Every address and interrupt in their node is identical to the table above**,
which was derived independently from Sony's tree here. Two independent
derivations agreeing is as close to confirmation as this gets without
hardware.

Jonathan Marek's camss patch is about 150 lines and does three things:

- drops the `IOMMU_DMA` dependency and switches from `VIDEOBUF2_DMA_SG` to
  `VIDEOBUF2_DMA_CONTIG`, because msm8974's camss has no IOMMU — the same
  constraint that forces the GPU onto a VRAM carveout on this SoC
- takes DMA addresses with `vb2_dma_contig_plane_dma_addr` instead of walking
  a scatter-gather table
- uses the 8x96 ISPIF interrupt handler (`ispif_isr_8x96`) for this ISPIF,
  consistent with msm8974 reporting `qcom,ispif-v3.0`

That branch is 5.17-era and selected its per-SoC resource structs with
`of_device_is_compatible()` in probe. Mainline 6.16 has since refactored to
`struct camss_subdev_resources` (id/hw_ops/formats embedded per entry)
aggregated into a per-SoC `struct camss_resources` chosen via `.data` in the
of_match table. So z3ntu's *data* (register names, clock lists, the 3 CSIPHY /
4 CSID / 1 ISPIF / 2 VFE counts, the ISPIF-8x96 routing, VFE 4.1 formats) is
directly reusable, but must be re-expressed as new `*_res_8974[]` tables plus
a `msm8974_resources` and a real `qcom,msm8974-camss` compatible with a
binding — cleaner than z3ntu's hack of binding to `qcom,msm8916-camss` and
overriding the tables. Mainline VFE ops include `vfe_ops_4_1` (what msm8916
uses); there is no `vfe_ops_4_0`, so 4.1 is the one to reuse. Estimated days,
not weeks: the hardware facts are all known.

## The device tree node

Adapted from the hardware table; the addresses match the Nexus 5 work exactly,
since it is the same SoC. This belongs in a shared msm8974 dtsi rather than a
per-device file.

```dts
camss: camss@fda00000 {
	compatible = "qcom,msm8916-camss";
	reg = <0xfda0ac00 0x200>, <0xfda00030 0x4>,
	      <0xfda0b000 0x200>, <0xfda00038 0x4>,
	      <0xfda0b400 0x200>, <0xfda00040 0x4>,
	      <0xfda08000 0x100>, <0xfda08400 0x100>,
	      <0xfda08800 0x100>, <0xfda08c00 0x100>,
	      <0xfda0a000 0x800>, <0xfda00020 0x10>,
	      <0xfda10000 0x1000>, <0xfda14000 0x1000>;
	reg-names = "csiphy0", "csiphy0_clk_mux",
		    "csiphy1", "csiphy1_clk_mux",
		    "csiphy2", "csiphy2_clk_mux",
		    "csid0", "csid1", "csid2", "csid3",
		    "ispif", "csi_clk_mux",
		    "vfe0", "vfe1";
	interrupts = <GIC_SPI 78 0>, <GIC_SPI 79 0>, <GIC_SPI 80 0>,
		     <GIC_SPI 51 0>, <GIC_SPI 52 0>,
		     <GIC_SPI 53 0>, <GIC_SPI 54 0>,
		     <GIC_SPI 55 0>,
		     <GIC_SPI 57 0>, <GIC_SPI 58 0>;
	interrupt-names = "csiphy0", "csiphy1", "csiphy2",
			  "csid0", "csid1", "csid2", "csid3",
			  "ispif", "vfe0", "vfe1";
	/* clocks, power-domains and ports still to be filled in */
};
```

The CPP and CCI blocks are missing from it, as their version notes.

### Stage 3 implementation spec (6.16 camss)

Confirmed from `drivers/media/platform/qcom/camss/camss.{c,h}` in the fork.
Model everything on the msm8916 tables (`csiphy_res_8x16`, `csid_res_8x16`,
`ispif_res_8x16`, `vfe_res_8x16`, aggregated in `msm8916_resources`).

- Fill one `struct camss_subdev_resources` per block —
  `{ regulators[], clock[], clock_for_reset[], clock_rate[][], reg[],
  interrupt[], union { csiphy | csid | vfe } }`. `reg[]` and `interrupt[]` are
  the reg-names / interrupt-names strings the driver looks up, so they must
  match the DT node (`csiphy0`, `csid0`, `ispif`, `vfe0`, …).
- Aggregate into a `msm8974_resources` (`struct camss_resources`) with
  `version`, `pd_name`, the four `*_res` tables, and the counts
  **`csiphy_num = 3`, `csid_num = 4`, `vfe_num = 2`** (ISPIF is a single
  struct, not counted). Add `qcom,msm8974-camss` to the of_match `.data`.
- `version`: reuse **`CAMSS_8x16`** (VFE 4.1, the `vfe_ops_4_1` path z3ntu
  used) rather than a new enum, but three behaviours are keyed on version and
  need checking against msm8974: (1) the **ISPIF interrupt handler** — z3ntu
  switched to `ispif_isr_8x96`; (2) **dual VFE** — 8x16 ships `vfe_num = 1`, so
  the two-VFE path (present for 8x96) must be exercised with `vfe_num = 2`;
  (3) **CSIPHY type** — msm8974 uses the older 2-phase CSIPHY like 8x16
  (`camss-csiphy-2ph-1-0.c`), not the 3-phase one. If 8x16's version gates any
  of these wrong for two VFEs, a `CAMSS_8x74` enum is the fallback.
- Carry z3ntu's no-IOMMU change in `camss-video.c`
  (`vb2_dma_contig_*` instead of `vb2_dma_sg_*`) — msm8974 has no camera IOMMU.

This needs a media-configured kernel build to compile and hardware to test the
ISPIF/dual-VFE paths, so it is a build-and-iterate job, not a blind patch.

## Device tree for stage 2 (verified references)

Everything here was checked against the msm8974-mainline fork sources and
Sony's stock tree, so it is a starting point, not a guess — but it is untested
(the kernel has no media/CCI config yet, and there is no sensor driver, so the
nodes will not probe until stage 4).

- **Enable CCI:** `&cci { status = "okay"; };`. Both buses (`cci_i2c0`,
  `cci_i2c1`) and their pins (`cci_default`: gpio19/20 `cci_i2c0`, gpio21/22
  `cci_i2c1`) are already in `qcom-msm8974.dtsi`.
- **MCLK:** rear gpio15 function `cam_mclk0`, clock `CAMSS_MCLK0_CLK` (mmcc 77);
  front gpio17 function `cam_mclk2`, clock `CAMSS_MCLK2_CLK` (mmcc 79). Add
  pinctrl states for these (they are single-pin `cam_mclk0`/`cam_mclk2`
  functions in `pinctrl-msm8x74.c`).
- **Reset:** rear gpio94, front gpio18, both as plain `gpio` function,
  `reset-gpios = <&tlmm 94 GPIO_ACTIVE_LOW>` / `<&tlmm 18 GPIO_ACTIVE_LOW>`.
- **Rails** (labels exist in shinano-common unless noted):
  `vdig` = `pm8941_l3` (1.2 V), `vana` = `pm8941_l17` (2.7 V),
  `vaf` = `pm8941_l23` (2.8 V, rear only). `vio` is Sony's **LVS2**, and
  `pm8941_lvs2` is **not defined** in this tree (only `pm8941_lvs3` is) — it
  must be added to the pm8941 `regulators` node (group `vdd_l2_lvs1_2_3` on
  `pm8941_s3`). `pm8941_lvs1` is likewise referenced by the board but check it
  is defined before reusing.
- **I2C addresses:** rear sensor 0x20 (7-bit 0x10) on `cci_i2c0`, front 0x6c
  (7-bit 0x36) on `cci_i2c1`; each EEPROM at 0xa0 (7-bit 0x50).

Bare identification without a sensor driver needs the rails, MCLK and reset
brought up by hand — mainline sensor drivers manage their own supplies on
probe, so with no driver the rails stay off and the part will not ACK. Either
a throwaway stub i2c driver that enables them, or a temporary regulator
consumer in the DT, is needed to read the chip ID with `i2ctransfer`.

## What is actually left

**The sensors.** Mainline has no driver for either part. `drivers/media/i2c`
carries imx111, 208, 214, 219, 258, 274, 283, 290, 296, 319, 334, 335, 355,
412, 415, 471 and 678 — nothing that matches a 2014 Sony flagship. The Nexus 5
work needed a hand-written IMX179 driver for exactly this reason, and it is
marked HACK.

So the order is:

1. Enable CCI (`&cci { status = "okay"; }`) and add the two sensor nodes on
   `cci_i2c0` (rear, 0x20) and `cci_i2c1` (front, 0x6c). Node and driver are
   already in this fork; no forward-port needed. Needs `CONFIG_I2C_QCOM_CCI`.
2. Power the rails and read the sensor ID registers over CCI with
   `i2ctransfer` to confirm the two parts — IMX200 rear and IMX132 front on
   current evidence — and save the two EEPROMs (0xa0).
3. Bring up camss against `qcom,msm8916-camss` with Jonathan Marek's patch
   (contiguous DMA, no IOMMU; 8x96 ISPIF handler) forward-ported from 5.17 to
   6.16, where the resource tables are now `camss_subdev_resources` collected
   in a per-SoC `camss_resources`. Needs `CONFIG_VIDEO_QCOM_CAMSS` and the
   media stack, which this kernel is built without (the single blocker:
   camera-plan.md stage 1).
4. Write or adapt a driver for each sensor.

5. Userspace: libcamera's `simple` pipeline handler with the software ISP is
   essentially the qcom-camss path (SoftISP was first enabled for qcom-camss),
   and handles 8/10-bpp unpacked RAW Bayer — enough for preview and stills. No
   msm8974 tuning exists, so the Z2 would be first.

Steps 1 to 3 are porting and wiring; step 1 is now just device tree. Step 4 is
the real work, and it is per sensor. None of it is speculative any more, which
is the difference between this and where the assessment started.

Biggest risks: the no-IOMMU contiguous DMA needs a CMA reservation big enough
for 20 MP RAW10 buffers (the same VRAM/CMA pressure the GPU already has); the
dual-VFE ISPIF routing is the least-tested camss code; and the rear sensor's
power and register sequence is the main reverse-engineering unknown (and which
part it even is — see the IMX200/IMX220 question above). The front IMX132 is a
later, separate effort.

## Bringing the bus up by hand, and four traps

Everything above was read with no camera driver at all: the CCI node enabled,
the two master clocks hung off it, the rails held on, and the resets released
from userspace. What it took, in order, because each of these was a dead end
until it was right:

1. **All four rails, `lvs2` included.** `l3` (1.2 V digital), `l17` (2.7 V
   analogue) and `l23` (2.8 V focus) are not enough. `lvs2` is `vio`, the
   sensors' I/O supply, and it powers their I2C interface: without it every
   transfer returns `Operation timed out` and the bus looks broken. With it,
   transfers return a normal NAK and the bus can be scanned. `lvs2` has no
   node of its own in the tree and has to be given one.
2. **The master clocks have to be running before a transfer.** The CCI driver
   uses runtime PM and only enables its clocks for the duration of a
   transfer, and a sensor needs its INCK running before it will answer at
   all. Pin the bus awake:

       echo on > /sys/bus/platform/devices/fda0c000.cci/power/control

   The clocks are hung off the CCI node deliberately: the driver calls
   `devm_clk_bulk_get_all()` and enables everything it finds, without
   consulting names or positions, so clocks added at the end are simply
   turned on with the bus.
3. **The bus numbers move.** The CCI adapters are not a fixed `i2c-1` and
   `i2c-2`: the numbering depends on when the module loads relative to the
   QUP buses, and it changed between two boots of the same image. Resolve
   them every time:

       ls -d /sys/bus/platform/devices/fda0c000.cci/i2c-*

   Scanning the wrong adapter finds the speaker amplifiers and the
   accelerometer and looks like a camera result.
4. **`i2cdetect` cannot probe these sensors.** It uses a one-byte read and
   they are 16-bit addressed, so it reports nothing on a perfectly working
   bus. Use `i2ctransfer` with an explicit register:

       i2ctransfer -y -f <bus> w2@0x10 0x00 0x16 r2

   It does find the EEPROMs and the actuator, which are byte addressed, so a
   scan is still worth running once the bus is alive.

Resets are TLMM 94 (rear) and 18 (front), released high, held from userspace
with `gpioset --hold-period=… --chip gpiochip0 94=1 18=1`. A block read
(`r16`) is rejected by the controller; read two bytes at a time.
