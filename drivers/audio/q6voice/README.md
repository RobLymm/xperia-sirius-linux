# Call audio: the DSP's voice services

A call on this phone connects and carries no sound, because the audio does
not go through the processor at all. On Qualcomm platforms of this generation
the modem hands voice audio to the audio DSP, and the DSP will not carry it
until three of its services have been driven over the message interface: a
multi-mode voice manager, which owns the session, a core voice stream, which
is the side the modem talks to, and a core voice processor, which connects the
session to a microphone and a speaker.

Mainline Linux has none of that. Its `qdsp6` stack has `q6asm`, `q6adm`,
`q6afe`, `q6routing` and `q6core`, and no voice anything: no voice ports, no
voice mixers, no voice session types. It is not a disabled config option, the
concept is absent.

This is Stephan Gerhold's `q6voice` driver, taken from the `wip/q6voice`
branch of `sdm845-mainline/linux` and adapted for this phone.

## State

**Not finished.** The session establishes completely and carries no audio.
Every command the DSP is sent is accepted:

    create voice manager session      0x000110FF  ok
    create voice stream session       0x00011140  ok
    attach stream to manager          0x0001123C  ok
    set dual control policy           0x00011327  ok
    create voice processor, V2        0x000112BF  ok
    enable voice processor            0x000100C6  ok
    attach voice processor            0x0001123E  ok
    start voice                       0x00011190  ok

and the teardown is equally clean. A voice PCM appears, the two ports it
connects come up, and the codec's microphone chain powers on and stays on for
the length of a call. The far end hears nothing and neither does this phone.

**What is almost certainly missing is calibration.** The voice processor is
created with a processing topology for each direction, and a real topology is
refused unless the platform's calibration data has been loaded into the DSP
first. Tested directly: with the echo-cancellation topology the create
command fails, and with `VSS_IVOCPROC_TOPOLOGY_ID_NONE` it succeeds. So what
runs is a voice processor with no processing, which may well be one that
connects nothing. Loading calibration means mapping Sony's `acdbdata` blobs
into the DSP and registering them, and it is the last thing in Sony's own
start sequence that this driver does not do.

## What the adaptation to this phone needed

Three changes, each of which the branch this came from gets wrong for a 2014
DSP. Its own newest commits read "cvp enable fails" and "let's debug", so
none of this was working there either.

1. **The session type.** The branch hardcodes VoiceMMode2, a two-SIM
   multi-mode session from much newer hardware. msm8974 predates those by
   years and wants session 0, "default modem voice", the classic
   circuit-switched session. Now the `voice_path` parameter on
   `q6voice-dai`, defaulting to 0.

2. **The command version.** It sends the create-session command for the voice
   processor as V3, `0x00013169`. This DSP answers `ADSP_EUNSUPPORTED`. The
   version it implements is V2, `0x000112BF`, which is what Sony's own kernel
   for this platform uses, and the struct the branch already had matches V2
   field for field.

3. **The missing command.** The driver attaches the voice processor to the
   manager but never attaches the *stream*. A manager session with a
   processor and no stream starts without complaint and moves no audio in
   either direction. `VSS_IMVM_CMD_ATTACH_STREAM`, `0x0001123C`, with the
   matching detach on teardown.

The ports the voice processor connects, which the driver hardcodes, happen to
be right for this phone already: `SLIMBUS_0_TX` for the microphone and
`QUAT_MI2S_RX` for the loudspeakers.

## Porting to 6.16

- `strlcpy` has been removed from the kernel; `strscpy` replaces it.
- An APR driver's `remove` callback returns `void` rather than `int`.
- Each file is its own module, as upstream builds them.

## The userspace side, and a trap in it

Opening the voice PCM is what starts the session, and something has to do
that when a call begins. postmarketOS packages `q6voiced` for the job, which
watches ModemManager over D-Bus.

**It opens the PCM and lets it close again.** The moment it closes, the back
ends are torn down and the microphone and its SLIMbus port power off, about a
second after the call connects, which looks exactly like a driver that does
not work. `voicehold.sh` here holds it open for as long as a call is active
instead, and `holdpcm.c` is the small program that holds a PCM open without
reading or writing it, since no data passes through the processor.

## Building

    make -C /path/to/linux M=$PWD

Needs the three service nodes and a voice front end in the device tree; see
`devicetree/qcom-msm8974pro-sony-xperia-sirius-codec.dts`.

## Watching what the DSP says

Nothing is logged unless the driver's debug statements are switched on:

    for m in q6voice q6voice_common q6mvm q6cvs q6cvp q6voice_dai; do
        echo "module $m +p" | sudo tee /sys/kernel/debug/dynamic_debug/control
    done

Then every command and its status appears in the kernel log. `status: 0x0` is
success; the codes are in `q6dsp-errno.h`, and the two worth recognising are
3, unsupported, which means the wrong command version, and 1, failed, which
for the voice processor means the topology wants calibration.
