#!/bin/sh
# Play FM radio from the Xperia Z2's Broadcom tuner through the speakers.
#
#   fm-play.sh 98.9        tune and play until Ctrl-C
#
# Needs the internal FM capture port (drivers/audio/0014) loaded, the machine
# driver that makes the FM link a back end, and a device tree with the
# "Internal FM Capture" link and a MultiMedia2 capture front end.
# Headphones must be plugged in: the lead is the aerial.
#
# Path: Broadcom FM I2S (chip is clock master) -> secondary MI2S pads gpio79-81
# -> LPASS SECONDARY_MI2S_TX (clock consumer) -> q6routing
# "MultiMedia2 Mixer SEC_MI2S_TX" -> capture on MultiMedia2 -> the desktop sound
# server's speaker sink.
#
# FM must not use MultiMedia1. The sound server holds it open for speaker
# playback, a q6asm session serves one direction at a time, and q6routing
# keeps a single route per front end, so routing MultiMedia1 capture would
# also take the speaker's route away.
#
# Capture opens the card directly: the UCM HiFi profile declares only a
# Speaker playback device, so the sound server exposes no source for it.
set -e
MHZ=${1:-98.9}
HERE=$(cd "$(dirname "$0")" && pwd)
FM="$HERE/bcm-fm.sh"
CARD=${CARD:-0}
# The MultiMedia2 capture PCM, found by name: its device number follows the
# order of the sound card's links in the device tree (3 on the FM variant).
DEV=$(awk -F'[-:]' -v c="$(printf %02d "$CARD")" \
	'$1 == c && $3 ~ /^ MultiMedia2/ && /capture/ { print $2 + 0; exit }' /proc/asound/pcm)
[ -n "$DEV" ] || { echo "no MultiMedia2 capture PCM: flash boot-sirius-fm2.img"; exit 1; }
# Read through the fmrepair ALSA plugin when it is installed: it repairs the
# tuner's periodic sign-bit corruption (the 41.6 Hz "flicking") at the device
# layer (drivers/audio/fmrepair). Raw device otherwise.
if [ -e /usr/lib/alsa-lib/libasound_module_pcm_fmrepair.so ]; then
	PCM="sirius_fm:CARD=$CARD,DEV=$DEV"
else
	PCM="hw:$CARD,$DEV"
fi

cmd() { sudo hcitool -i hci0 cmd 0x3f 0x15 "$@" | sed -n '4p'; }

stop() {
	trap - EXIT INT TERM
	amixer -q -c "$CARD" cset name='MultiMedia2 Mixer SEC_MI2S_TX' 0 2>/dev/null || true
	sudo hcitool -i hci0 cmd 0x3f 0x61 0x01 0x19 0x18 0x18 0x18 >/dev/null 2>&1 || true
	"$FM" off 2>/dev/null || true
}
trap stop EXIT INT TERM

"$FM" on
"$FM" tune "$MHZ"

# Broadcom side: PCM pads carry FM I2S with the chip as clock master (0xFC61),
# PCM_ROUTE bit 7 (to SCO) clear, volume 255, AUD_CTL0 0x2c = I2S on, left and
# right un-muted, 50 us de-emphasis. Broadcom's driver always sets the two
# un-mute bits; without them the samples are zero.
sudo hcitool -i hci0 cmd 0x3f 0x61 0x05 0x19 0x18 0x18 0x18 >/dev/null
route=$(cmd 0x4d 0x01 0x01 | awk '{print $7}')
cmd 0x4d 0x00 "$(printf '0x%02x' $((0x$route & 0x7f)))" >/dev/null
cmd 0xf8 0x00 0xff 0x00 >/dev/null
cmd 0x05 0x00 0x2c 0x00 >/dev/null
echo "AUD_CTL0 $(cmd 0x05 0x01 0x02)"

# DSP side: route the secondary MI2S port into the MultiMedia2 capture stream.
amixer -q -c "$CARD" cset name='MultiMedia2 Mixer SEC_MI2S_TX' 1

# Sum to mono if the downmix helper is on PATH: FM stereo on a weak signal
# carries the L-R difference as noise, and the mono sum drops it.
if command -v sirius-fm-downmix >/dev/null; then DOWN=sirius-fm-downmix; CH=1; else DOWN=cat; CH=2; fi

echo "playing $MHZ MHz; Ctrl-C to stop"
if command -v pacat >/dev/null && pactl info >/dev/null 2>&1; then
	arecord -q -D "$PCM" -f S16_LE -r 48000 -c 2 -t raw | $DOWN |
		pacat --playback --format=s16le --rate=48000 --channels=$CH
elif command -v pw-play >/dev/null; then
	arecord -q -D "$PCM" -f S16_LE -r 48000 -c 2 -t raw | $DOWN |
		pw-play --format=s16 --rate=48000 --channels=$CH -
else
	arecord -q -D "$PCM" -f S16_LE -r 48000 -c 2 -t raw | $DOWN |
		aplay -q -f S16_LE -r 48000 -c "$CH" -t raw
fi
