# Camera ISP: msm8974 CAMSS

Last verified against the device on 2026-09-19.

Two patches that teach mainline's `qcom/camss` driver about msm8974. The
subsystem state, the hardware map and the sensor work are in
[`../../docs/camera.md`](../../docs/camera.md); this file is about the
patches.

**The module builds and loads; the driver has never bound to anything.**
`qcom-camss.ko` links clean against 6.16.12 out of tree, loads, and registers
at `/sys/bus/platform/drivers/qcom-camss` — but there is no device for it
until the camss node is in a flashed image, so no probe path has run. Treat
every claim below about the hardware as reasoned from the register maps, not
observed.

| | |
|---|---|
| `0001-media-camss-add-msm8974-support.patch` | the driver: resource tables, a new SoC version, and a buffer path that works without an IOMMU |
| `0002-ARM-dts-qcom-msm8974-add-the-camss-node.patch` | the node, disabled by default; a board enables it |

## Why msm8974 needs its own version enum

The obvious move is to reuse `CAMSS_8x16`: msm8974's blocks are the same
generation as msm8916's — 2-phase CSIPHY, CSID 4.1, VFE 4.1 — and that is
what z3ntu's Nexus 5 work did, binding to `qcom,msm8916-camss` outright.

It is wrong, and quietly. camss branches on `res->version` in four places,
and msm8974 does not fall on the same side at all of them:

| site | what it decides | msm8974 goes with |
|---|---|---|
| `camss.c`, allocating the ISPIF | whether there is an ISPIF at all | 8x16/8x53/8x96 |
| `camss-csiphy.c`, `base_clk_mux` | whether to map the CSIPHY clock mux register | 8x16/8x53/8x96 |
| `camss-csid.c`, `csid_src_pad_code` | 8x16 passes the sink code through, later parts remap it | **8x16** |
| `camss-ispif.c`, ×4 | ISPIF register layout, line count and interrupt handler | **8x96** |

msm8974's ISPIF reports `qcom,ispif-v3.0` in Sony's tree and has four CSIDs,
so it needs `ispif_isr_8x96` and `line_num = 4`. Reusing `CAMSS_8x16` gives it
the 8x16 handler and two lines; reusing `CAMSS_8x96` gives it the wrong CSID
format path. Hence `CAMSS_8x74`, added on the correct side of each.

## The buffer path

msm8974's camera subsystem has no IOMMU — the SoC sets neither `ARM_SMMU` nor
`QCOM_IOMMU` — which is the same constraint that puts the GPU on a VRAM
carveout. Two consequences:

- `VIDEO_QCOM_CAMSS` is excluded from the build entirely by
  `depends on (ARCH_QCOM && IOMMU_DMA)`. It does not appear in menuconfig on
  this SoC. The patch drops that dependency.
- The VFE takes one address per plane, so it cannot be handed a
  scatter-gather list. z3ntu's patch replaced scatter-gather outright, which
  would break every SoC that does have an IOMMU.

Instead the allocator is chosen from the device rather than the SoC:

    if (device_iommu_mapped(video->camss->dev))
            q->mem_ops = &vb2_dma_sg_memops;
    else
            q->mem_ops = &vb2_dma_contig_memops;

with the matching branch in `video_buf_init()`. That is a property of the
hardware, not a quirk table, and should be acceptable upstream.

Contiguous buffers come from CMA, and 20 MP RAW10 frames are large: the
phone's command line already carries `cma=768M` alongside `msm.vram=512m` for
the GPU. Whether that is enough for a full-resolution queue is untested.

## Clocks

Every clock the resource tables name is already in mainline's
`mmcc-msm8974`; none had to be added. Two differences from msm8916 worth
knowing:

- msm8974 has **no plain `CAMSS_AHB_CLK`**. msm8916's tables list an `"ahb"`
  clock and msm8974 has only `CAMSS_TOP_AHB_CLK` and `CAMSS_MICRO_AHB_CLK`,
  so there is no `"ahb"` entry here. If a block turns out to need one,
  `TOP_AHB` is the candidate.
- Both VFEs share a single `CAMSS_VFE_GDSC`, so there is no per-VFE
  `has_pd`/`pd_name` as on msm8953; the power domain goes on the camss node,
  as on msm8916. The domain is registered and visible as `camss_vfe` in
  `pm_genpd_summary`.

VFE rates are `ftbl_camss_vfe_vfe0_1_clk`, CSI and CSIPHY-timer rates are
100 and 200 MHz.

## Building it

The media core has to be built first — see
[`../../docs/handover-camera.md`](../../docs/handover-camera.md), which also
covers the `Module.symvers` tooling any out-of-tree module on this phone
needs. Then:

    EXTRA_SYMVERS="$K/drivers/media/mc/Module.symvers
                   $K/drivers/media/v4l2-core/Module.symvers
                   $K/drivers/media/common/videobuf2/Module.symvers" \
      J=3 ./kbuild-mod.sh drivers/media/platform/qcom/camss \
          CONFIG_VIDEO_QCOM_CAMSS=m

## What has been checked, and what has not

Checked:

- `qcom-camss.ko` links with no unresolved symbols, loads, and registers its
  platform driver, pulling in the media core as dependencies.
- The device tree change produces exactly ten differences against the tree
  before it — the camss node and its `ports`, the `lvs2` regulator, the two
  camera MCLK pin states, `regulator-always-on` on `l3` and `l23`, and the
  `cci` node's clocks, clock-names and status — and nothing else.
- Addresses and interrupts agree across three independent derivations:
  Sony's published `msm8974-camera.dtsi`, the stock device tree read off the
  device, and z3ntu's Nexus 5 work.

Not checked, because it needs a flash:

- that the driver probes at all — nothing past `platform_driver_register` has
  executed;
- that the ISPIF and dual-VFE paths behave — 8x16 ships one VFE, so two is
  new ground;
- that `media-ctl -p` shows the CSIPHY → CSID → ISPIF → VFE graph;
- that a CSID test pattern produces frames.

The first flash is worth doing before either sensor driver is written: it
turns the whole of the rest of the camera work from theory into something
testable.

## The image to flash

`images/boot-camss-v1.img`, also on the phone at `~/boot-camss-v1.img`. It is
**the image the phone is already running with the camss node added and nothing
else changed**, which is why it was built by editing that image's device tree
rather than from source: the repository cannot yet rebuild the running tree.
Building the codec variant from the board file and patch 0012 still lands 87
differences and 35 nodes short of what the phone runs — thermal trip points,
remoteproc power domains and interconnect clocks that came from kernel patches
not published here. A source-built image would therefore be a regression, so
this one reuses `boot-cam-v2.img`'s own kernel, ramdisk and command line
byte for byte.

What was checked before handing it over:

    tail of the kernel is exactly cam-v2.dtb                     cmp: equal
    ramdisk, cmdline, vmlinuz after repack                       all equal
    strings -a boot-camss-v1.img | grep -c msm.vram=512m         1
    strings -a boot-camss-v1.img | grep -c max1187x              1
    dt-equiv.py boot-camss-v1.img live.dtb    604 nodes, 2 differences
                                              /soc/camss@fda0ac00 and its ports

and inside the node, read back out of the built blob: 31 clocks and the
power domain all resolving to `/soc/clock-controller@fd8c0000`, power-domain
index 3 (`CAMSS_VFE_GDSC`), 14 reg ranges, 10 interrupts, and `status =
"okay"` — the SoC patch leaves it disabled, and this image is what a board
file enabling it would produce.

The backup taken from the boot partition **before** flashing, as the rules
require, is `images/boot-backup-before-camss.img`. Its first 17803264 bytes
are byte-identical to `boot-cam-v2.img`, which independently confirms what the
phone was running.

`qcom-camss.ko` is already installed at
`/lib/modules/6.16.12/updates/media/`, so after flashing:

    sudo modprobe qcom-camss
    dmesg | tail -40
    media-ctl -p          # if v4l-utils is installed

The interesting question is whether probe gets through the clocks and the VFE
GDSC. Expect it not to work first time.
