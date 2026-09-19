# Camera bring-up plan

For the current state and how to reproduce it, read `handover-camera.md`
first.

The staged plan for getting both Xperia Z2 cameras working on a mainline
kernel. `camera.md` has the hardware map and the prior art; this file is the
order of work, what each stage needs and how each is proven done.

## What is already known

**Sensors.** Rear: 20 MP with autofocus, **IMX200**, settled on 2026-09-19 by
reading the chip ID over CCI (register 0x0016 -> 0x0200, five identical
reads). The stock tuning file was right and the public Z2 specifications,
which say IMX220, are wrong for this device. Front: a Sony **IMX132**
(`LGI02BN1_IMX132.dat`, `SEM02BN1_IMX132.dat`, 2 MP, two module suppliers),
fixed focus. Neither part has a mainline Linux driver (IMX132 exists only in
the unusable `staging/media/atomisp`); `imx258`/`imx283` are the closest
templates and register sequences come from Sony's downstream CAF driver.

**Wiring**, from Sony's stock device tree:

| | rear (`sony_camera_0`) | front (`sony_camera_1`) |
|---|---|---|
| CCI master | 0 | 1 |
| I2C address | 0x20 (7-bit 0x10) | 0x6c (7-bit 0x36) |
| CSIPHY | 0 | 2 |
| data lanes | 4 (`csi-lane-mask` 0x1f) | 2 (`csi-lane-mask` 0x07) |
| MCLK GPIO | 15 | 17 |
| reset GPIO | 94 | 18 |
| vdig | pm8941 L3, 1.2 V | pm8941 L3, 1.2 V |
| vana | pm8941 L17, 2.7 V | pm8941 L17, 2.7 V |
| vio | pm8941 LVS2 | pm8941 LVS2 |
| vaf (autofocus) | pm8941 L23, 2.8 V | — |
| EEPROM | 0xa0, 2048 bytes | 0xa0, 1024 bytes |
| mount angle | 270° | 270° |

CCI data and clock lines are TLMM 19–22.

L17 at 2.7 V is the same rail the IIO sensors already use, so enabling it for
the cameras cannot conflict with them.

**CCI is already upstream.** `qcom-msm8974.dtsi` in 6.16 has a complete
`cci: cci@fda0c000` node with `compatible = "qcom,msm8974-cci"`, clocks,
pinctrl and both `cci_i2c0` / `cci_i2c1` buses, only `status = "disabled"`.
The `i2c-qcom-cci` driver matches it with its v1.5 data. The Nexus 5 "CCI hack"
is obsolete.

**CAMSS is not.** 6.16 camss has no msm8974 support, and its structure has
changed since the 5.17-era Nexus 5 patch: resources are now
`camss_subdev_resources` tables collected in a `camss_resources` per SoC. The
old patch cannot be applied; its content has to be re-expressed. camss still
depends on `IOMMU_DMA` and uses `videobuf2-dma-sg`, which is exactly what the
Nexus 5 patch had to change, because msm8974's camera subsystem has no IOMMU.

## Stage 1 — kernel configuration

Needs a kernel rebuild; everything else depends on it.

    CONFIG_MEDIA_SUPPORT=m
    CONFIG_MEDIA_CAMERA_SUPPORT=y
    CONFIG_MEDIA_PLATFORM_SUPPORT=y
    CONFIG_VIDEO_DEV=m
    CONFIG_V4L2_FWNODE=m
    CONFIG_VIDEO_QCOM_CAMSS=m      (after stage 3's Kconfig change)
    CONFIG_I2C_QCOM_CCI=m
    CONFIG_VIDEOBUF2_DMA_CONTIG=m  (stage 3 selects it; msm8974 has no camera IOMMU)

Config audit of the aport (2026-09-13, `config-postmarketos-qcom-msm8974.armv7`):
none of the above are set yet — `MEDIA_SUPPORT` and `I2C_QCOM_CCI` are
explicitly "not set", the rest absent. This is the **only** kernel-config work
the next stages need: NFC (`NFC_NXP_NCI`/`NFC_NCI`/`NFC_PN544`) and SLIMbus
(`SLIMBUS`, `SLIM_QCOM_NGD_CTRL`) are **already `=m` in the config**, so NFC and
the future WCD9320 audio path need no config change — only device tree (NFC) or
a codec driver (audio). So one media-stack addition to the next kernel build
unblocks the camera; fold it in with any other pending kernel change.

**Done when** `/dev/media0` can exist, i.e. the media core loads.

## Stage 2 — CCI bus and sensor identification

Device tree only, no driver work:

- `&cci { status = "okay"; }`
- the camera regulators (L3, L17, L23, LVS2) wired to placeholder nodes on
  `cci_i2c0` at 0x10 and `cci_i2c1` at 0x36, with MCLK and reset GPIOs
- MCLK from `CAMSS_MCLK0_CLK` / `CAMSS_MCLK2_CLK` at the sensor's rate

Then power the rails, release reset, and read the sensor ID registers and the
two EEPROMs with `i2ctransfer` on the CCI buses.

**Done when** both chip IDs read back: the front IMX132, and the rear part,
which the read decides between IMX200 and IMX220 (or corrects entirely). Save
the EEPROM contents.

## Stage 3 — CAMSS for msm8974

A patch to `drivers/media/platform/qcom/camss`:

- an `msm8974_resources` table: 3 CSIPHY, 4 CSID, 1 ISPIF, 2 VFE, using the
  msm8916 (`CAMSS_8x16`, VFE 4.1) code paths, with msm8974's register names,
  interrupts and MMCC clocks — the addresses are already confirmed twice
- `qcom,msm8974-camss` in the match table, and a binding entry
- a contiguous-memory buffer path: `videobuf2-dma-contig` when there is no
  IOMMU, rather than making camss depend on one — upstreamable, unlike
  replacing scatter-gather outright as the Nexus 5 hack did
- the ISPIF interrupt handler choice the Nexus 5 patch made (8x96 handler for
  this ISPIF v3.0)
- the camss node in `qcom-msm8974.dtsi`, with CMA sized for camera buffers
  on top of the GPU carveout

**Done when** `media-ctl -p` shows the full CSIPHY → CSID → ISPIF → VFE graph,
and a capture from a CSID test pattern generator produces frames.

## Stage 4 — front sensor driver (IMX132)

Front first: 2 MP, two lanes, no autofocus, so the smallest driver and the
quickest end-to-end proof.

- register sequences: Sony's stock userspace sensor stack is the source, the
  same way the upstream IMX300 driver was written from Xperia userspace
- a standard V4L2 sensor subdev: power sequence (rails, MCLK, reset), mode
  table, link frequency, exposure, analogue gain, test pattern
- device tree endpoint to CSIPHY 2

**Done when** `v4l2-ctl --stream-mmap` captures valid Bayer frames from the
front camera.

## Stage 5 — rear sensor driver (IMX200/IMX220)

Same method, larger: 20 MP, four lanes, plus the autofocus voice-coil actuator
on L23 (its own small I2C driver, part to be identified from Sony's tree) and
the flash LED. EEPROM calibration data (lens shading, AF) from stage 2.

**Done when** rear frames capture, focus can be driven, and the flash fires.

## Stage 6 — userspace

libcamera's `simple` pipeline handler supports Qualcomm camss with its
software ISP. Add tuning files from the EEPROM data, then test with `cam`,
then Snapshot or Megapixels on Phosh.

**Done when** a photo is taken from the phone's camera app.

## Order and dependencies

    1 kernel config ──► 2 CCI + identify ──► 4 front sensor ──┐
                    └─► 3 CAMSS ────────────────────────────┴─► 6 userspace
                                         5 rear sensor ─────┘

Stages 2 and 3 can proceed in parallel after stage 1. Stage 4 needs both.
Stage 5 reuses everything from 4.

The single blocker today is stage 1: this device's kernel is built with no
media support at all.
