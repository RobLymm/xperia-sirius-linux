# Adreno 330: making the GPU say what went wrong

**Applications drawing on the GPU still lock it up.** The symptoms, the
diagnosis and the dead ends are in `../../docs/known-problems.md`; this page
is about the patches here and how to use them.

Last verified against the device on 2026-09-20.

## The four that are fixes, not instrumentation

**`0022-drm-msm-keep-the-shrinker-off-carveout-backed-objects.patch`** and
**`0023-drm-msm-reclaim-carveout-objects-when-the-carveout-fills.patch`** are
a pair and must be taken together. Without an IOMMU, msm serves GEM objects
out of a VRAM carveout, and those objects are created with
`drm_gem_private_object_init()`, so they have no shmem file. Nothing stopped
the shrinker taking one: purging it reached
`shmem_truncate_range(file_inode(obj->filp), ...)` on a NULL file, killing
kswapd inside the GEM locks and leaving the compositor blocked in
`msm_gem_madvise()` with no way back but a reboot. Evicting one was quieter
and worse -- `put_pages()` returned the carveout allocation with no backing
store to restore from. Freeing them also returned nothing to the system,
because the carveout is reserved at boot, while the shrinker counted the full
size as reclaimed.

0022 reports them unpurgeable, which also covers eviction. 0023 is the
companion: with the shrinker barred, nothing else would ever reclaim the
carveout, so `get_pages_vram()` now purges the objects userspace has marked
DONTNEED and retries once before giving up.

Verified on the device: firing the shrinker directly through
`/sys/kernel/debug/dri/0/shrink` with 39 purgeable carveout objects present
returned SHRINK_STOP and purged none, and 928 MiB of memory pressure --
MemAvailable down to 111 MB, CmaFree to 92 kB -- left `Purged` at zero with
no crash. Separately, with the carveout sized down until it genuinely filled,
0023 reclaimed 49 objects, which only it can do once 0022 is in place.

**`0024-drm-msm-do-not-report-statistics-the-gpu-never-wrote.patch`** and
**`0025-drm-msm-do-not-truncate-the-divisor-for-the-cp-clock.patch`** are also
a pair, and 0025 is a bug for every Adreno, not just this one.
`retire_submit()` reads per-submission statistics out of the ring, but only
a6xx emits the packets that write them; a3xx and a4xx write nothing, nothing
clears the slot, and it is reused every 64 submissions, so the values are
whatever was last in that memory. Measured here:

    msm_gpu_submit_retired: elapsed=96076787681504331 ns mhz=8478678518
                            start=-9 end=-83886081

0024 clears the slot before the GPU runs the submission.

0025 fixes the arithmetic that consumes it. The guard tests all 64 bits:

    if (elapsed) {
            clock = cycles * 1000;
            do_div(clock, elapsed);
    }

but `do_div()` takes a 32 bit divisor -- `uint32_t __base = (base)` -- so it
divides by the low half alone. High half set, low half zero, and the guard
passes with a zero divisor. `elapsed` passes 32 bits at 4.3 seconds, so any
Adreno reaches this whenever a submission stalls, which is exactly the case
the value is read for. On this phone it fired about once a second while
drawing: 93 kernel stack dumps in the first 375 seconds of uptime, and zero
after the fix.

The driver for this GPU is `drm/msm` in the kernel with Mesa's freedreno in
userspace. Both are written for Adreno hardware, and heavy 3D runs on them
without trouble: glmark2 completes every scene, full screen, for over five
minutes with no hang. The lockups come from what GTK applications draw.

## What is here

Eight patches against 6.16.12, carried in postmarketOS's
`linux-postmarketos-qcom-msm8974` aport. 0018 to 0021 make the GPU report
what it already knows; 0022 to 0025 are ordinary kernel bugs found while
looking, each of which stands on its own and is not specific to this phone.

**`0018-drm-msm-a3xx-report-the-faults-the-gpu-already-raises.patch`** — the
one that changed the investigation. `a3xx_irq()` read `RBBM_INT_0_STATUS`,
cleared it and looked at nothing, so an opcode error, a protected register
write, a bus error or an out of bounds cache access all left no trace. Those
interrupts were enabled the whole time. This prints the status along with
`CP_PROTECT_STATUS`, `RBBM_AHB_ERROR_STATUS`, `CP_STAT`, `RBBM_STATUS` and
where the command processor is, clears a bus error so it cannot repeat, adds
the hardware's own hang detect interrupt to the mask, and programs
`RBBM_INTERFACE_HANG_INT_CTL` with the value this revision wants: `0x8000ffff`
on an a330v2, where every part was given the earlier revision's `0x00010fff`.
Qualcomm's driver for this generation does both.

**`0019-drm-msm-a3xx-send-the-context-rollover-packet.patch`** — the dummy
set-constant that makes the shader front end roll its context has sat behind
`#if 0` since this driver was written, and no longer compiled: the register it
named was renamed and the `CP_REG` helper removed. Qualcomm's driver sends it
after every A3XX submission. It has not changed the hang rate here, but it is
one line of divergence from the hardware's own driver, on the block the
stalls implicate.

**`0020-drm-msm-a3xx-tell-hangcheck-when-the-cp-is-moving.patch`** — without a
progress callback the hangcheck timer cannot tell a slow submission from a
stopped one, so a long frame is killed exactly like a lockup and the GPU is
reset under a working application. This samples both indirect buffer pointers
and their remaining sizes, which is what Qualcomm's driver samples for the
same purpose. When it is in use the hangcheck period halves, to 250 ms, which
is the quick way to tell the patch is live.

**`0021-drm-msm-look-for-a-hang-when-the-gpu-says-it-has-one.patch`** — with
hang detection enabled the hardware reports a stall several seconds before the
timer notices: in one 48 second test it raised seven while the timer declared
three, eighteen seconds after the first report. That delay matters for
diagnosis, because the GPU state captured for a crash dump is then taken long
after whatever went wrong. This adds `msm_gpu_hangcheck_soon()`, which brings
a pending hangcheck forward without changing what it decides, and calls it
from the A3XX handler when the hang detect interrupt arrives.

## What the GPU reports

With 0018 in place, every fault during an application lockup reads the same:

    adreno fdb00000.gpu: error: int0 01000000, protect 00000000,
        ahb-err 0000009c, cp-stat 84d43cf8, rbbm e0684003
    adreno fdb00000.gpu: ib1 92724000 left 1034, ib2 9271c000 left 738,
        ring rptr 202 wptr 202

- `int0` bit 24 is `MISC_HANG_DETECT`, the GPU's own interface hang detector.
- `protect` is zero: no protected register was written.
- `ahb-err 0000009c` is **not** a bus error. That register reads `0000009c`
  while the GPU is idle and healthy, so it is a constant.
- `cp-stat` has the micro-engine busy and waiting on its register interface,
  with the ring and both indirect buffer queues busy.
- `rbbm` has the shader front end, vertex fetch, vertex parameter cache and
  primitive control blocks busy.

Together: a draw that never finishes, with the command processor waiting
behind it. Not a malformed packet and not a forbidden register access.

## Measuring a change

Count the hardware faults, not the lockups. The hardware reports about twice
as many events as the driver declares, so a test reaches an answer sooner:

    ssh phone 'sudo journalctl -k -b 0 | grep -c "error: int0"'

Two tools in `../../tools/` help with the rest. `ib-at-hang.py` decodes the
command stream at the point the command processor stopped, from a crash dump
in `/sys/class/devcoredump`, naming registers and packets from Mesa's adreno
register descriptions. `compare-frames.py` compares two screenshots of the
same frame, one drawn by the GPU and one by the CPU, and reports how many
pixels differ and where. On a still frame the two agree except along the
edges of text and rounded shapes, which is antialiasing rounding rather than
corruption.

## Building

These are ordinary kernel patches. In the aport they sit after the CPU
frequency and L2 cache patches; nothing else in the tree depends on them.

    cd <pmaports>
    pmbootstrap build linux-postmarketos-qcom-msm8974 --arch armv7 --force

Flash an image whose device tree comes out of the boot partition that is
already flashed, not from a rebuilt source tree, or device tree work done by
others is lost.
