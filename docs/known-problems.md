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

## Light sensor reads zero

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

## A call can leave the modem holding a stale call

Observed once, after an outgoing call that worked and carried audio in both
directions. The next incoming call was refused by the network, with "it has
not been possible to connect your call" at the calling end.

ModemManager showed no calls and reported the modem registered. The modem's
own AT interface disagreed:

    AT+CLCC  ->  +CLCC: 1,1,0,1,0,"",128

One call, inbound, state active, still held. `AT+CHUP` returned `OK` and did
not clear it. `mmcli -m 0 --reset` did clear it, and the modem came back
registered.

Not yet known: whether every call leaves this behind, or whether it was a
one-off. Worth checking `AT+CLCC` on `/dev/wwan0at0` after a call before
concluding anything about the voice driver, because a modem that believes it
is busy looks exactly like broken call handling.

One reading to avoid: on this modem the AT service does not track
circuit-switched state, so `AT+CREG?` reports `0,2` (searching) and `AT+CSQ`
reports `99,99` even while the QMI interface reports the modem registered and
calls work. Telephony is on the QMI port. Only the call list above proved
anything.

## Microphones

Capture works for the handset microphone, and call audio works in both
directions. Two faults remain: capture returns exact zeros on roughly every
other attempt, and the secondary microphone reads nothing. There is also no
headphone jack detection, which needs `CONFIG_REGMAP_IRQ`. See
`../drivers/audio/README.md` and `../drivers/audio/q6voice/README.md`.
