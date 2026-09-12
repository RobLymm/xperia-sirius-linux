# FM radio driver for the WCNSS tuner

`radio-wcnss-fm.c` is a V4L2 radio driver for the FM receiver inside the
WCN3680, written for this project. **It has never been run.** The phone was
unreachable while it was written, so it has not been compiled against a kernel
tree, loaded, or tested against hardware. Treat it as a first draft that is
structurally complete, not as working code.

It is here because FM was the one subsystem on this phone with no existing
implementation anywhere: not in mainline, not in the msm8974-mainline fork,
and listed as "No driver" in the Fairphone 2 support table by someone who has
worked on this SoC for years.

## How it works

The tuner is not a separate chip. It sits inside the WCN3680 next to Wi-Fi and
Bluetooth, and the AP reaches it over a single SMD channel named **`APPS_FM`**
on the WCNSS edge, carrying an HCI-like command and event protocol.

That makes the driver a sibling of `btqcomsmd`, which does the same thing for
Bluetooth over `APPS_RIVA_BT_CMD` and `APPS_RIVA_BT_ACL`. The WCNSS control
driver calls `of_platform_populate()` on its own node, so a child node in the
device tree becomes a platform device; the driver then takes the WCNSS handle
from its parent and calls `qcom_wcnss_open_channel()`.

Frames are `type(1) | opcode(2, little endian) | plen(1) | payload`, with
`0x11` for commands and `0x14` for events. Opcodes pack an opcode group in the
top six bits and a command code in the low ten: receiver control is group
`0x13`, common control `0x15`.

Implemented: enable and disable the receiver on first open and last close,
tune, read station parameters, mute, mono/stereo, and hardware seek. Not
implemented: RDS, alternate frequencies, the station list, and the transmitter
side. Those are all in the downstream driver if someone wants them.

## Where the protocol came from

`radio-iris`, the downstream Qualcomm driver, which was never upstreamed. From
the Sony msm8974 Android kernel:

    include/media/radio-iris.h          opcodes, event codes, payload structs
    drivers/media/radio/radio-iris.c    the HCI and V4L2 logic, 5261 lines
    drivers/media/radio/radio-iris-transport.c   the SMD transport, 243 lines

The transport file is where the channel name comes from:

    smd_named_open_on_edge("APPS_FM", SMD_APPS_WCNSS, &hsmd->fm_channel, ...)

This driver is a reimplementation against the modern rpmsg API rather than a
port; the downstream one is built on the old SMD interface that no longer
exists, and carries a great deal that a mainline driver should not.

## Device tree

The node goes under the WCNSS control node, beside the existing `bt` and
`wifi` children:

```dts
&wcnss {
	fm {
		compatible = "qcom,wcnss-fm";
	};
};
```

No resources of its own: the channel comes from the parent.

## Building it

Out of tree it needs `CONFIG_QCOM_WCNSS_CTRL`, `CONFIG_RPMSG`, `CONFIG_VIDEO_DEV`
and `CONFIG_RADIO_ADAPTERS`. In tree it wants a Kconfig entry:

```
config RADIO_WCNSS_FM
	tristate "Qualcomm WCNSS FM receiver"
	depends on VIDEO_DEV && RADIO_ADAPTERS
	depends on QCOM_WCNSS_CTRL || COMPILE_TEST
	select RPMSG
	help
	  V4L2 radio driver for the FM receiver inside the Qualcomm WCNSS
	  coprocessor, found on msm8974 and similar platforms.
```

and a Makefile line in `drivers/media/radio/`.

## What to check first when it is tried

In rough order of how likely each is to be wrong:

1. **Whether the WCNSS firmware exposes `APPS_FM` at all.** Bluetooth and
   Wi-Fi work on this phone, so the coprocessor is up, but the FM service may
   need something else enabled first. If the channel never opens, nothing else
   matters.
2. **The command-complete payload layout.** The driver assumes
   `num_pkts(1), opcode(2), status(1)` and matches the returned opcode against
   the one it sent. If completions never match, that assumption is wrong and
   every command will time out after five seconds.
3. **The enable payload.** `struct fm_recv_conf` is taken from the downstream
   header and is the most structured thing sent; if the receiver rejects it,
   compare field by field against `hci_fm_recv_conf_req`.
4. **The antenna selection.** Set to 0, the headset wire. On a phone the
   headphone lead is the aerial, so FM will not work at all without
   headphones plugged in, whatever the driver does.
5. **The audio path.** This driver only controls the tuner. Getting sound out
   also needs the ADSP routing that Sony's tree exposes as
   `qcom,msm-dai-q6-int-fm-rx`, wired into the sound card. Expect a working
   `/dev/radio0` with silence until that is done.

A first test needs no application:

    v4l2-ctl -d /dev/radio0 --all
    v4l2-ctl -d /dev/radio0 --set-freq=98.1
    v4l2-ctl -d /dev/radio0 --get-freq
