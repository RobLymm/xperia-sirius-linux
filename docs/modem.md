# Modem: boots, then stalls during initialisation

Unfinished. This document exists so the next person does not repeat the two
dead ends already walked, and knows exactly where the remaining problem is.

## Current state

The modem subsystem (MSS remoteproc) loads its firmware and starts. It then
stalls roughly 41 seconds into initialisation and never finishes coming up, so
there is no usable modem: no QMI services, no ModemManager device, no calls or
data.

## Dead end 1: the Xperia Z3's modem firmware does not work on a Z2

postmarketOS ships `firmware-sony-leo-modem`, the Xperia Z3 set, because the
Z2 runs the Z3 device tree. The Z3 firmware is **rejected** by the Z2 —
loading fails outright rather than misbehaving subtly.

What works is the Z2's own `modem.*` and `mba.*`, taken from the stock system
partition of the phone itself. Those are proprietary and not redistributable;
see `extracting-from-stock.md` for how to get them off your own device.

Note the packaging trap. Those firmware paths under `/lib/firmware/postmarketos/`
are **owned by `firmware-sony-leo-modem`**, so replacing the files by hand
works until any upgrade, reinstall or `apk fix` of that package silently puts
the Z3 files back and the modem stops booting again. The proper fix is a
separate `firmware-sony-sirius-modem` package that owns the Z2 paths.

## Dead end 2: rmtfs with nothing to serve

The modem keeps its calibration and settings in NV storage on the host, served
by `rmtfs` over shared memory. If rmtfs has no backing files it answers every
read with a failure, and the modem gets far enough to look initialised while
having no usable state.

The backing files have to be copied out of the phone's own partitions:

    modemst1  ->  modem_fs1
    modemst2  ->  modem_fs2
    fsg       ->  modem_fsg

Placed where rmtfs expects them (on this system, `/boot/`). **Do not delete
them.** They are the modem's own NV storage, and on a device where the stock
ROM is gone they are the only copy.

## Where it actually stops, and what has been ruled out

With the Z2's own firmware and populated NV storage, the modem loads, starts,
and then exactly 40 seconds later kills itself:

    remoteproc0: remote processor fc880000.remoteproc is now up
    qcom-q6v5-mss: fatal error received: dog.c:1495:Watchdog detects stalled initialization
    remoteproc0: crash detected in fc880000.remoteproc: type fatal error

That message is the modem's own watchdog reporting that one of its
initialisation tasks never checked in. Five hypotheses have been tested and
eliminated:

**The mpss region is not too small.** The firmware totals 47.9 MB against an
81 MB region.

**The firmware set is not incomplete.** `modem.b07` is absent from the files,
which looks alarming, but segment 7 in `modem.mdt` has `filesz = 0`, so no
file is expected. Every segment that carries data has a matching file.

**The load addresses are right.** The firmware's own program headers ask for
0x08000000 to 0x0d102000, and the reserved regions place mpss at 0x08000000
with the hash segment landing at the mba base, 0x0d100000. The Z3's firmware
wants 0x08000000 to 0x0cf02000, two megabytes less, so the regions in this
tree have already been sized for the Z2 rather than inherited from the Z3.

**rmtfs is not the problem.** Run with `-v` across a full boot-and-stall
cycle, it prints `registering services` and then nothing: the modem never
asks it for anything before the watchdog fires. All three partitions it would
serve — modemst1, modemst2, fsg — are present under /dev/disk/by-partlabel.

**The userspace stack is not missing anything.** It matches the Fairphone 2,
another msm8974 device whose modem works on mainline: msm-modem,
msm-modem-uim-selection, soc-qcom-msm8974, rmtfs, qrtr. `pd-mapper` is not
packaged for this distribution and the FP2 does not use it either.

## What the modem does manage before it stalls

The SMD link to the AP comes up and three channels appear:

    remoteproc0:smd-edge.IPCRTR
    remoteproc0:smd-edge.SSM_RTR_MODEM_APPS
    remoteproc0:smd-edge.rpmsg_ctrl

So shared memory, the SMD edge and the QRTR transport all work. What is
missing is everything that appears later: the ADSP on the same kernel opens
DIAG, DIAG_CNTL, sys_mon and the APR channels, and the modem opens none of
those. It gets through transport setup and stops before service registration.

That is the shape of the remaining problem: not firmware, not memory, not
rmtfs, but something the modem waits for between bringing up IPCRTR and
registering its services.

The obvious remaining difference from the Fairphone 2 is the bootloader. FP2
boots through `lk2nd-msm8974`; this phone is booted directly by Sony's stock
bootloader with a fastboot-flashed image. Whether lk2nd leaves state the
modem depends on has not been tested and is the next thing worth trying.

## What is not the problem

- The remoteproc plumbing. MSS and ADSP both load and start.
- Missing `rmtfs`. It is present and running.
- QRTR. The transport comes up; the services never arrive to be discovered.
