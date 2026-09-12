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

## Where it actually stops

With the right firmware and populated NV storage, the modem starts and then
stalls at about 41 seconds. That is the open problem.

The most promising next step, not yet done: compare the modem node in the
device tree used here against Sony's stock device tree property by property.
The tree in use is the Xperia Z3's, and the firmware is the Z2's, so any
property describing memory regions, clocks, regulators or reset lines that
differs between the two models is a candidate. The stock tree is available by
decompiling the FOTA kernel, as described in `extracting-from-stock.md`.

Worth trying alongside that: the Z2's own ADSP firmware. The ADSP currently
runs the Z3 files and appears to work, which is not proof that it is correct,
and the modem depends on a working ADSP.

## What is not the problem

- The remoteproc plumbing. MSS and ADSP both load and start.
- Missing `rmtfs`. It is present and running.
- QRTR. The transport comes up; the services never arrive to be discovered.
