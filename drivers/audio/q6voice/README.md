# Call audio: the DSP's voice services

**The downlink works. The uplink does not.** On a live call the caller's
voice comes out of this phone. Nothing from this phone's microphone reaches
the far end, and as far as is known it never has. An earlier version of this
file claimed both directions worked, on the strength of one call report that
was later withdrawn; treat "it establishes" and "it carries audio" as
entirely separate claims here, because this driver is very good at the first.

On Qualcomm platforms of this generation the modem hands voice audio to the
audio DSP, and the DSP will not carry it until its voice services have been
driven over the message interface: a multi-mode voice manager, which owns the
session, and a core voice processor, which connects the session to a
microphone and a speaker. There is a third, the core voice stream, which
matters for VoIP and not for a call through the modem — see below.

Mainline Linux has none of that. Its `qdsp6` stack has `q6asm`, `q6adm`,
`q6afe`, `q6routing` and `q6core`, and no voice anything: no voice ports, no
voice mixers, no voice session types. It is not a disabled config option, the
concept is absent.

This is Stephan Gerhold's `q6voice` driver, adapted for this phone. Two
versions of it exist and they differ in ways that matter: the `wip/q6voice`
branch of `sdm845-mainline/linux`, which this port started from and whose own
newest commits read "cvp enable fails" and "let's debug", and the one carried
in `msm8916-mainline/linux`, which works. Where they disagree, the msm8916
one is right.

## The sequence that carries a call

    create voice manager session, passive   0x000110FF
    set dual control policy                 0x00011327
    create voice processor, V2              0x000112BF
    enable voice processor                  0x000100C6
    attach voice processor to manager       0x0001123E
    start voice                             0x00011190

Six commands, and no calibration data of any kind. The voice processor is
created with the `TX_SM_ECNS` and `RX_DEFAULT` topologies, tx port
`SLIMBUS_0_TX`, rx port `QUAT_MI2S_RX`, profile `CAL_NETWORK_ID_NONE`, mode
`EC_INT_MIXING`.

That is enough for the downlink. What the uplink additionally needs is not
known; the section below records what it is not, so the same ground is not
covered again.

## What the adaptation needed

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

3. **The topology.** This is the one that decides whether a call has sound.
   See the next section.

4. **Not creating a voice stream.** A call through the modem is the modem's
   stream: the modem creates its own and attaches it to the passive manager
   session, which it finds by name. Creating one on this side as well and
   attaching that puts an empty stream in the slot the modem's should
   occupy — which starts cleanly, reports success for every command, and
   moves no audio in either direction. `attach_stream` is a parameter,
   default off; the code stays for a stream this side really does own, such
   as VoIP.

## The topology, and a wrong conclusion worth recording

Earlier notes in this file said calibration was the missing piece. That was
wrong, and the way it was wrong is worth keeping, because the evidence for it
looked good.

The voice processor takes a processing topology for each direction. Three
things are true of this DSP, each established by asking it:

- `TOPOLOGY_ID_NONE` (`0x00010F70`) is **accepted**, and produces a voice
  processor containing no processing. The session establishes completely,
  every command returns success, the voice PCM appears, both ports come up,
  and the codec's microphone chain powers on and stays on for the length of a
  call. No audio reaches either end. It is the most misleading result
  available here: nothing looks wrong anywhere.
- The **V2** topologies, `TX_SM_ECNS_V2` (`0x00010F89`) among them, are
  rejected with `EBADPARAM`. These are the ones that need calibration loaded
  into the DSP first.
- `TX_SM_ECNS` (`0x00010F71`) with `RX_DEFAULT` (`0x00010F77`) is **accepted**
  and carries audio both ways, with no calibration at all.

The wrong conclusion came from testing the V2 topology, seeing it refused,
and reading that as "real topologies need calibration". The plain topology
one number lower had never been tried. `msm8916-mainline/linux` uses exactly
that pair and carries a comment reading `/* TODO: Implement calibration */`,
which settles the question: calibration is not required for call audio on
this generation of DSP.

Calibration is still worth having eventually — it is what the tuned
topologies use, and what Sony's own stack loads from its `acdbdata` blobs —
but it buys audio quality, not audio.

## The uplink: what it is not

Everything below was measured on this phone, and none of it fixed the uplink.
It is recorded because each one looked like the answer.

**It is not the application processor side.** During a voice session the
codec's whole capture chain reads powered — `AMIC4`, `ADC4`, `DEC3 MUX`,
`SLIM TX7 MUX`, `AIF1_CAP Mixer`, `AIF1 CAP`, `AIF1 Capture`, all `On in 2
out 1` — identically to an ordinary capture that records real audio. The AFE
port configuration is identical too, byte for byte: `port 0x4001, rate 48000,
nch 1, map 134`. The same codec route, at the same moment, delivers RMS 200
of room noise to an `arecord` on MultiMedia1.

**It is not the port start ordering,** although there was a real bug there.
The session used to start from the DAI's `startup` callback, which runs when
the PCM is opened, before `hw_params` has configured the back ends: the
uplink port was configured about thirty milliseconds *after* the voice
processor had been created naming it. The downlink never suffered from this,
because its port carries ordinary loudspeaker playback and so is already
running. That asymmetry matched the symptom exactly, and fixing it (the
session now starts from `prepare`) changed nothing audible. The fix is kept
because the old order was indefensible, not because it helped.

**It is not a mute.** `VSS_IVOLUME_CMD_MUTE_V2` is sent for both directions
at session start, accepted with status 0. Silence either way.

**It is not the echo canceller.** `TX_SM_ECNS` is single-microphone echo
cancellation and noise suppression, and an echo canceller with a bad
reference cancels everything, which would silence the uplink and leave the
downlink alone. Setting `tx_topology` to `TOPOLOGY_ID_NONE` while leaving
`rx_topology` at `RX_DEFAULT` is accepted by the DSP and makes no difference.

**It is not the missing voice stream.** Creating an AP-side CVS and attaching
it (`attach_stream=Y`) alongside the real topologies is accepted in full —
`0x11140`, `0x1123c`, `0x112bf` all status 0 — and is also silent. This is
worth stating because it is the combination Sony's own driver for this
platform uses, and because it was never tested in isolation: the topologies
were fixed and the stream removed in the same change.

**It is not the 48 kHz back end rate,** most likely. The machine driver's
`be_hw_params_fixup` forces every back end to 48 kHz while the voice front
end declares 8 kHz. That looked suspicious, but a voice processor is a
device-side processor that resamples between the device ports and the
vocoder, and downstream runs 48 kHz device ports on this generation too.
Untested rather than eliminated.

Candidates not yet tried: `VSS_IVOCPROC_CMD_TOPOLOGY_SET_DEV_CHANNELS`, to
tell the voice processor how many channels each device has, and
`VSS_IVOCPROC_CMD_SET_DEVICE_V2` after enable.

## Playing a recording into a call

Audio played out of the loudspeaker during a call does not reach the far end,
and will not even once the uplink works: `TX_SM_ECNS` subtracts the
loudspeaker's own output from the microphone signal, which is what stops the
far end hearing itself. Sending a recording down the line is a separate
feature needing the DSP's in-call playback command,
`VSS_IPLAYBACK_CMD_START` on the voice stream. That requires an AP-side
stream, which is why the `attach_stream` code is kept rather than deleted.

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
success; the codes are in `q6dsp-errno.h`. Three are worth recognising: 3,
unsupported, which means the wrong command version; 2, bad parameter, which
for the voice processor means a topology this DSP will not accept without
calibration; and 1, failed.

Note that success everywhere proves very little on its own. The silent
configuration described above returns `status: 0x0` for all six commands.
