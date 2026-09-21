# Running the compositor in software, and why it is worth considering

The Adreno 330 on this phone stalls when **two** processes use the GPU at
once. One is fine. The full evidence is in `gpu-debug/BRIEF.md`; the short
version, measured 2026-09-21 on kernel #19:

    compositor GLES2 + app cairo     faults 0
    compositor GLES2 + app opengl    faults 3, then 4
    compositor pixman + app cairo    faults 0
    compositor pixman + app opengl   faults 0, then 0

So there are two stable configurations and one bad one. The bad one is the
obvious-looking choice: leave the compositor on the GPU, where it belongs, and
turn on GPU rendering for applications. That is what `/etc/sirius-renderer`
set to `gl` does, and it produced 596 GPU faults and six `phoc` segfaults in a
single boot, after which the session died and the phone fell back to the login
screen.

## What moving the compositor to software buys

The GPU then belongs to whatever else wants it, and the thing that wants it
most on this phone is the camera. libcamera's software ISP debayers on the GPU
by default, and with the compositor out of the way:

    compositor pixman, debayer cpu    298463 us/frame     3.4 fps
    compositor pixman, debayer gpu     18241 us/frame    54.8 fps, 0 faults

Sixteen times faster, and stable. Applications can also use `GSK_RENDERER=gl`
without faulting, which the other stable configuration does not allow.

What it costs is compositing on the CPU. The shell renders correctly --
wallpaper, icons and antialiasing all look right -- but whether it *feels*
fast enough is a judgement only someone holding the phone can make.

## How to switch it

`phosh-session` is what greetd runs for a user session, and it already passes
`WLR_BACKENDS` through, so the renderer goes in the same place:

    export WLR_RENDERER=pixman
    export WLR_RENDERER_ALLOW_SOFTWARE=1

immediately before the existing `WLR_BACKENDS` lines in
`/usr/sbin/phosh-session`. Keep a copy of the original first; the session has
to be restarted (`sudo systemctl restart greetd`) and that returns you to the
login screen.

The greeter itself runs `/usr/libexec/phrog-greetd-session`, a different
script, so the greeter stays on the GPU unless you change that too. That is
fine: the greeter is the only thing drawing at the time.

Confirm which renderer was actually created, rather than assuming:

    journalctl -b 0 -t phoc | grep -iE "Creating (GLES2|pixman) renderer"

## Put the applications back on the GPU as well

Moving the compositor to software is only half of it. Once it is there,
applications should go **back** to `GSK_RENDERER=gl` -- `/etc/sirius-renderer`
set to `gl` -- because that pairing does not fault, and leaving them on cairo
costs something expensive that is easy to miss.

With the application on the cairo renderer, GTK cannot use a GL texture, so
every camera frame that arrives as a dmabuf gets imported into GL and then
**downloaded back to the CPU**. Counted over 26 seconds of camera preview with
`GDK_DEBUG=offload,dmabuf`:

    app renderer    dmabuf imports    GPU->CPU downloads
    cairo                       11                    22
    opengl                       2                     0

Each of those is a full 1920x1080 readback, per frame, and GPU readbacks are
among the slowest things a GPU does. On the GL renderer they disappear
entirely.

So the configuration is: **compositor in software, applications on the GPU**,
which is the opposite way round from the obvious choice, and the only one of
the four combinations that is both stable and fast.

## Eight hardware planes are still going unused

The display controller has far more capability than any of this uses.
`gpu-debug/drmplanes.c` asks the kernel what it exposes:

    /dev/dri/card0: 8 planes
    plane 35 primary, planes 41..77 overlay -- all 8 accept
    NV12 NV21 NV16 NV61 VYUY UYVY YUYV YVYU YU12 YV12

Eight planes, every one of them able to scan out YUV directly. MDP5 on this
SoC has three VIG pipes with scaling and a colour space converter, three RGB
and two DMA, feeding five layer mixers, and phoc's own log says `Found 8 DRM
planes`. wlroots 0.20 has the output-layer support to drive them.

So in principle the shell panel and an application could each sit on their own
hardware plane, blended by the display controller, with **nothing** composited
by either the GPU or the CPU -- and a camera preview could be scanned out as
YUV with no conversion at all. None of that is happening today: GTK's graphics
offload never engages, so the preview goes through the compositor like any
other surface.

That is the next thing worth chasing, and it would make both the GPU fault and
the software compositing cost irrelevant rather than traded off against each
other.

## This is a workaround, not a fix

The bug is in the driver and it is being chased in `gpu-debug/BRIEF.md`. Two
GPU clients is the necessary condition, but not a sufficient one -- four
concurrent test processes do not reproduce it -- so what is special about the
compositor specifically is still open.
