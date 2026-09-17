#!/bin/sh
# Microphone test for the Xperia Z2.
#
#   sh mic-test.sh            the handset microphone (AMIC4, the main one)
#   sh mic-test.sh AMIC1      the secondary microphone
#   sh mic-test.sh AMIC2      the headset microphone (needs a headset in)
#
# Records three times and reports the loudest, because roughly every other
# capture comes back as exact zeros; see the driver README.
#
# Three things are easy to get wrong here. Each analogue input reaches one
# fixed converter, and each converter can only be selected by certain
# decimators: the pairing below is the one that works, and a wrong one leaves
# the path disconnected and records silence with no error anywhere. Capture
# has to use the MultiMedia2 front end, because MultiMedia1's playback and
# capture directions cannot both be open. And both gains start at their
# minimum, which reads as near silence.
set -u
AMIC="${1:-AMIC4}"
case "$AMIC" in
AMIC1) ADC=ADC1; DEC=DEC6 ;;
AMIC2) ADC=ADC2; DEC=DEC5 ;;
AMIC3) ADC=ADC3; DEC=DEC4 ;;
AMIC4) ADC=ADC4; DEC=DEC3 ;;
AMIC5) ADC=ADC5; DEC=DEC2 ;;
AMIC6) ADC=ADC6; DEC=DEC1 ;;
*) echo "unknown input $AMIC (expected AMIC1 to AMIC6)"; exit 1 ;;
esac

if [ ! -d /proc/device-tree/soc/sound@fe02f000/slim-capture-dai-link ]; then
	echo "The running device tree has no SLIMbus capture link. Flash an"
	echo "image that has one first."
	exit 1
fi

echo "=== route: $AMIC -> $ADC -> $DEC -> SLIM TX7 -> MultiMedia2 ==="
amixer -q -c0 cset name="$DEC MUX" "$ADC"
amixer -q -c0 cset name='SLIM TX7 MUX' "$DEC"
amixer -q -c0 cset name='SLIM TX7' 1
amixer -q -c0 cset name='MultiMedia2 Mixer SLIMBUS_0_TX' 1
# Near the top of both gains. A loud tone held against the phone clips at
# these settings; speech at arm's length does not.
amixer -q -c0 cset name="$DEC Volume" 100
amixer -q -c0 cset name="$ADC Volume" 15

measure() {
	python3 - "$1" <<'PY'
import wave, struct, math, sys
try:
	w = wave.open(sys.argv[1])
	d = w.readframes(w.getnframes())
	s = struct.unpack("<%dh" % (len(d) // 2), d)
	n = min(len(s), 24000)
	re = im = 0.0
	for i in range(n):
		a = 2 * math.pi * 1000.0 * i / 48000.0
		re += s[i] * math.cos(a)
		im -= s[i] * math.sin(a)
	print("%d %d %d" % (max(abs(x) for x in s),
		int(math.sqrt(sum(x * x for x in s) / len(s))),
		int(math.hypot(re, im) / n)))
except Exception:
	print("0 0 0")
PY
}

echo "=== quiet, three tries ==="
for t in 1 2 3; do
	rm -f /tmp/mic-quiet.wav
	arecord -q -D hw:0,3 -f S16_LE -r 48000 -c 1 -d 2 /tmp/mic-quiet.wav 2>/dev/null
	set -- $(measure /tmp/mic-quiet.wav)
	printf "  try %d: peak=%-6s rms=%-6s energy at 1 kHz=%s\n" "$t" "$1" "$2" "$3"
done

SINK=$(pactl list short sinks 2>/dev/null | grep -v auto_null | head -1 | cut -f2)
if [ -n "$SINK" ]; then
	echo "=== 1 kHz on the loudspeaker, three tries ==="
	pactl set-sink-volume "$SINK" 85% 2>/dev/null
	( timeout 20 speaker-test -D pulse -c2 -r48000 -t sine -f 1000 -l1 -P4 \
		>/dev/null 2>&1 ) &
	TONE=$!
	sleep 2
	for t in 1 2 3; do
		rm -f /tmp/mic-tone.wav
		arecord -q -D hw:0,3 -f S16_LE -r 48000 -c 1 -d 2 /tmp/mic-tone.wav 2>/dev/null
		set -- $(measure /tmp/mic-tone.wav)
		printf "  try %d: peak=%-6s rms=%-6s energy at 1 kHz=%s\n" "$t" "$1" "$2" "$3"
	done
	kill $TONE 2>/dev/null
	wait $TONE 2>/dev/null
fi

echo "A working microphone shows nothing at 1 kHz when quiet and a lot of it"
echo "with the tone playing."

# Leave the capture route off. Saved with the rest of the mixer and restored
# at the next boot, it makes the sound server's card probe touch the capture
# path, which fails the probe and loses the card.
amixer -q -c0 cset name='MultiMedia2 Mixer SLIMBUS_0_TX' 0
amixer -q -c0 cset name='SLIM TX7' 0
amixer -q -c0 cset name='SLIM TX7 MUX' ZERO
amixer -q -c0 cset name="$DEC MUX" ZERO
