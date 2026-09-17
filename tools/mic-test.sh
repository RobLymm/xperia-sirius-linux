#!/bin/sh
# Microphone test for the Xperia Z2. Needs the device tree that carries the
# SLIMbus capture link and the microphone routing.
#
#   sh mic-test.sh            the handset microphone (AMIC4, the main one)
#   sh mic-test.sh AMIC1      the secondary microphone
#   sh mic-test.sh AMIC2      the headset microphone
#
# Prints the peak and RMS of what it captured. Silence reads zero; speech a
# hand's width away should read a few thousand.
#
# Each analogue input reaches one fixed converter, and each converter can
# only be selected by certain decimators. Getting that wrong leaves the
# whole path disconnected and records silence with no error anywhere, which
# is what the mapping below is for.
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
DECNUM=${DEC#DEC}

if [ ! -d /proc/device-tree/soc/sound@fe02f000/slim-capture-dai-link ]; then
	echo "The running device tree has no SLIMbus capture link, so there is"
	echo "nothing to record from. Flash an image that has one first."
	exit 1
fi

echo "=== route: $AMIC -> $ADC -> $DEC -> SLIM TX7 -> AIF1 Capture ==="
set -e
amixer -q -c0 cset name="$DEC MUX" "$ADC"
amixer -q -c0 cset name='SLIM TX7 MUX' "$DEC"
amixer -q -c0 cset name='SLIM TX7' 1
amixer -q -c0 cset name='MultiMedia1 Mixer SLIMBUS_0_TX' 1
amixer -q -c0 cset name="DEC$DECNUM Volume" 84 2>/dev/null || true
set +e

echo "=== recording 5 s ==="
sudo dmesg -C
arecord -D hw:0,0 -f S16_LE -r 48000 -c 1 -d 5 /tmp/mic.wav 2>&1 | tail -1

python3 - <<'PY'
import wave, struct, math
try:
	w = wave.open("/tmp/mic.wav")
	d = w.readframes(w.getnframes())
	s = struct.unpack("<%dh" % (len(d) // 2), d)
	print("frames=%d peak=%d rms=%d nonzero=%d" % (len(s), max(abs(x) for x in s),
		int(math.sqrt(sum(x * x for x in s) / len(s))), sum(1 for x in s if x)))
except Exception as e:
	print("could not read the recording:", e)
PY

D="/sys/kernel/debug/asoc/Sony Xperia Z2/217:a0:1:0/dapm"
echo "=== was the path actually connected? ==="
for w in "$AMIC" "$ADC" "$DEC MUX" "SLIM TX7 MUX" "AIF1_CAP Mixer" "AIF1 CAP" "MIC BIAS1 External"; do
	printf "  %s\n" "$(sudo head -1 "$D/$w" 2>/dev/null || echo "$w: missing")"
done
echo "=== errors ==="
sudo dmesg | grep -iE "error|fail" | head -4

# Put the route back. Left on, it is saved with the rest of the mixer and
# restored at the next boot, and then the sound server touches the capture
# path while probing the card, which both fails the probe and leaves the DSP
# holding the port so nothing can open it again.
echo "=== route put back ==="
amixer -q -c0 cset name="MultiMedia1 Mixer SLIMBUS_0_TX" 0
amixer -q -c0 cset name="SLIM TX7" 0
amixer -q -c0 cset name="SLIM TX7 MUX" ZERO
amixer -q -c0 cset name="$DEC MUX" ZERO
