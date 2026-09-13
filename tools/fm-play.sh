#!/bin/sh
# Play FM radio from the Xperia Z2's Broadcom tuner through the speakers.
#
#   fm-play.sh 98.9        tune and play until Ctrl-C
#
# Needs the internal FM capture port (drivers/audio 0014 patch) and a device
# tree with the "Internal FM Capture" link. Headphones must be plugged in:
# the lead is the aerial.
#
# Path: Broadcom FM I2S -> LPASS internal FM port (AFE 0x3005) -> q6routing
# "MultiMedia1 Mixer INT_FM_TX" -> capture on the card -> played back to the
# default (speaker) sink.
set -e
MHZ=${1:-98.9}
HERE=$(cd "$(dirname "$0")" && pwd)
FM="$HERE/bcm-fm.sh"
CARD=${CARD:-0}

cmd() { sudo hcitool -i hci0 cmd 0x3f 0x15 "$@" | sed -n '4p'; }

"$FM" on
"$FM" tune "$MHZ"

# Broadcom side: I2S output on (bit 5), manual mute off (bit 1), keep the rest.
cur=$(cmd 0x05 0x01 0x02 | awk '{print $8 $7}')
new=$(( (0x$cur | 0x20) & ~0x02 ))
cmd 0x05 0x00 "$(printf '0x%02x' $((new & 0xff)))" "$(printf '0x%02x' $((new >> 8)))" >/dev/null
echo "AUD_CTL0 0x$cur -> $(printf '0x%04x' $new)"

# DSP side: route the internal FM port into the MultiMedia1 capture stream.
amixer -c "$CARD" cset name='MultiMedia1 Mixer INT_FM_TX' 1

trap 'amixer -c "$CARD" cset name="MultiMedia1 Mixer INT_FM_TX" 0 >/dev/null; "$FM" off' EXIT INT TERM
echo "playing $MHZ MHz; Ctrl-C to stop"
# PipeWire owns the card, so go through it rather than the hw device.
pw-record --target "alsa_input.platform-sound.HiFi__hw_${CARD}__source" --rate 48000 --channels 2 - 2>/dev/null |
	pw-play --rate 48000 --channels 2 - 2>/dev/null ||
arecord -D "hw:$CARD,0" -f S16_LE -r 48000 -c 2 | aplay -f S16_LE -r 48000 -c 2
