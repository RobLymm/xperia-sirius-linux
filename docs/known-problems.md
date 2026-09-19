# Known problems

Last verified against the device on 2026-09-19.

The things that will bite you, and what is understood about each.

## Applications rendering on the GPU hang it

The compositor runs on the Adreno 330 through freedreno and is stable.
Applications rendering on the GPU are not: they trigger repeated GPU resets,
and eventually the compositor deadlocks inside `msm_gem_madvise`, at which
point the phone is frozen and cannot be killed into a working state. Recovery
is a power cycle.

The workaround in use is to keep the compositor on the GPU and put GTK
applications on the cairo renderer, by setting `GSK_RENDERER=cairo` in the
session environment. Applications then render on the CPU, which is slower but
does not wedge the machine.

Two dead ends worth naming so they are not retried: `GSK_GPU_SKIP` does not
exist in GTK 4.22, and `GDK_DISABLE=gles-api` silently falls back to cairo
rather than doing what it appears to do, so both can look like fixes while
changing nothing.

A real fix is kernel side. A GPU hang core dump was captured and is the
starting point for anyone wanting to work on it.

## The GPU memory shrinker can crash and take the compositor with it

Seen on 2026-09-19. The phone was left on and plugged in, and the touchscreen
stopped responding. It was not the touchscreen — the max1187x driver was
loaded, bound, and its input device still carried events.

What had actually happened is in `dmesg`:

    Unable to handle kernel NULL pointer dereference at virtual address 00000014
    Internal error: Oops: 5 [#2] SMP ARM
    PC is at msm_gem_purge+0x100/0x15c
    Process kswapd0

    msm_gem_purge <- purge <- drm_gem_lru_scan <- msm_gem_shrinker_scan
                  <- do_shrink_slab <- shrink_slab <- shrink_one
                  <- shrink_node <- balance_pgdat <- kswapd

Under memory pressure the kernel ran the MSM GEM shrinker, which dereferenced
a null pointer while purging a buffer object. That killed **kswapd0**, so
memory reclaim stopped working, and it died inside the GEM locks. phoc's main
thread then blocked in `msm_gem_madvise` on the same lock and stayed in
uninterruptible sleep:

    ps -o pid,stat,wchan -p $(pgrep -x phoc)
    16141 D msm_gem_madvise

A compositor whose main thread is in D state draws nothing and reads no input,
so the screen freezes and touch looks dead. `grim` hangs too, which is a quick
way to tell this apart from a touch fault.

**There is no recovery but a reboot.** A thread in uninterruptible sleep on a
lock whose holder has died cannot be killed.

How to tell it from the touchscreen bug below: check whether phoc is wedged
before blaming touch.

    pgrep -x phoc                       # still running?
    ps -o stat,wchan -p $(pgrep -x phoc)  # D state in msm_gem_* means this bug
    grim /tmp/x.png                     # hangs, rather than failing

**What set it off is not certain.** The session that hit it had been running
the camera hard, and libcamera's EGL debayer was failing on every frame for
part of it — see `../drivers/camera/README.md`. Failed GPU debayer passes
leaking GEM objects would explain a shrinker being driven hard enough to reach
a rare path. A large disk cleanup ran shortly before, which would also have
pushed the page cache. Neither is proven.

It is probably related to the GPU hangs above: the same driver, and the same
pattern of a mainline msm driver on this SoC being exercised in ways nobody
else exercises it.

## The GPU needs a VRAM carveout, and the size matters

msm8974 has no GPU IOMMU support in mainline, so the GPU is given a contiguous
carveout:

    cma=768M msm.vram=512m msm.allow_vram_carveout=1

With 128 MB the screen freezes once the carveout fills. 512 MB has been stable
for compositor use.

## Do not build a boot image from the files in /boot

Two files there look authoritative and are not:

- **`/boot/boot.img` is not what the phone boots.** Its stored command line
  lacks `cma=768M msm.vram=512m msm.allow_vram_carveout=1`, which the running
  kernel has. Flash it and the GPU gets no memory and the boot stalls before
  USB networking starts: blank screen, no way in.
- **`/boot/qcom-msm8974pro-sony-xperia-shinano-leo.dtb` is not the device
  tree the phone boots.** It is missing the board's own nodes, the
  touchscreen among them. Build an image on it and the phone starts with no
  touchscreen, which on a phone means no way to use it.

Both were discovered the hard way, by shipping three boot images built on
that DTB and leaving the device unusable for an evening. The nodes that
*are* present make it worse: the display and GPU nodes are there, so the
phone looks nearly right while being unusable.

**Before flashing anything, check the image carries what it should:**

    strings -a boot.img | grep -c msm.vram=512m     # expect 1
    strings -a boot.img | grep -c max1187x          # expect 1, the touchscreen

**Take backups from the boot partition, not from /boot, and take them before
flashing, not after:**

    sudo dd if=/dev/disk/by-partlabel/boot of=backup.img bs=1M

A backup made after flashing a bad image is a copy of the bad image. That
also happened.

The correct source for a modified device tree is the DTB inside a boot image
known to start, or the board sources in `../devicetree/`. Not `/boot`, and
not `dtc -I fs -O dts /proc/device-tree` — the live tree round-trips into
something that does not boot.

## Suspend and resume

Suspend itself works. It was recorded here as broken, but `suspend.target`
and `sleep.target` were simply **masked**, which is why the kernel's suspend
counters read zero attempts. Writing `freeze` to `/sys/power/state` suspends
and resumes correctly, and the kernel reports success. What made it look
fatal is two drivers that do not come back.

### Nothing but the power key can wake it

There is no remote wake source at all. The Broadcom chip answers "Not
supported" when asked about wake-on-wireless, the RTC alarm does not wake
s2idle, the USB gadget goes down with everything else, and enabling the
`smp2p-modem` wake flag did not make an incoming call rouse the phone. So a
suspended phone cannot be reached over Wi-Fi, over mobile data, or by ringing
it, and suspend cannot be tested unattended.

That last one matters for using this as a phone: a handset that cannot be
rung while asleep is not much of a handset, and it is the reason suspend is
not simply switched on and left on.

### Wi-Fi does not survive it — fixed with a workaround

On suspend:

    brcmfmac: brcmf_ops_sdio_suspend: Failed to set pm_flags 1
    WARNING: at drivers/mmc/core/sdio.c:1044 mmc_sdio_suspend

The driver asks the SDIO host to keep the card powered, with
`MMC_PM_KEEP_POWER`, and the host does not support it. The chip therefore
loses power, and on resume the driver cannot reach it:

    brcmf_sdio_bus_sleep: error while changing bus sleep state -110
    brcmf_sdio_dpc: failed backplane access over SDIO, halting operation

Unbinding and rebinding the host controller re-detects the card and makes
brcmfmac download the firmware again, which brings the interface back with
its connection intact. `../userspace/udev/` has the sleep hook that does it.

The proper fix is `keep-power-in-suspend` on the Wi-Fi mmc node
(`f9864900.mmc`, which is mmc2 — `f9824900.mmc` is the eMMC and must not be
touched). That is a device tree change and so needs a new boot image.

### The touchscreen does not survive it

`suspend()` calls `set_suspend_mode()`, which puts the controller into sleep
mode. Nothing ever took it out again: the only caller of `set_resume_mode()`
was a framebuffer blank notifier, and that whole block is compiled out behind
`#if 0`. So every suspend since this port began put the touchscreen to sleep
permanently. The symptom is a device that is still listed in
`/proc/bus/input/devices` with its interrupt intact, and completely
unresponsive — the interrupt count does not move when the screen is touched.

Calling `set_resume_mode()` from the driver's resume is necessary and **not
sufficient**. With it in place the function runs, and the controller still
reports nothing. What is known after that:

- The chip answers on I2C perfectly well afterwards — `chip_id` 0x78,
  firmware 1.30.60, `config_id` 0x048D all read back.
- `enable_resume_por` is 1 in the device tree, so resume takes the
  power-on-reset path rather than the software one. Forcing that reset
  through the `por` sysfs entry reports `irq reset timeout`: the reset
  interrupt never arrives, and the driver's `reset_sem` is left held.
- Sending the software wake sequence by hand through the `command` entry —
  `SET_POWER_MODE`/`ACTIVE_MODE` then `SET_TOUCH_RPT_MODE` — does not revive
  it either.

So the controller is awake on the bus and not scanning, and neither the reset
path nor the command path restores it. Unsolved.

### The touch driver could not be re-probed at all — fixed

Recovering by reloading the driver was impossible: the i2c driver had
`.probe` and `.shutdown` but **no `.remove`**, so unbinding freed nothing.
The interrupt GPIO and the input device leaked, and any second probe failed
with `GPIO request failed for max1187x_tirq`. Its existing `shutdown()` is
already a complete teardown — sysfs, interrupt, input device, GPIOs,
regulators — and has the same signature as `remove()`. Wiring it up makes
unbind and rebind work cleanly, which gives the sleep hook a way to bring the
touchscreen back even though the driver's own resume cannot.

## ~~Light sensor reads zero~~ — it does not

Recorded here for a long time as "binds but reports 0 lux". It does not: the
APDS-9930 reports 48-86 lux and follows the room. The zero was
`net.hadess.SensorProxy`'s `LightLevel` property, which reads 0 until a
client claims the sensor. Reading the property without claiming looks
exactly like a dead sensor.

## Superseded: light sensor reads zero

The APDS-9930 is present, binds and its registers can be read, but the
illuminance channel reports 0 lux with both raw channels at zero. The
proximity side works in the sense that it responds, but its thresholds have
never been calibrated, so treat any near/far decision as unverified.

## Touch controller speaks evdev protocol A

The MAX1187x does not advertise `ABS_MT_SLOT`, which means it reports
multitouch in protocol A, not protocol B. Anything that assumes protocol B —
including most event-injection scripts written against modern touchscreens —
will have its events silently ignored. Injected presses also need a few pixels
of movement to register.

## Modem

Works. It needs `ta-service` to answer the Sony trim-area requests, or its
own watchdog kills it after about forty seconds. See `modem.md`.

## The modem's registration module can wedge, and calls stop arriving

Seen after an outgoing call that worked and carried audio both ways.
Incoming calls were then refused by the network, with "it has not been
possible to connect your call" at the calling end. ModemManager reported the
modem registered the whole time, on GSM/GPRS at 28% signal, cached.

The cause was in the kernel log, and only appeared once the modem was asked
to change power state:

    qcom-q6v5-mss fc880000.remoteproc: fatal error received:
      mmoc.c:1998:NAS(REG) did not respond to deactivate for 72s

The modem's network access stratum, the part that owns registration, had
stopped answering. The modem's own watchdog then declared a fatal error and
reloaded its firmware. After the reload the modem came back on **LTE at 68%
live signal**, where before it had been GSM/GPRS at 28% cached — so the weak
registration was the symptom of the wedge, not poor coverage.

`mmcli -m N --set-power-state-low` is what surfaced this, by asking for a
deactivate the wedged module could not service. On this modem that request
crashes it rather than turning the radio off. Use `--reset`, or let the
watchdog do it.

Note the modem re-enumerates with a new index after a restart, so anything
that addresses it needs to look the index up each time rather than hardcode
`-m 0`.

### AT+CLCC is not evidence on this modem

While chasing the above, `AT+CLCC` on `/dev/wwan0at0` reported a held call:

    +CLCC: 1,1,0,1,0,"",128

That reading is worthless here. It survived `AT+CHUP`, a `--reset`, and a
complete modem firmware reload, and it reports mode 1 (data) with an empty
number, which is not a voice call. The same port reports `+CREG: 0,2`
(searching) and `+CSQ: 99,99` even while QMI shows the modem registered and
calls work: the AT service on this modem does not track circuit-switched
state. Telephony is on the QMI port. Do not diagnose call problems from this
port, which is the mistake made here first.

## Call audio only works one way

The caller's voice comes out of the phone; nothing from the phone's
microphone reaches the far end. The application processor side of the uplink
has been eliminated by measurement — the codec's capture chain is powered and
the AFE port configured identically to an ordinary capture that records real
audio at the same moment. See `../drivers/audio/q6voice/README.md`, which
lists what else has been ruled out.

## The dialler crashes when its window closes — an upstream GTK bug

`gnome-calls` segfaults repeatedly. It is not a telephony fault and not
this port's code:

    #0  gdk_wayland_toplevel_remove_from_session (toplevel=0x0)
          at ../gdk/wayland/gdktoplevel-wayland.c:2893
    #1  gtk_application_window_removed (...)
          at ../gtk/gtkapplication.c:556

GTK passes a NULL toplevel and the Wayland backend dereferences it without a
guard. It will hit any GTK 4 application whose window is destroyed before its
surface is realised, so it is not specific to the dialler either.

Known upstream and **fixed in GTK 4.22.5**. This phone has 4.22.4, and
Alpine's edge/community has not yet packaged 4.22.5, so the fix is to wait
for the package or rebuild GTK with the upstream patch. Building GTK on the
device is impractical at 960 MHz; a cross build would be the sane route.

## Capture returns exact zeros on some attempts

Recording from the handset microphone succeeds most of the time and
occasionally returns nothing at all: the right number of frames, every sample
exactly zero, no error anywhere. Six consecutive two-second recordings gave
190, 0, 186, 0, 194, 0; a later four gave 193, 187, 166, 0. So it is not a
strict alternation.

It is not a race, and not timing. Twenty one-second captures give ten good
and ten silent, strictly alternating, and the rate is identical with a
0.3 second gap and with a 3 second gap. Something has two states and flips
on each stream.

The driver's trace is identical on a good run and a silent one — same channel
(`ch 134 6 6`), same AFE port configuration — and so is the SLIMbus log with
dynamic debug on `slim_qcom_ngd_ctrl` and the SLIMbus core: thirty lines
each, no difference. The commands issued are the same; only the result
differs.

Two explanations have been tested and both are wrong, which is worth
recording so they are not tried again. `wcd9320.c` now carries a parameter
for each:

| `disable_ports_on_stop` | `keep_stream` | good / silent out of 20 |
|---|---|---|
| Y (default) | N (default) | 11 / 9 |
| N | N | 12 / 8 |
| Y | Y | 1 / 19 |
| N | Y | 0 / 20 |

- **Disabling the slave ports on stop is not the cause.** It was added to fix
  an earlier all-zeros fault and was the obvious suspect for causing this
  one. Turning it off changes nothing beyond noise.
- **Holding the SLIMbus stream open between streams makes it far worse**, not
  better. With the stream held up, capture almost never works. So the
  teardown and rebuild is what makes data flow at all, and the theory that
  the channel activation lands one reconfiguration late is wrong: without a
  fresh activation there is no data whatsoever.

Whatever alternates is below the level the driver can see. Unfixed.

## Microphones

Capture works for the handset microphone, subject to the intermittency
above. Two faults remain: capture returns exact zeros on roughly every
other attempt, and the secondary microphone reads nothing. There is also no
headphone jack detection, which needs `CONFIG_REGMAP_IRQ`. See
`../drivers/audio/README.md` and `../drivers/audio/q6voice/README.md`.
