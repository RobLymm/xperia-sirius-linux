# Call audio: the DSP's voice services

**The downlink works. The phone can play audio into a call. The microphone
cannot.** On a live call the caller's voice comes out of this phone, and a
recording played on this phone is heard by the far end. What does not work is
the microphone: nothing it picks up reaches the far end, and as far as is
known it never has.

Those last two are different paths, which is the useful thing to know. The
microphone reaches the far end through the voice processor's transmit device
port, which is the part that does not work. Playing audio into a call uses
the voice stream's in-call playback instead and touches neither the
microphone, the codec, nor the SLIMbus transmit port. See "Playing a
recording into a call" below. An earlier version of this
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

**It is not a command this DSP has.**
`VSS_IVOCPROC_CMD_TOPOLOGY_SET_DEV_CHANNELS`, which would tell the voice
processor how many channels each device has, does not exist in the command set
for this vintage: it is absent from the downstream header for this platform.
`VSS_IVOCPROC_CMD_SET_DEVICE_V2` (`0x000112C6`) does exist and has not been
tried.

## Why the topology is the most likely answer

Three observations, taken together, point at one explanation:

- `TOPOLOGY_ID_NONE` kills **both** directions.
- `RX_DEFAULT` carries the downlink with no calibration at all.
- Every TX topology this DSP offers — `TX_SM_ECNS`, `TX_DM_FLUENCE` — is
  silent.

`RX_DEFAULT` is a trivial topology. The TX topologies are real
echo-cancellation and noise-suppression algorithms, and an algorithm with no
coefficients loaded would output silence while accepting every command. That
fits all three observations, and it means the remaining work is the
calibration this file previously dismissed: mapping DSP-visible memory with
`VSS_IMEMORY_CMD_MAP_PHYSICAL`, parsing the vocproc and vocstrm blocks out of
Sony's ACDB data, and sending the four registration commands. It is a defined
project rather than a guess, and it is the honest next step.

The counter-argument, which is why this is "most likely" and not "the
answer": `msm8916-mainline/linux` runs a call on the same TX topology with
the same `/* TODO: Implement calibration */`. Either that DSP's firmware
carries default coefficients and this one's does not, or something else is
different.

## The route round it, and why it does not work either

Since the microphone records perfectly well on its own, and in-call playback
demonstrably reaches the far end, the obvious workaround is to capture the
microphone on the application processor and feed it back in through in-call
playback. It does not work, for a reason worth recording.

**The microphone's port cannot be captured while a call is up.** Whichever
claims `SLIMBUS_0_TX` first keeps it. A recording started during a call
returns the right number of frames of exact zeros; a recording already
running instead stops the voice session from starting at all. Three things
were tried to get round that, and none of them was enough:

- Capturing on MultiMedia2 rather than MultiMedia1, so the two directions do
  not share a front end. Necessary — using one front end for both frees its
  audio client and breaks playback until a reboot — but not sufficient.
- Giving the voice processor a different transmit port so it does not hold the
  microphone's. It will not enable with `PORT_ID_NONE`
  (`VSS_IVOCPROC_CMD_ENABLE` returns `EFAILED`), but it accepts any real
  port, so an unused one works: `tx_port=0x4003`.
- Starting the session on the playback direction alone (`require_both=N`), so
  opening the voice PCM does not start the microphone's back end at all.

With all three, and **no call in progress**, a voice session and a live
microphone capture do coexist: the session establishes and a recording made
alongside it is not zeros. With a real call in progress the recording is
zeros again. Something claims the port once a call exists, and it is not this
driver. That has not been isolated.

One practical warning. The microphone is silenced locally by pointing both
amplifiers at the unused channel, and if a capture-to-loudspeaker loop is
started *before* that is applied, the result is acoustic feedback at full
volume. Apply the amplifier routing first.

## The experiment that should come next

Everything above varies one thing at a time inside a configuration that has
never been shown to work, which is why it has produced so little. One
question separates the two possible faults and has not been asked:

**Does the transmit leg work at all, for any source?**

The FM tuner provides a known-good audio source on a different transmit port:
`SEC_MI2S_TX` (`0x1003`), which this phone already captures radio from. Point
the voice processor's transmit port at it during a call with the radio playing
(`tx_port=0x1003`) and listen at the far end.

- If the far end hears the radio, the transmit leg works, and the fault is
  specific to the microphone reaching `SLIMBUS_0_TX` — a codec and SLIMbus
  problem, not a DSP one, and a much smaller search.
- If the far end hears nothing, the transmit leg is broken whatever feeds it,
  which makes the topology-and-calibration explanation above the live one and
  says to stop looking at the codec.

Either answer removes half the search space, which none of the tests so far
has done. Worth one call before any more work.

Two cheaper things to try first, both of which need no call:

- **Scan the topology space.** Only `NONE` (`0x10F70`), `TX_SM_ECNS`
  (`0x10F71`) and the V2 variants have been tried. `TX_DM_FLUENCE`
  (`0x10F72`) has not, and the identifiers between `0x10F72` and `0x10F77`
  are undocumented here. Which ones the DSP *accepts* can be discovered
  without a call, by starting a session with `holdpcm` and reading the status
  of `VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2`. If any accepted TX
  topology is a pass-through rather than an algorithm, it would need no
  calibration.
- **`VSS_IVOCPROC_CMD_SET_DEVICE_V2`** (`0x000112C6`), sent after enable.
  Still untried, and the struct is known.

## Playing a recording into a call

**This works.** It is a separate mechanism from the microphone and does not
depend on it. `tools/call-say.sh` exercises it end to end.

One thing to know before turning it on permanently: the port it taps carries
the downlink as well, so the far end may hear itself echoed. `call-say.sh`
switches in-call playback on for its own call and off again afterwards, which
leaves ordinary calls alone. That is why the driver's `playback_port` default
is off rather than 0x1006.

`VSS_IPLAYBACK_CMD_START` (`0x000112BD`) on the voice stream names an AFE
port, and the DSP reads audio from that port and mixes it into the uplink.
The payload is a single `u16` port id. It needs a voice stream this side
owns, so `attach_stream=Y` as well.

    echo Y      > /sys/module/q6voice/parameters/attach_stream
    echo 0x1006 > /sys/module/q6voice/parameters/playback_port

`0x1006` is `QUATERNARY_MI2S_RX`, the loudspeaker port. Anything played on
the phone while a call is up is then heard by the far end. Note this is *not*
the arrangement the command was designed for — Qualcomm's own use routes a
playback stream to the AFE pseudoport `0x8005` and names that — but this DSP
is perfectly willing to tap an ordinary port, which saves plumbing a
pseudoport through `q6afe` and `q6routing`. The DSP accepts `0xFFFF`
(meaning "use the default pseudoport"), `0x8005`, `0x1006` and `0x4001`
alike, so acceptance proves nothing; `0x1006` is the one heard at the far
end, because it is the only one of them that anything actually feeds.

The drawback of using the loudspeaker port is that the phone's own speaker
plays the recording too. There is a way round that without the pseudoport,
and it is what `tools/call-say.sh` does: put the audio on the **left** channel
only and point **both** amplifiers at the **right** channel, which then
carries silence. The port still holds the audio for the DSP to take, and the
speakers reproduce nothing. Confirmed on a live call: heard at the far end,
inaudible on the phone.

    amixer -c0 cset name='Speaker Top Amp Input' Right
    amixer -c0 cset name='Speaker Bottom Amp Input' Right

It works because the two amplifiers are each wired to one channel of the same
MI2S stream and their only control is which channel they take, so pointing
both at the unused one is the nearest thing to a mute they have.

The proper fix is still the pseudoport, which nothing local consumes. Adding it means teaching `q6afe` that port `0x8005` exists
and is started with `AFE_PSEUDOPORT_CMD_START` (`0x000100BF`) rather than
`AFE_PORT_CMD_DEVICE_START`, giving it a DAI in `q6afe-dai`, and adding a
back end link in the device tree.

Note also that once the microphone path does work, playing audio out of the
loudspeaker will *not* reach the far end by that route: `TX_SM_ECNS`
subtracts the loudspeaker's own output from the microphone signal, which is
what stops the far end hearing itself. In-call playback bypasses that, which
is why it is the right mechanism for this rather than a workaround.

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
