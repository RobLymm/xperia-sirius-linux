# ALSA UCM profile for the Z2 sound card

Without a UCM profile PipeWire falls back to a generic stereo profile. Sound
comes out, but there is no named "Speaker" port, the volume slider in the
shell does not map onto anything sensible, and the routing mixer that actually
connects the DSP to the amplifiers has to be set by hand with `amixer` after
every boot.

## Installing

    sudo mkdir -p /usr/share/alsa/ucm2/conf.d/msm8974
    sudo cp "conf.d/msm8974/Sony Xperia Z2.conf" /usr/share/alsa/ucm2/conf.d/msm8974/
    sudo cp conf.d/msm8974/HiFi.conf              /usr/share/alsa/ucm2/conf.d/msm8974/
    systemctl --user restart pipewire wireplumber

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
