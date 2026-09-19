# Camera ISP: msm8974 CAMSS

Last verified against the device on 2026-09-19.

Two patches that teach mainline's `qcom/camss` driver about msm8974. The
subsystem state, the hardware map and the sensor work are in
[`../../docs/camera.md`](../../docs/camera.md); this file is about the
patches.

**They work.** On an Xperia Z2 the driver probes clean, `media-ctl` shows the
full CSIPHY → CSID → ISPIF → VFE graph, and a capture from CSID0's test
pattern generator produces correct frames. What has not been tested is a real
sensor, because no sensor driver exists yet.

| | |
|---|---|
| `0001-media-camss-add-msm8974-support.patch` | the driver: resource tables, a new SoC version, and a buffer path that works without an IOMMU |
| `0002-ARM-dts-qcom-msm8974-add-the-camss-node.patch` | the node, disabled by default; a board enables it |

## What it enumerates

    3 CSIPHY      msm_csiphy0-2
    4 CSID        msm_csid0-3
    4 ISPIF lines msm_ispif0-3
    2 VFE         msm_vfe0_rdi0-2, msm_vfe1_rdi0-2
    6 video nodes msm_vfe{0,1}_video{0,1,2}  ->  /dev/video0-5
                  plus /dev/media0 and 17 subdevs

The four ISPIF lines are worth noticing: they are the direct evidence that
msm8974 belongs with msm8996 and not msm8916 for the ISPIF, because msm8916's
code path gives two.

## Why msm8974 needs its own version enum

The obvious move is to reuse `CAMSS_8x16`: msm8974's blocks are the same
generation as msm8916's — 2-phase CSIPHY, CSID 4.1, VFE 4.1 — and that is
what z3ntu's Nexus 5 work did, binding to `qcom,msm8916-camss` outright.

It is wrong, and quietly. camss decides things by SoC version in five places,
and msm8974 does not fall on the same side at all of them:

| site | what it decides | msm8974 goes with |
|---|---|---|
| `camss.c`, allocating the ISPIF | whether there is an ISPIF at all | 8x16/8x53/8x96 |
| `camss-csiphy.c`, `base_clk_mux` | whether to map the CSIPHY clock mux register | 8x16/8x53/8x96 |
| `camss-csid.c`, `csid_src_pad_code` | 8x16 passes the sink code through, later parts remap it | **8x16** |
| `camss-vfe.c`, `vfe_src_pad_code` | which source codes a sink code can produce | **8x16** |
| `camss-ispif.c`, ×4 | ISPIF register layout, line count and interrupt handler | **8x96** |

The VFE one is a `switch` rather than a chain of `==` comparisons, so it is
easy to miss when grepping — and missing it is not silent. Its `default:` is
`WARN(1, "Unsupported HW version")`, which fires during probe with a full call
trace. That is how it was found.

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
hardware, not a quirk table, and should be acceptable upstream. It is the path
the test-pattern capture below actually used.

Contiguous buffers come from CMA, and 20 MP RAW10 frames are large: the
phone's command line already carries `cma=768M` alongside `msm.vram=512m` for
the GPU. 1920x1080 works; a full-resolution queue is untested.

## Clocks, and one unreachable rate

Every clock the resource tables name is already in mainline's
`mmcc-msm8974`; none had to be added. Three things differ from msm8916:

- msm8974 has **no plain `CAMSS_AHB_CLK`**. msm8916's tables list an `"ahb"`
  clock and msm8974 has only `CAMSS_TOP_AHB_CLK` and `CAMSS_MICRO_AHB_CLK`,
  so there is no `"ahb"` entry here, and the driver does not miss it.
- Both VFEs share a single `CAMSS_VFE_GDSC`, so there is no per-VFE
  `has_pd`/`pd_name` as on msm8953; the power domain goes on the camss node,
  as on msm8916. It appears as `camss_vfe` in `pm_genpd_summary`.
- **The VFE rate list stops at 400 MHz, deliberately.** msm8974's
  `ftbl_camss_vfe_vfe0_1_clk` ends with `F(465000000, P_MMPLL3, 2, 0, 0)`, but
  `vfe0_clk_src`'s parent map is `mmcc_xo_mmpll0_mmpll1_gpll0_map`, which has
  no MMPLL3. That entry is unreachable and `clk_round_rate` returns `-ENOENT`
  for it. It matters because **with no sensor attached camss asks for the
  highest rate in the list**, so the first thing that happens on opening
  `/dev/video0` is:

      qcom-camss fda0ac00.camss: clk round rate failed: -2
      qcom-camss fda0ac00.camss: Failed to power up pipeline: -22

  and the node cannot be opened at all. Whether the real fix belongs in
  `mmcc-msm8974` — giving `vfe0_clk_src` a parent map that includes MMPLL3 —
  is worth asking upstream. Dropping the rate here is the conservative half.

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

Rebuilding the module needs no flash, so iterating on the driver is fast. Only
a device tree change needs one.

## Capturing from the test pattern generator

This is the check that proves the pipeline end to end without a sensor. CSID0
generates the frames, and they travel ISPIF0 → VFE0 RDI0 → `/dev/video0`.

    media-ctl -d /dev/media0 -l '"msm_csid0":1->"msm_ispif0":0[1]'
    media-ctl -d /dev/media0 -l '"msm_ispif0":1->"msm_vfe0_rdi0":0[1]'
    media-ctl -d /dev/media0 -V '"msm_csid0":0[fmt:SRGGB10_1X10/1920x1080]'
    media-ctl -d /dev/media0 -V '"msm_ispif0":0[fmt:SRGGB10_1X10/1920x1080]'
    media-ctl -d /dev/media0 -V '"msm_vfe0_rdi0":0[fmt:SRGGB10_1X10/1920x1080]'
    v4l2-ctl -d /dev/v4l-subdev3 --set-ctrl test_pattern=1
    v4l2-ctl -d /dev/video0 --set-fmt-video=width=1920,height=1080,pixelformat=pRAA
    v4l2-ctl -d /dev/video0 --stream-mmap --stream-count=10 --stream-to=/tmp/tpg.raw

`v4l-utils` is not installed by default; `apk add v4l-utils`. The subdev
numbers move between boots, so read them from `/sys/class/video4linux/*/name`
rather than assuming `v4l-subdev3` is CSID0.

Result on 2026-09-19: ten frames, 2,592,000 bytes each, which is exactly
1920 × 1080 × 10 / 8 for packed SRGGB10. The content is the generator's ramp —
row 0 begins `00 01 02 03`, row 540 begins `80 81 82 83`, all 256 byte values
present, 0.4% zeros, and successive frames identical as a static pattern
should be. No errors in dmesg.

## What is not done

- **No sensor has been attached**, because neither sensor driver exists. Every
  claim above is about the ISP, not about the cameras.
- **`vdda-supply` is missing from the node.** Sony's tree gives the CSIDs
  `pm8941_l12` at 1.8 V; without it the driver logs `supply vdda not found,
  using dummy regulator` four times at probe. Harmless for the test pattern,
  which needs no MIPI signalling, but it has to be added before a real sensor
  is wired up — and that needs another flash.
- **Only VFE0 RDI0 has been exercised.** The dual-VFE paths and the PIX
  (format-converting) lines are untouched; 8x16 ships one VFE, so two is new
  ground.
- **Only 1920x1080.** Nothing has been tried at 20 MP, where the CMA
  reservation is the thing to watch.

# Front camera sensor: Sony IMX132

`imx132.c`, and `sensor-nodes.dtsi` for the device tree half. Both are
**written and compiling; neither has ever bound to the sensor**, because that
needs the device tree node, which needs a flash. The module loads and
registers on the I2C bus; nothing past that has run.

## Where the numbers came from

There is no register table for this part in any kernel, and the stock Android
camera stack computes its writes at run time rather than holding one — see
`../../docs/prior-art.md`. So every value in `imx132_mode_1976x1144` was read
back from the sensor's own power-on defaults over CCI, and the driver's
`imx132_configure()` writes them out again. That is closer to an assertion
than a configuration, and it is written in full so that a second mode has
something to differ from.

Two independent reasons to trust them. The geometry matches Sony's own device
tree for this phone exactly — `X_OUTPUT_SIZE` 1976 against Sony's
`pixel_number_w = <1976>` — and the crop window is self-consistent:
1975 - 0 + 1 = 1976, and 1171 - 28 + 1 = 1144. The full register dump is in
`../../docs/camera.md`.

The power sequence is Sony's, taken exactly from their published device tree:
vdig, vio, vana, reset released, then the clock, with 1 ms between each and
98 ms held down on the way out.

## Things that are guesses, and how to check them

- **Analogue gain.** The reciprocal law `gain = 256 / (256 - value)` is Sony's
  usual one and the register is in the usual place, but the maximum is a
  guess. Sweep it against a fixed scene once frames arrive.
- **The clock rate.** The driver requires 19.2 MHz because that is what the
  board supplies and what the sensor was read at. Sony's own driver sets
  8 MHz. Both cannot be right about what Sony shipped, and the PLL maths
  below depends on which it is.
- **Link frequency**, 216 MHz, is `19.2 MHz x 45 / 10` for the pixel clock and
  then `x 10 bits / 2 lanes / 2` for DDR. If the sensor is actually driven at
  8 MHz this is wrong by a factor of 2.4.
- **Lane mapping.** Clock on lane 1 with data on 0 and 2 reproduces Sony's
  `csi-lane-mask = <0x7>`, and the rear camera's `0x1f` is the same layout
  with four data lanes. Consistent, but not confirmed by a working link.
- **Bayer order.** Sony's `subdev_code = 0x3007` is `SBGGR10`, so that is the
  no-flip order, and the flip table follows from it. A wrong guess here shows
  up as swapped colours, not as a failure.

## The image to flash

`images/boot-imx132-v1.img` — the running image plus the sensor node, the
camss `port@2` endpoint and `vdda-supply`, and nothing else.

    tail of the kernel is exactly the previous image's DTB    cmp: equal
    ramdisk, cmdline, vmlinuz after repack                    all equal
    strings -a ... | grep -c msm.vram=512m                    1
    strings -a ... | grep -c max1187x                         1
    dt-equiv.py boot-imx132-v1.img live.dtb   606 nodes, 6 differences

Every phandle in the new nodes was read back out of the built blob and checked
against its target: the supplies resolve to `l17`, `l3` and `lvs2`, the clock
to the MMCC node index 79 (`CAMSS_MCLK2_CLK`), the reset GPIO to TLMM 18, the
camss `vdda` to `l12`, and the two endpoints to each other. That check earned
its keep: the clock rate had been written `0x1249f00`, a transposition of
`0x124f800`, which would have failed the driver's frequency test at probe.

The backup taken beforehand is `images/boot-backup-before-imx132.img`; its
prefix is byte-identical to `boot-camss-v1.img`, which confirms what the phone
was running.

After flashing:

    sudo modprobe imx132
    dmesg | tail -40
    media-ctl -d /dev/media0 -p | head -30

The first question is whether probe reads the chip ID back — that exercises the
power sequence, the clock and the CCI bus together. Expect the link itself to
need work after that.

Note the master clocks are still hung off the `cci` node as well as the sensor
node. That was a workaround from before there was a sensor driver, and it
means MCLK runs whether or not the driver asks for it. It should come out once
the driver is known to manage the clock itself — `sensor-nodes.dtsi` has the
`&cci` override that removes it, and that override is deliberately **not** in
the flashed image, so this first test cannot fail for want of a clock.
