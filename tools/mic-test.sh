#!/bin/sh
# Microphone test. Run after flashing boot-codec-v5.img, which is the image
# that carries the SLIMBUS_0_TX link and the microphone routing.
#
#   sh ~/mic-test.sh            record the handset microphone for 5 seconds
#   sh ~/mic-test.sh AMIC1      a different analogue input
#
# Prints the peak and RMS of what it captured. Silence reads near zero;
# anything above a few hundred RMS is real sound.
set -u
AMIC="${1:-AMIC4}"
case "$AMIC" in
AMIC1) ADC=ADC1 ;; AMIC2) ADC=ADC2 ;; AMIC3) ADC=ADC3 ;;
AMIC4) ADC=ADC4 ;; AMIC5) ADC=ADC5 ;; AMIC6) ADC=ADC6 ;;
*) echo "unknown input $AMIC"; exit 1 ;;
esac

echo "=== is the capture link present? ==="
if [ ! -d /proc/device-tree/soc/sound@fe02f000/slim-capture-dai-link ]; then
	echo "The running device tree has no SLIMbus capture link, so there is"
	echo "nothing to record from. Flash images/boot-codec-v5.img first."
	exit 1
fi
cat /proc/asound/pcm

echo "=== route $AMIC -> $ADC -> DEC1 -> SLIM TX7 -> AIF1 Capture ==="
set -x
amixer -q -c0 cset name='DEC1 MUX' "$ADC"
amixer -q -c0 cset name='SLIM TX7 MUX' DEC1
amixer -q -c0 cset name='SLIM TX7' 1
amixer -q -c0 cset name='MultiMedia1 Mixer SLIMBUS_0_TX' 1
amixer -q -c0 cset name='DEC1 Volume' 84 2>/dev/null
set +x

echo "=== recording 5 s ==="
sudo dmesg -C
arecord -D hw:0,0 -f S16_LE -r 48000 -c 1 -d 5 /tmp/mic.wav 2>&1 | tail -2

python3 - <<'PY'
import wave, struct, math
try:
	w = wave.open("/tmp/mic.wav")
	n = w.getnframes()
	d = w.readframes(min(n, 240000))
	s = struct.unpack("<%dh" % (len(d) // 2), d)
	if s:
		print("frames=%d peak=%d rms=%d" % (n, max(abs(x) for x in s),
			int(math.sqrt(sum(x * x for x in s) / len(s)))))
	else:
		print("frames=%d, no samples" % n)
except Exception as e:
	print("could not read the recording:", e)
PY

echo "=== bias and converter registers ==="
R=/sys/kernel/debug/regmap/217:a0:1:0/registers
for r in 880 881 882 883 884 885 886 887; do printf "%s=%s " "$r" "$(sudo grep -E "^$r:" $R | cut -d' ' -f2)"; done
echo
echo "=== errors ==="
sudo dmesg | grep -iE "error|fail" | head -6
