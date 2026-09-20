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

A worked example, 2026-09-19, after the camera tree was flashed. Against the
phone's own live tree:

    tools/dt-equiv.py images/boot-camss-v1.img live.dtb
    606 nodes compared, 0 differences

    tools/dt-equiv.py images/boot-cam-v2.img live.dtb
    604 nodes compared, 2 differences

The first names what the phone is running. The second is the image it ran
before, and its two differences are the camss node and its `ports`. Node counts are
the quick tell that nothing was dropped: the images that cost the evening
below were built on a tree with fewer nodes, and this check would have said so
in a second.

## Which document holds which kind of fact

Thirty-odd markdown files is enough to lose a fact in, and the way this goes
wrong is the same as with device trees: two places say the same thing, one
gets updated, and the other quietly becomes a lie. Three claims in this
repository were wrong for weeks that way — CPU scaling recorded as broken
while the phone ran its full rated range, the light sensor recorded as dead
while it reported the room, and a "what is left" page still saying the modem
stalls after it had started making calls.

So each kind of claim has one home, and everything else links to it.

| Kind of claim | The one place | Everywhere else |
|---|---|---|
| What works and what does not | the state table in `../README.md` | link to it, do not restate |
| How big an unfinished thing is, and why | `whats-left.md` | |
| What to do next, and what proves it done | `todo.md` | |
| A fault's symptoms, diagnosis and dead ends | `known-problems.md` | |
| Where work goes; gaps between repo and phone | this file | |
| How a stranger installs the port | `from-stock-to-this.md` | |
| One subsystem in depth | `<subsystem>.md`, or the driver's own README | |

**Versions and changelogs.** Documents here carry neither, deliberately: git
is the changelog, and a hand-maintained one would be a second thing to forget
to update. What they do carry, wherever they assert something about the
device, is a line saying when that was last checked:

    Last verified against the device on <date>.

That date is the useful thing. It tells a reader how much to trust the page
without reading the git log, and it makes staleness visible instead of
silent. If you change a claim, move the date. If you read a page whose date
is old, check the phone before believing it.

## Canonical places

| Work | The one place it belongs |
|---|---|
| Board device tree | `devicetree/qcom-msm8974pro-sony-xperia-shinano-sirius.dts` |
| SoC-wide device tree changes (CPU clocks, thermal) | a kernel patch against `qcom-msm8974.dtsi`, never the board file |
| Kernel drivers | `drivers/<subsystem>/`, and a patch in the kernel package |
| Which source each `updates/` module is built from | `docs/out-of-tree-modules.md`. Rebuild all of them after any kernel change, or the subsystem goes missing with no visible error |
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

**Thirty-four of the modules the phone has loaded are hand-built**, from
`/lib/modules/6.16.12/updates`, not from the kernel package: the whole QDSP6
audio stack, the WCD9320 codec, the SLIMbus NGD controller, the PM8941 clock
divider, the q6voice set, the panel driver, the battery pair, the touchscreen,
the CCI controller, the eleven V4L2 media core modules and camss. Display, touch,
battery, audio and the camera bus therefore currently depend on modules nobody
else can obtain by installing packages.

- **The touch driver is in `drivers/touch/` but not in the kernel package.**
  Published on 2026-09-19; until then it existed only on the test phone.
  Nothing in the package builds it, so a packaged Z2 still has no
  touchscreen, and that is the remaining half of this gap.

  **It is not the pristine driver.** It carries two changes made while
  chasing suspend, both described in `../drivers/touch/README.md`. Both
  belong upstream.
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

**Camera: the control bus and the media core work, and neither is packaged.**
The phone runs `images/boot-camss-v1.img` — confirmed against
`/sys/firmware/fdt`, 606 nodes, 0 differences — so `cci@fda0c000` is enabled,
both sensors answer, and the camss node is present and bound. `i2c-qcom-cci` is hand-built in `/lib/modules/6.16.12/updates/cci/`, and the
eleven V4L2 media core modules plus `qcom-camss.ko` are hand-built in
`/lib/modules/6.16.12/updates/media/`. Nothing in the kernel package builds
any of them.

The board file now does carry the camera device tree changes that image has —
the `cci` node's clocks and status, `lvs2`, `l3` and `l23` held on, and the
two camera MCLK pin states.

**But the repository still cannot rebuild the tree the phone runs.** Measured
on 2026-09-19: building the codec variant from the board file with patch 0012
applied gives 569 nodes against the live tree's 604, and 87 differences.
What is missing is not camera-related — thermal trip points, `power-domains`
and `cx-supply` on all three remoteprocs, and interconnect clocks — and it
came from kernel patches that are not in this repository. Until that is
chased down, an image built from source here is a **regression** against what
the phone is running, and the way to make a test image is to edit the device
tree of an image known to boot and check the difference is only what you
intended. `../drivers/camera/README.md` does exactly that and shows the
checks.

`handover-camera.md` holds the state and the reproduction steps, and
`camera.md` the hardware. The one thing worth repeating here, because it is
not camera-specific: **out-of-tree modules for this phone need a
`Module.symvers` that the kernel tree does not have**, and
`tools/harvest-symvers.py` plus `tools/fill-symvers.py` build one from the
running kernel's own CRCs. Any module work on this device needs them, not
just the camera.

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


## Publishing to GitHub

Added 2026-09-20, because it was not written down and a push that day left the
top-level README describing the GPU as it had been that morning.

The repository is `git@github.com:RobLymm/xperia-sirius-linux.git`, branch
`main`. Pushing the files is the easy half; the parts that go stale are the
ones nobody is forced to touch.

**Before pushing, work outwards from the change:**

1. **The subsystem README** next to the patches. What the patch does, what
   was measured, and how someone else would check it.
2. **The state table in the top-level `README.md`**, and the sentence under it
   that names the biggest problem. This is what a visitor reads first and it
   is the easiest thing to leave behind. A change that alters what works, or
   what is known about why something does not, belongs here.
3. **`Last verified against the device on <date>`** in every README the change
   touches, including the top-level one. Different files carry different
   dates on purpose; each means the last time *that* page was checked.
4. **`docs/known-problems.md`** if the change alters a known problem, even
   when it does not fix it. Narrowing a cause counts.

**The GitHub repository metadata is part of the documentation and has no file
in the repository, so nothing reminds you.** Check it after any change that
adds a subsystem or changes what works:

    curl -s https://api.github.com/repos/RobLymm/xperia-sirius-linux |
        python3 -c 'import json,sys; d=json.load(sys.stdin); print(d["description"]); print(d["topics"])'

The description should name the subsystems someone would search for. It is
set from the repository page, or with a token:

    curl -X PATCH -H "Authorization: Bearer $GITHUB_TOKEN" \
        https://api.github.com/repos/RobLymm/xperia-sirius-linux \
        -d '{"description":"..."}'

As of 2026-09-20 the description still listed only the panel, sound, modem and
GNSS work: it predated the camera, the CPU frequency scaling and the GPU.

