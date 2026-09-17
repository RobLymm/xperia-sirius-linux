#!/bin/sh
# Hold the DSP voice path open for as long as a call is active.
#
# This is what q6voiced is meant to do. It opens the voice PCM but does not
# keep it open, and the moment it closes the back ends are torn down: the
# microphone and its SLIMbus port power off a second after the call connects.
set -u
CARD=0
DEV=5
HOLD=""

route() {
	amixer -q -c$CARD cset name='DEC3 MUX' ADC4
	amixer -q -c$CARD cset name='SLIM TX7 MUX' DEC3
	amixer -q -c$CARD cset name='SLIM TX7' 1
	amixer -q -c$CARD cset name='DEC3 Volume' 100
	amixer -q -c$CARD cset name='ADC4 Volume' 15
}

echo "watching for calls; holding hw:$CARD,$DEV open while one is active"
route
while true; do
	M=$(mmcli -L 2>/dev/null | grep -oE '/Modem/[0-9]+' | grep -oE '[0-9]+$' | head -1)
	ACTIVE=no
	if [ -n "$M" ]; then
		for c in $(mmcli -m "$M" --voice-list-calls 2>/dev/null |
			   grep -oE '/Call/[0-9]+' | grep -oE '[0-9]+$'); do
			ST=$(mmcli -m "$M" --call "$c" 2>/dev/null |
			     sed -n 's/.*state: *//p' | head -1)
			[ "$ST" = active ] && ACTIVE=yes
		done
	fi

	if [ "$ACTIVE" = yes ] && [ -z "$HOLD" ]; then
		route
		/home/rob/holdpcm "hw:$CARD,$DEV" 3600 >/tmp/voicehold.pcm.log 2>&1 &
		HOLD=$!
		echo "$(date +%H:%M:%S) call active: voice path held open (pid $HOLD)"
	elif [ "$ACTIVE" = no ] && [ -n "$HOLD" ]; then
		kill "$HOLD" 2>/dev/null
		wait "$HOLD" 2>/dev/null
		HOLD=""
		echo "$(date +%H:%M:%S) call ended: voice path released"
	fi
	sleep 1
done
