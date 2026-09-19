# Camera: the ISP is nearly solved, the sensors are not

Last verified against the device on 2026-09-19.

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
Xperia userspace, and the msm8996 IMX318 "first photo" write-up, with `imx258`
or the 2024 20 MP `imx283` as the closest structural templates.

## What Sony's published source gives, and what it does not

Read on 2026-09-19 from `sonyxperiadev/kernel`. The driver is
`sony_camera_v4l2.c` on branch `aosp/LNX.LA.3.5.1-01110-8x74.0` — **not** on
the newest 8x74 branch, which drops it — and the board data is
`arch/arm/boot/dts/msm8974pro-ab-shinano_sirius_common.dtsi`.

**There are no register or mode tables in it, and there are none in any
Qualcomm-era kernel.** `sony_camera_v4l2.c` is power sequencing and I2C
passthrough: nine commands, of which the only register write is streaming off.
The generic CAF drivers are the same — `imx135.c` is 236 lines of power
sequencing. Mode programming lived in the userspace HAL, so the IMX200 and
IMX132 mode tables have to come from the stock camera blobs
(`extracting-from-stock.md`), not from source. `prior-art.md` has the
correction in full.

What it *is* authoritative for, taken verbatim:

| | rear `sony_camera_0` | front `sony_camera_1` |
|---|---|---|
| fitted module | `SOI20BS0` | `SEM02BN1` (or `LGI02BN1`) |
| active pixels | 5248 × 3936 | 1976 × 1200 |
| unit cell | 1.20 µm | 1.12 µm |
| aperture | f/2.0 | f/2.8 |
| image circle | 7.87 mm | 2.59 mm |
| media bus code | `subdev_code = 0x3007`, i.e. `MEDIA_BUS_FMT_SBGGR10_1X10` | same |
| CSI lane mask | 0x1f — 4 data lanes + clock | 0x07 — 2 data + clock |
| lane assignment | 0x4320 | 0x4320 |
| PLL table | 18 entries, `600 318 386 684 318 578 293 388 597 599 318 596 599 474 599 825 578 578` | 18 entries, all 104 except entry 8 = 101 |

Both module part numbers match the EEPROM strings already read off the device,
so Sony's tree describes this exact handset.

**Sony's driver sets MCLK to 8 MHz; the phone runs 19.2 MHz and both sensors
work.** Both power-on steps are `CAM_CLK = <6 0 0 N>`, and value 0 means
`SENSOR_MCLK_DEFAULT` in `sony_camera_v4l2.c`, which is 8,000,000. The CCI
node here supplies 19.2 MHz (`camss_mclk0_clk` and `camss_mclk2_clk`, both
confirmed in `/sys/kernel/debug/clk`), and at that rate both sensors answer
and report sane PLL dividers. These are not necessarily in conflict — an IMX
sensor accepts a range of INCK and the PLL compensates — but which rate to
drive is an open question, and it changes every derived clock. Nothing has
streamed from a sensor yet.

**Power-on, rear**, in order, each with its delay in ms:

    CAM_VDIG  1.2 V   85 mA     wait 1
    CAM_VIO   (LVS2, no voltage) wait 1
    CAM_VANA  2.7 V  103 mA     wait 1
    CAM_VAF   2.8 V  106.5 mA   wait 1
    GPIO_RESET high             wait 1
    CAM_CLK   8 MHz             wait 10

**Power-off, rear**: write 0 to register **0x0100** and wait 100 ms, then
reset low, clock off, then VDIG, VIO, VANA, and VAF last with a 99 ms delay
after it.

**Power-on, front**: the same but with no VAF, and the final clock wait is 1 ms
rather than 10. **Power-off, front** drops the rails in the opposite order —
VANA, VIO, VDIG — with a 98 ms delay after VDIG.

That 0x0100 is the standard Sony/SMIA streaming-control register, which is
worth knowing given the rear sensor reads zero across the SMIA identity block:
the identity registers are non-standard, the streaming register is not.

## What the sensors say about themselves

Read over CCI on 2026-09-19 with the rails up, reset released and MCLK at
19.2 MHz. The defaults describe a complete, coherent full-resolution mode, and
they are the starting point for the geometry.

They are **not** sufficient to make either sensor stream. The MIPI
configuration lives in Sony's vendor register range and is zero at reset; for
the IMX132 those values are published in Intel's old atomisp driver, and
`prior-art.md` records what it holds. Nothing equivalent has been found for
the IMX200.

Both parts use the standard Sony/SMIA register map, and the geometry they
report matches Sony's device tree exactly.

| register | IMX200 (rear) | IMX132 (front) | meaning |
|---|---|---|---|
| 0x0112 | 0x0a0a | 0x0a0a | CSI data format: RAW10 in, RAW10 out |
| 0x0340 | 3984 | 1200 | frame length, lines |
| 0x0342 | 5904 | 2250 | line length, pixel clocks |
| 0x0344 / 0x0346 | 0, 0 | 0, 28 | crop start x, y |
| 0x0348 / 0x034a | 5247, 3935 | 1975, 1171 | crop end x, y |
| 0x034c / 0x034e | **5248, 3936** | **1976, 1144** | output size |
| 0x0380 | 1 | 1 | x increment: no binning or skipping |
| 0x0202 | 1000 | 800 | coarse integration time, lines |
| 0x0300 / 0x0302 | 10, 1 | 10, 1 | VT pixel and system clock dividers |
| 0x0306-7 | 110 | 45 | PLL multiplier |

`0x034c/0x034e` is the tell: 5248 x 3936 is exactly the rear
`pixel_number_w/h` in Sony's tree, and 1976 is exactly the front's. The crop
windows are self-consistent too — 5247 - 0 + 1 = 5248, and 1171 - 28 + 1 =
1144.

Taking the PLL at face value with a 19.2 MHz input gives plausible operating
points, which is the second sign the defaults are usable rather than
arbitrary:

    IMX200:  19.2 x 110 / 10 = 211.2 MHz pixel clock
             211.2e6 / (5904 x 3984)  =  ~9 fps at 20.7 MP
    IMX132:  19.2 x 45 / 10  =  86.4 MHz pixel clock
             86.4e6 / (2250 x 1200)   =  ~32 fps at 2.4 MP

Neither has been streamed, so treat the frame rates as arithmetic rather than
measurement. `tools/sensor-dump.sh` reads a range of registers over CCI; the
controller rejects block reads, so it goes two bytes at a time.

Two traps for whoever writes the drivers. The rear sensor keeps its model ID
at **0x0016**, not the conventional 0x0000, and reads zero across the whole
SMIA identity block — code that checks the usual place concludes the sensor is
absent. And the front keeps its ID at 0x0000 and reads zero at 0x0016, so each
looks dead at the other's register.

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
- `version`: **a new `CAMSS_8x74` is required, not optional.** This page used
  to call it a fallback behind reusing `CAMSS_8x16`; reading every site that
  branches on version shows msm8974 falls on different sides at different
  ones, so no existing value fits:

  | site | what it decides | msm8974 groups with |
  |---|---|---|
  | `camss.c` ~3587 | whether to allocate an ISPIF | 8x16/8x53/8x96 — it has one |
  | `camss-csiphy.c` ~607 | whether to map the CSIPHY `clk_mux` register | 8x16/8x53/8x96 — Sony's tree has it at fda00030/38/40 |
  | `camss-csid.c` ~635 | CSID source-pad format: 8x16 passes the sink code through, later parts remap | **8x16** — CSID 4.1 |
  | `camss-ispif.c` ~832, ~1113, ~1131, ~1165 | ISPIF register layout and interrupt handler | **8x96** — msm8974 is `ispif-v3.0` |

  Reusing `CAMSS_8x16` gives the wrong ISPIF handler; reusing `CAMSS_8x96`
  gives the wrong CSID format path. Add `CAMSS_8x74` to each of those
  conditions on the correct side.
- **CSIPHY type**: msm8974 uses the older 2-phase CSIPHY like 8x16
  (`camss-csiphy-2ph-1-0.c`), not the 3-phase one.
- **Dual VFE**: 8x16 ships `vfe_num = 1`, so the two-VFE path — exercised on
  8x96 — is new ground with `vfe_num = 2`.
- Carry z3ntu's no-IOMMU change in `camss-video.c`: `q->mem_ops =
  &vb2_dma_contig_memops` at ~717 and `vb2_dma_contig_plane_dma_addr()` in
  place of the `vb2_dma_sg_plane_desc()` scatter-gather walk at ~158. Also
  drop `depends on (ARCH_QCOM && IOMMU_DMA)` from `Kconfig` and select
  `VIDEOBUF2_DMA_CONTIG` instead of `VIDEOBUF2_DMA_SG`. **That dependency is
  why camss does not even appear in menuconfig today**: msm8974 sets neither
  `ARM_SMMU` nor `QCOM_IOMMU`, so `IOMMU_DMA` is off.

**Clocks: every one msm8974 needs is already in mainline's `mmcc-msm8974`**,
which removes the largest unknown. Mapping from the camss driver's clock names
to `dt-bindings/clock/qcom,mmcc-msm8974.h`:

| camss clock name | msm8974 binding |
|---|---|
| `top_ahb` | `CAMSS_TOP_AHB_CLK` |
| `ispif_ahb` | `CAMSS_ISPIF_AHB_CLK` |
| `csiphy0_timer` … `csiphy2_timer` | `CAMSS_PHY0_CSI0PHYTIMER_CLK` … `PHY2_CSI2PHYTIMER` |
| `csi0` … `csi3` | `CAMSS_CSI0_CLK` … `CSI3` |
| `csi0_ahb`, `csi0_phy`, `csi0_pix`, `csi0_rdi` | `CAMSS_CSI0_AHB_CLK`, `CSI0PHY`, `CSI0PIX`, `CSI0RDI` (×4) |
| `vfe0`, `vfe1` | `CAMSS_VFE_VFE0_CLK`, `VFE1` |
| `csi_vfe0`, `csi_vfe1` | `CAMSS_CSI_VFE0_CLK`, `VFE1` |
| `vfe_ahb`, `vfe_axi` | `CAMSS_VFE_VFE_AHB_CLK`, `VFE_AXI` |
| GDSC | `CAMSS_VFE_GDSC` |

One gap: the msm8916 tables list a plain **`ahb`** clock, and msm8974 has no
`CAMSS_AHB_CLK` — only `TOP_AHB` and `MICRO_AHB`. Decide what that maps to, or
leave it out, when writing the tables.

The media core is built and loads now (`camera-plan.md` stage 1), so this
compiles and can be iterated on the phone. Testing the ISPIF and dual-VFE
paths still needs hardware, so it is a build-and-iterate job, not a blind
patch.

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

The staged order of work is in `camera-plan.md` and is not repeated here. The
control bus, both sensor identities and the media core are settled; camss and
the two sensor drivers are not.

**Userspace is the one part that needs no new work.** libcamera's `simple`
pipeline handler with the software ISP is essentially the qcom-camss path —
SoftISP was first enabled for qcom-camss — and handles 8/10-bpp unpacked RAW
Bayer, enough for preview and stills. The libcamera 0.7.2 installed on the
phone lists `qcom-camss` among the drivers that handler accepts, and Snapshot
is installed. No msm8974 tuning exists, so the Z2 would be first.

Biggest risks, in the order they will be met:

- **Contiguous DMA without an IOMMU** needs a CMA reservation big enough for
  20 MP RAW10 buffers, on a phone whose command line already gives
  `cma=768M` and `msm.vram=512m` to the GPU. Sized, not yet tested.
- **Dual-VFE ISPIF routing** is the least-exercised code in camss: 8x16 ships
  one VFE, and msm8974 has two.
- **The rear sensor's register sequence** is the real unknown, and it is now a
  narrower one than it was. Which part it is, is settled. The power sequence
  is published by Sony and recorded above. What is missing is only the mode
  and register programming, which was never in any kernel and has to come off
  the stock system partition. The front IMX132 is the same problem, smaller.

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
