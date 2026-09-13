#!/bin/sh
# Play FM radio from the Xperia Z2's Broadcom tuner through the speakers.
#
#   fm-play.sh 98.9        tune and play until Ctrl-C
#
# Needs the internal FM capture port (drivers/audio/0014) loaded and a device
# tree with the "Internal FM Capture" link (images/boot-sirius-fm.img).
# Headphones must be plugged in: the lead is the aerial.
#
# Path: Broadcom FM I2S -> LPASS internal FM port (AFE 0x3005) -> q6routing
# "MultiMedia1 Mixer INT_FM_TX" -> capture on hw:0,0 -> the desktop sound
# server's speaker sink.
#
# Capture opens the card directly: the UCM HiFi profile declares only a
# Speaker playback device, so the sound server exposes no source for it.
# Playback shares the same PCM in the other direction, which is allowed.
set -e
MHZ=${1:-98.9}
HERE=$(cd "$(dirname "$0")" && pwd)
FM="$HERE/bcm-fm.sh"
CARD=${CARD:-0}

cmd() { sudo hcitool -i hci0 cmd 0x3f 0x15 "$@" | sed -n '4p'; }

stop() {
	trap - EXIT INT TERM
	amixer -q -c "$CARD" cset name='MultiMedia1 Mixer INT_FM_TX' 0 2>/dev/null || true
	"$FM" off 2>/dev/null || true
}
trap stop EXIT INT TERM

"$FM" on
"$FM" tune "$MHZ"

# Broadcom side: I2S output on (bit 5), manual mute off (bit 1), keep the rest.
cur=$(cmd 0x05 0x01 0x02 | awk '{print $8 $7}')
new=$(( (0x$cur | 0x20) & ~0x02 ))
cmd 0x05 0x00 "$(printf '0x%02x' $((new & 0xff)))" "$(printf '0x%02x' $((new >> 8)))" >/dev/null
echo "AUD_CTL0 0x$cur -> $(printf '0x%04x' $new)"

# DSP side: route the internal FM port into the MultiMedia1 capture stream.
amixer -q -c "$CARD" cset name='MultiMedia1 Mixer INT_FM_TX' 1

echo "playing $MHZ MHz; Ctrl-C to stop"
if command -v pacat >/dev/null && pactl info >/dev/null 2>&1; then
	arecord -q -D "hw:$CARD,0" -f S16_LE -r 48000 -c 2 -t raw |
		pacat --playback --format=s16le --rate=48000 --channels=2
elif command -v pw-play >/dev/null; then
	arecord -q -D "hw:$CARD,0" -f S16_LE -r 48000 -c 2 -t raw |
		pw-play --format=s16 --rate=48000 --channels=2 -
else
	arecord -q -D "hw:$CARD,0" -f S16_LE -r 48000 -c 2 -t raw |
		aplay -q -f S16_LE -r 48000 -c 2 -t raw
fi
