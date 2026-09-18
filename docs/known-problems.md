# Known problems

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

## The GPU needs a VRAM carveout, and the size matters

msm8974 has no GPU IOMMU support in mainline, so the GPU is given a contiguous
carveout:

    cma=768M msm.vram=512m msm.allow_vram_carveout=1

With 128 MB the screen freezes once the carveout fills. 512 MB has been stable
for compositor use.

## Suspend and resume

Disabled deliberately. Resume does not restore Wi-Fi or the touchscreen, so
the phone comes back unusable and looks like it has crashed. Turning suspend
off in logind is a workaround, not a fix; the underlying problem is
unaddressed.

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

## Capture returns exact zeros on some attempts

Recording from the handset microphone succeeds most of the time and
occasionally returns nothing at all: the right number of frames, every sample
exactly zero, no error anywhere. Six consecutive two-second recordings gave
190, 0, 186, 0, 194, 0; a later four gave 193, 187, 166, 0. So it is not a
strict alternation.

The driver's trace is identical on a good run and a zero run — same channel
(`ch 134 6 6`), same AFE port configuration — so nothing is being configured
differently. That points at a race in SLIMbus channel activation rather than
a wrong value, which matches the comments already in `wcd9320.c` about
leftovers from a previous stream. Unfixed.

## Microphones

Capture works for the handset microphone, subject to the intermittency
above. Two faults remain: capture returns exact zeros on roughly every
other attempt, and the secondary microphone reads nothing. There is also no
headphone jack detection, which needs `CONFIG_REGMAP_IRQ`. See
`../drivers/audio/README.md` and `../drivers/audio/q6voice/README.md`.
