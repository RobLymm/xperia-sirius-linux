# ALSA UCM profile for the Z2 sound card

Without a UCM profile PipeWire falls back to a generic stereo profile. Sound
comes out, but there is no named "Speaker" port, the volume slider in the
shell does not map onto anything sensible, and the routing mixer that actually
connects the DSP to the amplifiers has to be set by hand with `amixer` after
every boot.

## Installing

    D=/usr/share/alsa/ucm2
    sudo mkdir -p $D/Qualcomm/sony-sirius $D/conf.d/Sony_Xperia_Z2
    sudo cp Qualcomm/sony-sirius/*.conf $D/Qualcomm/sony-sirius/
    sudo ln -sf ../../Qualcomm/sony-sirius/sony-sirius.conf \
        $D/conf.d/Sony_Xperia_Z2/Sony_Xperia_Z2.conf
    pulseaudio -k

The profile is looked up by the card's name with spaces replaced by
underscores, which is why the conf.d directory is `Sony_Xperia_Z2`.

## Why the speaker route is in the verb

PulseAudio opens the PCM to probe a profile before it enables any of that
profile's devices. On this card the front end cannot open at all unless a back
end is routed, so probing failed, the card dropped to a null sink, and the
phone had no audio until something set the routing mixer by hand. Putting the
speaker route in the verb's EnableSequence, as well as in the Speaker device,
means the card always loads.

Use `#` for comments in these files. A `/* */` block makes the whole profile
fail to parse, and the only symptom is a card that will not load.

## The headphone jack is not a device here

It works, and the mixer settings that reach it are known, but it is left out
of this profile on purpose.

Both the speaker and the headphone jack play through the same PCM, `hw:0,0`;
only the routing controls differ. PulseAudio turns two UCM devices that share
a PCM into two card profiles rather than two ports on one sink. Switching card
profile tears the sink down and builds a new one, and the profile being
switched to only opens if the hardware is already routed the way that profile
expects: the DSP refuses to start the SLIMbus port when the codec's own muxes
are not already pointing at it, so the PCM fails with `-EINVAL` and the card
falls back to a null sink. The result was a phone that lost its audio
whenever the output was changed.

Two devices as two *ports* would avoid all of it, since a port change does not
recreate the sink, but PulseAudio only does that for devices that can be
active at the same time. Until that is worked out, the headphone route is set
by whatever wants it.

## Checking that it is found

UCM looks the profile up by the card's **driver name**, not its model string.
The machine driver in `../../drivers/audio/` sets `card->driver_name` to
`msm8974` for exactly this reason, which is what every other qcom machine
driver does. If the card was built before that change, the driver name will
instead be derived from the model and the directory above will not match.

    cat /proc/asound/card0/id          # card id, expected: Z2
    alsaucm -c "Sony Xperia Z2" list _verbs
    alsaucm -c "Sony Xperia Z2" set _verb HiFi set _enadev Speaker

If `list _verbs` prints nothing, the lookup path is wrong rather than the
profile being wrong; check the driver name the card actually registered with.

## Not yet verified on hardware

The profile was written from the device tree and the driver sources while the
phone was unavailable, so the control name has not been confirmed against a
running card. One line matters:

    QUAT_MI2S_RX Audio Mixer MultiMedia1

That name is built by q6routing from the back end name and the front end name,
and both are fixed by the device tree, so it should be right. Confirm with:

    amixer -c 0 scontrols | grep -i 'audio mixer'

The two amplifiers carry `sound-name-prefix` of "Speaker Top" and "Speaker
Bottom", so their own controls appear with those prefixes. None of them need
setting for playback to work — the TFA989x driver powers the amplifier from
DAPM — but they are where to look if only one channel is audible.
