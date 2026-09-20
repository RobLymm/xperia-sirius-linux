# Out-of-tree modules: what each one is built from

Last verified against the device on 2026-09-20.

The phone runs 28 modules from `/lib/modules/<release>/updates/` that the
kernel package does not provide, or provides in a version that does not work
on this device. `updates/` takes priority over `kernel/` in modprobe's search
order, so a module placed here shadows the package's copy of the same name.

**Every one of them has to be rebuilt whenever the kernel changes.**
`CONFIG_MODVERSIONS` is on, so a module built against a different vmlinux
fails to load with `disagrees about version of symbol ...` and the subsystem
it belongs to is simply absent. That is not a warning you will see unless you
look: the phone boots, and audio or the camera is missing.

Build them with `tools/kbuild-mod.sh`, which does the two-pass modpost that
fills in the vmlinux symbol versions. `LLVM=1` is not optional — this is a
`CONFIG_CFI_CLANG` kernel.

## The map

| `updates/` directory | modules | built from |
|---|---|---|
| `cci/` | `i2c-qcom-cci` | mainline `drivers/i2c/busses/`, `CONFIG_I2C_QCOM_CCI=m` |
| `media/` | 14: `mc`, `videodev`, `v4l2-*`, `videobuf2-*`, `qcom-camss`, `imx132` | mainline `drivers/media/`, plus the camss patches in `drivers/camera/` and `drivers/camera/imx132.c` copied into `drivers/media/i2c/` |
| `pmic-clkdiv/` | `clk-spmi-pmic-div` | **`drivers/clk/pmic-clkdiv/`, not the tree's mainline file** — see below |
| `q6voice/` | 6: `q6cvp`, `q6cvs`, `q6mvm`, `q6voice-common`, `q6voice-dai`, `q6voice` | `drivers/audio/q6voice/` |
| `qdsp6-fm/` | 5: `q6afe`, `q6afe-dai`, `q6routing`, `snd-q6dsp-common`, `snd-soc-msm8974` | mainline `sound/soc/qcom/` with `drivers/audio/0014-...patch` applied, plus `drivers/audio/msm8974-sndcard.c` copied into `sound/soc/qcom/` — see below |
| `wcd9320/` | `snd-soc-wcd9320` | `drivers/audio/wcd9320/` |

Three of these are not plain rebuilds of mainline code, and each one fails in
a way that does not name its cause.

## `pmic-clkdiv` must come from this repository

Building `drivers/clk/qcom/clk-spmi-pmic-div.c` straight out of the kernel
tree produces a module that loads and then refuses to probe:

    qcom,spmi-pmic-clkdiv ...clock-controller@5b00:
        error -EEXIST: failed to register clk 'div_clk1'
    probe with driver qcom,spmi-pmic-clkdiv failed with error -17

The mainline driver hard-codes the names `div_clk1`, `div_clk2`, `div_clk3`,
and this SoC's RPM clock controller has already registered clocks under those
names. `drivers/clk/pmic-clkdiv/` holds the same driver with one change, to
read the names from `clock-output-names`; the device tree then gives it
`pm8941_div_clk1` and the collision goes away. The reason and the patch are in
that directory's README.

**This is fixed for the next build.** As of pkgrel 18 the kernel package
carries the patch as
`0033-clk-qcom-spmi-pmic-div-take-names-from-the-device-tree.patch` and sets
`CONFIG_SPMI_PMIC_CLKDIV=m`. Once a kernel built from pkgrel 18 or later is
flashed, **delete `updates/pmic-clkdiv/`** — the package provides it, and a
stale copy there shadows the working one.

What this costs when it is wrong is not a missing clock. It is **all audio**:
the WCD9320 needs its 9.6 MHz master clock before it will answer on SLIMbus at
all, so without it the codec never gets a logical address, the machine driver
never finds its codec DAI, and the card never registers. What you see is

    wcd9320-slim 217:a0:0:0: Failed to get logical address

repeated ten or more times, and `/proc/asound/cards` saying `no soundcards`.
Two of those messages followed by `WCD9320 version 8 0` is the healthy case —
the codec retries and succeeds. It is the absence of the success line, not the
presence of the failures, that means something is wrong.

Check it with:

    grep pm8941_div_clk1 /sys/kernel/debug/clk/clk_summary

which should show 9600000 and a consumer of `217:a0:1:0 mclk`.

## `qdsp6-fm` exists because the kernel package is behind

`linux-postmarketos-qcom-msm8974` does not carry
`drivers/audio/0014-ASoC-qdsp6-add-the-internal-FM-capture-port.patch`, and
the `msm8974-sndcard.c` in its patch `0008` predates the codec-less FM back
end that `drivers/audio/msm8974-sndcard.c` now has. The flashed device tree
does have the `int-fm-dai-link` node, which references q6afe DAI 137
(`INT_FM_TX`).

A kernel built without patch 0014 therefore cannot resolve that link, and the
machine driver fails the whole card:

    msm8974-snd fe02f000.sound: error -EINVAL:
        Internal FM Capture: error getting cpu dai name
    probe with driver msm8974-snd failed with error -22

Shadowing the five modules in `updates/qdsp6-fm/` is the way to run the FM
back end without a flash.

**This is fixed for the next build.** As of pkgrel 18 the kernel package
carries the patch as `0032-ASoC-qdsp6-add-the-internal-FM-capture-port.patch`,
and its `0008` has been refreshed from `drivers/audio/msm8974-sndcard.c` — it
had been behind by three pieces of work, not one: the SLIMbus channel map, the
four microphone widgets, and the codec-less FM back end. Once a kernel built
from pkgrel 18 or later is flashed, **delete `updates/qdsp6-fm/`**: the
package provides these five, and leaving stale copies there shadows working
modules with broken ones.

Until that build is flashed, these five must be rebuilt alongside everything
else.

Confirm the routing reached the card with:

    amixer -c 0 controls | grep INT_FM_TX

which should list eight `MultiMediaN Mixer INT_FM_TX` controls.

## `media` and `cci` are out-of-tree only until pkgrel 18

The kernel package had `CONFIG_MEDIA_SUPPORT` switched off entirely and did
not carry the camss driver patch — only its device tree node. That is why all
fifteen camera modules had to be built by hand.

**Fixed for the next build.** pkgrel 18 adds
`0034-media-camss-add-msm8974-support.patch` (the driver),
`0035-media-i2c-add-the-sony-imx132-sensor-driver.patch` (the source file) and
`0036-media-i2c-wire-the-imx132-driver-into-the-build.patch` (Kconfig and
Makefile), and turns on the 25 media options. After flashing pkgrel 18 or
later, **delete `updates/media/` and `updates/cci/`** as well.

`CONFIG_V4L_PLATFORM_DRIVERS=y` is the one to watch. It is a plain menu option
with no default, nothing selects it, and `VIDEO_QCOM_CAMSS` depends on it — so
without it `olddefconfig` silently drops the camss driver and the build
finishes with no camera and no error. Check a config change with:

    make LLVM=1 ARCH=arm olddefconfig && grep VIDEO_QCOM_CAMSS .config

`videobuf2-vmalloc` is in `updates/media/` but nothing on this device depends
on it and it is never loaded. It is not carried into the package.

## The imx132 source file



`drivers/camera/imx132.c` is a source file, not a patch. It has to be copied
into `drivers/media/i2c/` and wired in with
`drivers/camera/0003-media-i2c-wire-the-imx132-driver-into-the-build.patch`,
which adds the Kconfig entry and Makefile line the file never had. A tree that
still holds an older copy builds that one instead; the giveaway is a compile
error at `v4l2_event_subdev_unsubscribe`, or a driver that builds and then
produces no frames. `drivers/camera/README.md` has the checks.

`CONFIG_VIDEOBUF2_VMALLOC=m` also has to be set explicitly. It is not selected
by anything else in this configuration, and without it `videobuf2-vmalloc` is
skipped with no error.

## Checking the whole set

After any kernel change, rebuild all 28, install them, `depmod -a`, reboot,
and then:

On the first boot of a kernel built from pkgrel 18 or later, first delete
`updates/media/`, `updates/cci/`, `updates/qdsp6-fm/` and
`updates/pmic-clkdiv/` — those 21 modules are in the package now, and the old
copies would shadow them. That leaves **7** to rebuild, not 28: the six
`q6voice` modules and `snd-soc-wcd9320`, which are drivers mainline does not
have and are not in the kernel package.

    dmesg | grep -ciE "disagrees about version|Unknown symbol"   # want 0
    cat /proc/asound/cards                                       # want card 0
    ls /dev/video* /dev/media0                                   # want 7 nodes

A count of zero on the first is the only one of the three that proves the
rebuild itself was complete. The other two prove the two subsystems that these
modules exist for actually came up.
