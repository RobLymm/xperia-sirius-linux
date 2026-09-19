# Touchscreen: Maxim MAX1187x

Last verified against the device on 2026-09-19.

The Xperia Z2's touchscreen controller. This driver is what the phone runs;
until now it existed only on the device itself, which is the reason it is
here — a working phone whose touch driver lived nowhere but the phone.

`max1187x.c` version 3.3.2.2, Maxim Integrated and Sony Mobile, GPL-2.0. It
is a downstream Android driver, not a mainline one, and it shows: it uses
`gpio_request()` rather than the managed API, carries a framebuffer blank
notifier compiled out behind `#if 0`, and has no device tree binding
document.

## Two changes from the version on the device

Both were made while chasing suspend, and anyone diffing against Sony's
original will meet them.

**`.remove` is wired to the existing `shutdown()`.** The driver had `.probe`
and `.shutdown` but no `.remove`, so unbinding it freed nothing: the
interrupt GPIO and the input device leaked, and any second probe failed with
`GPIO request failed for max1187x_tirq`. There was no way to recover the
touchscreen short of a reboot. `shutdown()` was already a complete teardown —
sysfs, interrupt, input device, GPIOs, regulators — with the same signature,
and simply was not connected.

**`resume()` now calls `set_resume_mode()`.** `suspend()` puts the controller
into sleep mode unconditionally, and the only caller of `set_resume_mode()`
was that disabled framebuffer notifier, so nothing ever woke it again. Every
suspend since this port began left the touchscreen present in
`/proc/bus/input/devices`, with its interrupt intact, and completely
unresponsive.

**Neither is sufficient on its own.** With both in place the controller still
answers on I2C after a resume and reports no touches; the power-on-reset path
times out waiting for its reset interrupt, and sending the wake sequence by
hand does not revive it either. What does work is re-probing the driver,
which the first change makes possible and which
`../../userspace/systemd/50-sirius-resume` does automatically. The underlying
resume path is unsolved; see `../../docs/known-problems.md`.

## Building it

Out of tree, against a 6.16.12 msm8974 source tree. The headers are not in
the kernel tree, hence the include path:

    make -C /path/to/linux M=$PWD

With `CONFIG_MODVERSIONS` on, an out-of-tree build needs symbol CRCs for
everything it imports; `docs/where-work-goes.md` and the device workflow
notes describe the approach used here.

Install to `/lib/modules/<version>/updates/` so it takes precedence over any
packaged copy, then `depmod -a`.

## What should happen to it

It belongs in the kernel package's patch series rather than as a hand-built
module — nothing in the package builds it today, so a packaged Z2 has no
touchscreen at all. Both changes above are genuine bug fixes and belong
upstream with it. Sony's own published source, `sonyxperiadev/kernel`, is the
origin and worth comparing against; see `../../docs/prior-art.md`.
