#!/bin/sh
# Dump a Sony IMX sensor's register space over CCI.
#
#   sensor-dump.sh <bus> <addr> <first> <last>
#
# The controller rejects block reads, so registers are read two bytes at a
# time with an explicit 16-bit register address. Reset must already be
# released and the master clock running; see docs/handover-camera.md.
set -u
BUS="${1:?usage: sensor-dump.sh <bus> <7-bit addr> <first> <last>}"
ADDR="$2"; FIRST="$3"; LAST="$4"

r=$FIRST
while [ "$r" -le "$LAST" ]; do
	hi=$(printf '0x%02x' $((r >> 8)))
	lo=$(printf '0x%02x' $((r & 0xff)))
	out=$(i2ctransfer -y -f "$BUS" w2@"$ADDR" "$hi" "$lo" r2 2>/dev/null)
	if [ -n "$out" ]; then
		a=$(echo "$out" | awk '{print $1}')
		b=$(echo "$out" | awk '{print $2}')
		printf '0x%04x %s %s\n' "$r" "$a" "$b"
	else
		printf '0x%04x -- --\n' "$r"
	fi
	r=$((r + 2))
done
