# Camera: the ISP is nearly solved, the sensors are not

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
suppliers), plus a leftover `libchromatix_imx135_liveshot.so`. So the rear is
most likely an IMX200 and the front an IMX132, with the front module sourced
from two vendors. The sensor ID registers will confirm once the CCI bus works.
No mainline or out-of-tree driver was found for IMX200, IMX132 or IMX135; the
nearest prior art is the IMX300 driver written by reverse-engineering Sony
Xperia userspace sensor drivers. Sony names them
`sony_camera_0` and `sony_camera_1` rather than giving part numbers, so
identifying the actual sensors is a prerequisite. There is an EEPROM at 0xa0
that should answer it on hardware.

## What already exists

`z3ntu/linux`, branch `flto-msm8974-5.17-camera`, has msm8974 camera work for
the Nexus 5:

    a990f998b  [WIP] dts msm8974: add CAMSS         Luca Weiss, 2022
    051d2027d  [WIP] dts msm8974: add CCI bus
    be6e07586  [HACK] CCI driver for msm8974
    5a36d2a51  media: camss: HACK for msm8974       Jonathan Marek, 2019
    2f441d048  media: imx179 HACK driver
    2a54eac74  [WIP] dts hammerhead configure rear camera

The two things worth knowing from it:

**They bind msm8974 to the msm8916 driver.** The device tree node uses
`compatible = "qcom,msm8916-camss"`. No new camss variant was written. That
matches the hardware: msm8974's VFE is `qcom,vfe40`, the generation camss
implements as `camss-vfe-4-1.c` for msm8916.

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
- uses the 8x96 ISPIF interrupt handler for 8x16, consistent with msm8974
  reporting `qcom,ispif-v3.0`

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

## What is actually left

**The sensors.** Mainline has no driver for either part. `drivers/media/i2c`
carries imx111, 208, 214, 219, 258, 274, 283, 290, 296, 319, 334, 335, 355,
412, 415, 471 and 678 — nothing that matches a 2014 Sony flagship. The Nexus 5
work needed a hand-written IMX179 driver for exactly this reason, and it is
marked HACK.

So the order is:

1. Identify the two parts. Sony's abstraction hides them; the EEPROM at 0xa0
   and the sensor ID registers will say, once the CCI bus is up.
2. Get CCI working, which needs the msm8974 CCI hack forward-ported.
3. Bring up camss against `qcom,msm8916-camss` with the patch above
   forward-ported from 5.17 to current.
4. Write or adapt a driver for each sensor.

Steps 1 to 3 are porting and wiring. Step 4 is the real work, and it is per
sensor. None of it is speculative any more, which is the difference between
this and where the assessment started.
