#!/bin/sh
# Headphone test for the Xperia Z2. Plug headphones in and run this; it plays
# a 440 Hz tone out of the jack and reports what the amplifiers did.
#
#   sh headphone-test.sh [seconds]
#
# The amplifier status registers are the way to tell a working path from one
# that only looks right: 0x04 means idle, 0x08 means the amplifier is being
# driven. A path with no master clock powers up, reports every stage on, and
# reads 0x04 throughout.
set -u
SECS="${1:-15}"
REGS=/sys/kernel/debug/regmap/217:a0:1:0/registers

route() {
	while [ $# -gt 0 ]; do
		amixer -q -c0 cset name="${1%=*}" "${1#*=}" || echo "  no control: ${1%=*}"
		shift
	done
}

echo "=== route to the headphone jack ==="
route \
	"QUAT_MI2S_RX Audio Mixer MultiMedia1=0" \
	"SLIM RX1 MUX=AIF1_PB" \
	"SLIM RX2 MUX=AIF1_PB" \
	"RX1 MIX1 INP1=RX1" \
	"RX2 MIX1 INP1=RX2" \
	"CLASS_H_DSM MUX=DSM_HPHL_RX1" \
	"HPHL DAC Switch=1" \
	"HPHL Volume=19" \
	"HPHR Volume=19" \
	"SLIMBUS_0_RX Audio Mixer MultiMedia1=1"

echo "=== $SECS s of 440 Hz ==="
timeout "$SECS" speaker-test -D hw:0,0 -c2 -r48000 -t sine -f 440 -P4 >/dev/null 2>&1 &
PID=$!
sleep 3
printf "amplifier status (0x04 idle, 0x08 driving): "
sudo grep -E '^(9b3|9b9):' "$REGS" | awk -F': ' '{printf "%s ", $2}'
echo
wait $PID 2>/dev/null

echo "=== back to the loudspeakers ==="
route \
	"SLIMBUS_0_RX Audio Mixer MultiMedia1=0" \
	"HPHL DAC Switch=0" \
	"CLASS_H_DSM MUX=ZERO" \
	"SLIM RX1 MUX=ZERO" \
	"SLIM RX2 MUX=ZERO" \
	"QUAT_MI2S_RX Audio Mixer MultiMedia1=1"
