#!/bin/sh
# Play audio into a voice call.
#
# This exists to exercise the phone's in-call playback capability: audio
# generated on the phone reaching the far end of a call. Text is spoken with
# piper, or a wav file is played with -f.
#
# The audio goes into the call through the DSP's in-call playback, which mixes
# an AFE port into the uplink. No microphone is involved. The phone's own
# loudspeaker stays silent because the speech is put on the left channel only
# while both amplifiers are pointed at the right channel, which carries
# nothing: the port still holds the audio for the DSP to take.
#
#   call-say.sh 07700900123 "This is a test of the call audio path."
#   call-say.sh 07700900123 -f /path/to/recording.wav
#
# Options:
#   -r N   say it N times (default 1)
#   -v V   piper voice name (default the southern English female one)
#   -w N   seconds to wait for an answer (default 45)
#
# Playback starts as soon as the call goes active, with no lead-in.

set -u

VOICE_DIR="$HOME/piper-voices"
VOICE="en_GB-southern_english_female-low"
REPEAT=1
WAIT=45
WAVIN=""
PLAYBACK_PORT=0x1006          # QUATERNARY_MI2S_RX, the loudspeaker port
WORK=$(mktemp -d)

usage() { sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

NUM=""
TEXT=""
while [ $# -gt 0 ]; do
	case "$1" in
	-f) WAVIN="${2:?-f needs a file}"; shift 2 ;;
	-r) REPEAT="${2:?-r needs a count}"; shift 2 ;;
	-v) VOICE="${2:?-v needs a voice}"; shift 2 ;;
	-w) WAIT="${2:?-w needs seconds}"; shift 2 ;;
	-h|--help) usage ;;
	*) if [ -z "$NUM" ]; then NUM="$1"; else TEXT="${TEXT:+$TEXT }$1"; fi; shift ;;
	esac
done
[ -n "$NUM" ] || usage
[ -n "$TEXT" ] || [ -n "$WAVIN" ] || usage

# ---- what to restore, whatever happens ---------------------------------
TOP_WAS=$(amixer -c0 cget name='Speaker Top Amp Input' 2>/dev/null |
	  sed -n 's/^  : values=//p' | head -1)
BOT_WAS=$(amixer -c0 cget name='Speaker Bottom Amp Input' 2>/dev/null |
	  sed -n 's/^  : values=//p' | head -1)
CALL=""
MODEM=""

cleanup() {
	[ -n "$CALL" ] && sudo mmcli -m "$MODEM" --call "$CALL" --hangup >/dev/null 2>&1
	[ -n "$TOP_WAS" ] && amixer -q -c0 cset name='Speaker Top Amp Input' "$TOP_WAS" 2>/dev/null
	[ -n "$BOT_WAS" ] && amixer -q -c0 cset name='Speaker Bottom Amp Input' "$BOT_WAS" 2>/dev/null
	echo 0 | sudo tee /sys/module/q6voice/parameters/playback_port >/dev/null 2>&1
	rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

# ---- the speech --------------------------------------------------------
if [ -z "$WAVIN" ]; then
	[ -f "$VOICE_DIR/$VOICE.onnx" ] || { echo "no such voice: $VOICE" >&2; exit 1; }
	printf '%s\n' "$TEXT" |
		piper --model "$VOICE_DIR/$VOICE.onnx" \
		      --output_file "$WORK/say.wav" >/dev/null 2>&1 ||
		{ echo "piper failed" >&2; exit 1; }
	WAVIN="$WORK/say.wav"
fi
[ -f "$WAVIN" ] || { echo "no such file: $WAVIN" >&2; exit 1; }

# Left channel carries the audio; right channel is silence for the speakers.
python3 - "$WAVIN" "$WORK/left.wav" <<'PY' || exit 1
import sys, wave
src = wave.open(sys.argv[1])
if src.getsampwidth() != 2:
    sys.exit("need 16-bit audio")
mono = src.getnchannels() == 1
d = src.readframes(src.getnframes())
step = 2 if mono else 4
out = bytearray()
for i in range(0, len(d) - step + 1, step):
    out += d[i:i+2]     # left: the speech
    out += b"\x00\x00"  # right: silence, which is what the speakers get
dst = wave.open(sys.argv[2], "wb")
dst.setnchannels(2)
dst.setsampwidth(2)
dst.setframerate(src.getframerate())
dst.writeframes(bytes(out))
dst.close()
PY

# ---- the call ----------------------------------------------------------
systemctl is-active --quiet sirius-voicehold ||
	sudo systemctl start sirius-voicehold 2>/dev/null

echo Y | sudo tee /sys/module/q6voice/parameters/attach_stream >/dev/null
echo $((PLAYBACK_PORT)) | sudo tee /sys/module/q6voice/parameters/playback_port >/dev/null

amixer -q -c0 cset name='Speaker Top Amp Input' Right
amixer -q -c0 cset name='Speaker Bottom Amp Input' Right

MODEM=$(sudo mmcli -L 2>/dev/null | grep -oE '/Modem/[0-9]+' | grep -oE '[0-9]+$' | head -1)
[ -n "$MODEM" ] || { echo "no modem" >&2; exit 1; }

SINK=$(pactl list short sinks | grep -v auto_null | head -1 | cut -f2)
pactl set-sink-volume "$SINK" 100% >/dev/null 2>&1
pactl set-sink-mute "$SINK" 0 >/dev/null 2>&1

OUT=$(sudo mmcli -m "$MODEM" --voice-create-call="number=$NUM" 2>&1) ||
	{ echo "$OUT" >&2; exit 1; }
CALL=$(echo "$OUT" | grep -oE '/Call/[0-9]+' | grep -oE '[0-9]+$')
sudo mmcli -m "$MODEM" --call "$CALL" --start >/dev/null 2>&1
echo "calling $NUM"

waited=0
while [ "$waited" -lt "$WAIT" ]; do
	ST=$(sudo mmcli -m "$MODEM" --call "$CALL" 2>/dev/null |
	     sed -n 's/.*state: *//p' | head -1)
	[ "$ST" = active ] && break
	case "$ST" in
	terminated|failed) echo "not answered ($ST)"; exit 1 ;;
	esac
	sleep 2; waited=$((waited + 2))
done
[ "$ST" = active ] || { echo "no answer after ${WAIT}s"; exit 1; }

echo "answered - speaking"
i=0
while [ "$i" -lt "$REPEAT" ]; do
	paplay --device="$SINK" "$WORK/left.wav" >/dev/null 2>&1
	i=$((i + 1))
	[ "$i" -lt "$REPEAT" ] && sleep 1
done
sleep 1                       # do not clip the end by hanging up too soon
echo "done"
