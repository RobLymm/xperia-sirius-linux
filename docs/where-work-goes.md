# Where work goes, and what is being moved away from

Read this before changing anything. Several people work on this port at once,
and the commonest way to lose a day is to edit or read a file that looks
current and is not.

Last checked against the device and the repository on 2026-09-19.

## The rule that matters most

**Never read a `.dtb` file to find out what the phone is running.** The phone
boots a device tree appended to the kernel inside the image flashed to the boot
partition with `fastboot`. Files under `/boot` are build artifacts of
postmarketOS's own image tooling, and they are stale whenever the flashed image
is newer.

To find out what is actually running:

    ssh phone 'sudo cat /sys/firmware/fdt' > live.dtb
    tools/dt-equiv.py images/<candidate>.img live.dtb     # 0 differences = that image

`/sys/firmware/fdt` is the tree the kernel received. Everything else is a guess.

**Nor rebuild a tree from `/proc/device-tree`.** `dtc -I fs -O dts
/proc/device-tree` looks like the same thing and is not: the reconstruction
does not round-trip into a bootable tree. Use `/sys/firmware/fdt`, which is
the blob itself, or the DTB inside an image known to boot.

A worked example, 2026-09-19. Comparing the image the phone was flashed with
against its own live tree:

    tools/dt-equiv.py images/boot-voice-v1.img live.dtb
    601 nodes compared, 0 differences

and a candidate carrying deliberate camera changes against the same tree:

    tools/dt-equiv.py images/boot-cam-v2.img live.dtb
    601 nodes compared, 8 differences

all eight being the intended ones. Equal node counts are the quick tell that
nothing was dropped: the images that cost the evening below were built on a
tree with fewer nodes, and this check would have said so in a second.

## Canonical places

| Work | The one place it belongs |
|---|---|
| Board device tree | `devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` |
| SoC-wide device tree changes (CPU clocks, thermal) | a kernel patch against `qcom-msm8974.dtsi`, never the board file |
| Kernel drivers | `drivers/<subsystem>/`, and a patch in the kernel package |
| Packaging | the pmaports recipes: `device-sony-sirius`, `firmware-sony-sirius-modem`, `linux-postmarketos-qcom-msm8974` |
| Userspace configuration | `userspace/` (UCM, udev, systemd) |
| Modem support | `modem/` |
| Radio application | its own repository, `robwatts-fm-radio` |
| How a stranger installs this | `docs/from-stock-to-this.md` |
| Device tree style and checks | `devicetree/README.md`, "Keeping it in line" |

## Being moved away from, and why

| Thing | Status |
|---|---|
| `/boot/qcom-msm8974pro-sony-xperia-shinano-leo.dtb` | Stale artifact. Named after the Z3 because `deviceinfo_dtb` still says leo, and missing every board-specific node including the touchscreen. Not what boots. Goes when the device package is swapped |
| `/boot/dtbs/*.dtb` (60-odd files) | The kernel package's device trees for every supported phone. Only `...-shinano-sirius.dtb` concerns this device, and even that is not what boots |
| Decompiled trees: `leo*.dts`, `sensors-*.dts`, `nfc.dts`, `touch-node.dts` | History. Phandle numbers instead of labels, so SoC changes cannot reach them. Do not edit, build or flash |
| `qcom-msm8974-sony-xperia-sirius.dts` in the kernel fork | The Rhine-era Z2 tree. Hangs before USB comes up. Nothing points at it and nothing should |
| postmarketOS's archived `device-sony-sirius` | The old port, which used that non-booting tree. Not related to the package here beyond the name |
| `device-sony-leo`, `firmware-sony-leo-adsp`, `firmware-sony-leo-wifi` | Installed on the test phone and still in use. Being replaced by Z2-named packages. The ADSP and Wi-Fi blobs are genuinely needed, so these cannot simply be removed |
| Variant trees `...-sirius-codec.dts`, `...-sirius-fmaudio.dts` | Temporary. They `#include` the board file and add one capability each. Fold into the board file once the capability is proven on hardware, then delete |
| Kernel patches 0002 and 0004 | Present in the package directory but not in its `source=` list, so they do nothing. 0004 adds a ramoops region the phone does not use |
| `latest.dts`, `latest.dtb`, `tools/update-latest.sh` | Deleted on 2026-09-13. Do not reintroduce a "latest" pointer: it went stale without anyone noticing |

## Known gaps between this repository and the running phone

Checked on the device on 2026-09-19. The phone works; a phone built only from
what is packaged here would not, and these are the reasons.

**Twenty-two of the modules the phone has loaded are hand-built**, from
`/lib/modules/6.16.12/updates`, not from the kernel package: the whole QDSP6
audio stack, the WCD9320 codec, the SLIMbus NGD controller, the PM8941 clock
divider, the q6voice set, the panel driver, the battery pair and the
touchscreen. Display, touch, battery and audio therefore currently depend on
modules nobody else can obtain by installing packages.

- **The touch driver is not in this repository at all.** `max1187x.c` and its
  two headers live in `drivers-wip/max1187x/` outside the repository, and on
  the test phone in `/home/rob/max1187x-fix`. `drivers/touch/` here is an
  empty directory. Nothing in the kernel package builds it, so a packaged Z2
  has no touchscreen.

  **That copy is not the pristine driver.** It carries two changes made while
  chasing suspend, and anyone diffing it against the packaged version will
  meet them: `.remove` is wired to the existing `shutdown()`, without which
  the driver leaks its interrupt GPIO and input device and can never be
  probed a second time; and `resume()` now calls `set_resume_mode()`, whose
  only previous caller sat behind `#if 0`, so nothing ever woke the
  controller after a suspend. Both belong upstream. Neither is sufficient on
  its own to make touch survive a suspend; see `known-problems.md`.
- **The board device tree carries no headphone or FM nodes.** No SLIMbus,
  codec, secondary MI2S, internal FM, voice link or MCLK controller. Those
  are in the two variant trees, and a tree built from the board file alone
  gives speakers only.

  Until 2026-09-19 this was worse than it sounded: the codec tree
  `#include`s `...-sirius-fmaudio.dts`, and that file had never been
  published, so the chain could not be built from the repository at all. It
  is now present. The chain is codec -> fmaudio -> board, and the codec tree
  is the one to build.
- **The kernel package builds 14 patches**: the panel, the two battery
  patches, the q6afe fix, the sound card machine driver, the board device tree
  and the CPU and L2 scaling set. It does not build the touch driver, the
  WCD9320 codec, the PM8941 clock divider, q6voice or the internal FM capture
  port, all of which the phone is running.
- **Its CPU table caps the clock at 960 MHz**: 22 of its 31 operating points
  are marked disabled, although the phone itself runs the full rated range
  from a hand-built tree.
- **The phone still identifies as a Z3.** `device-sony-sirius` is not
  installed, only its `-alsa` and `-phosh` subpackages; `deviceinfo` names
  `sony-leo` and the Z3 device tree, and `/boot` holds only the stale
  leo `.dtb`.
- **The packaged install route has never been booted.** Everything verified so
  far was flashed by hand, which is why `docs/from-stock-to-this.md` still
  starts by installing postmarketOS as an Xperia Z3 and layering the Z2 on top.

**Camera: the control bus works.** With `cci@fda0c000` enabled and the two
camera master clocks hung off it, both CCI masters come up as ordinary i2c
adapters, mainline's `i2c-qcom-cci` binds to `qcom,msm8974-cci`, its
interrupt fires on every transfer, and the clocks run at 19.2 MHz. The module
is hand-built and installed at `/lib/modules/6.16.12/updates/cci/`; it is not
loaded at the moment only because the phone was reverted to a tree in which
the node is disabled.

The sensors do not answer yet. Three of the four rails were on; `lvs2`, the
`vio` supply that powers the sensors' I/O and therefore their I2C, was not,
and that is the next thing to test. Two details worth not rediscovering:
`i2cdetect` is useless on these parts, because it probes with a one-byte read
while they use 16-bit register addressing, and an absent sensor shows as
`master 0 queue 0 timeout` rather than a NAK.

See `known-problems.md` for the evening this cost, and the rule at the top of
this page for how to avoid repeating it.

## Naming

The Xperia Z2's codename is **sirius**, and it is a member of Sony's
**Shinano** platform with the Z3 (leo), Z3 Compact (aries) and Z2 Tablet
(castor). New files, packages and nodes use sirius or Z2. Device tree files
follow mainline's pattern,
`qcom-msm8974pro-sony-xperia-shinano-<codename>.dts`.

Anything still named leo or Z3 is either a genuine dependency listed above or
something not yet renamed. It is never the right place for new work.

## Before you commit a change

1. A change that should not alter the hardware description: build the DTB
   before and after, and `tools/dt-equiv.py` must report 0 differences.
2. A change that should alter it: the same command must list exactly what you
   intended and nothing else.
3. After flashing: compare the flashed image with `/sys/firmware/fdt`. 0
   differences proves the phone runs what you built.
4. Then check the hardware itself, not only the tree.

A cheap pre-flight when `dt-equiv.py` is not to hand — every boot image must
contain both of these, and an image missing either will start a phone you
cannot use:

    strings -a boot.img | grep -c msm.vram=512m   # 1: else the GPU gets no
                                                  # memory and the boot stalls
    strings -a boot.img | grep -c max1187x        # 1: else there is no
                                                  # touchscreen

Take backups from the partition, before flashing, never after:

    sudo dd if=/dev/disk/by-partlabel/boot of=backup.img bs=1M

A backup made after flashing a bad image is a copy of the bad image.

Static checks cannot prove a change is safe. The first conversion of this tree
passed every static check and still broke audio and Bluetooth. The boot is the
test.

## Where this is all going

1. postmarketOS: the packaging recipes, so a Z2 owner installs as a Z2.
2. Mainline Linux: the device tree and drivers, in the order set out in
   `upstream/README.md`.

Until both land, this repository is the reference, and the board file plus the
kernel package are what a Z2 needs.
