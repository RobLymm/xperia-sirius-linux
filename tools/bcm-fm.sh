#!/bin/sh
# Drive the FM receiver inside the Xperia Z2's Broadcom BCM4335C0 Bluetooth
# chip from userspace, over HCI vendor command 0xFC15. No kernel driver needed.
#
#   bcm-fm.sh on | off
#   bcm-fm.sh tune 98.8
#   bcm-fm.sh status
#   bcm-fm.sh sweep [start MHz] [end MHz]    RSSI and SNR every 100 kHz
#
# The headphone lead is the aerial: with nothing plugged in, expect no signal.
#
# Protocol, from Sony's v4l2_fm_driver (LineageOS android_kernel_sony_msm8974,
# drivers/bluetooth/broadcom/v4l2_fm_driver):
#   command  : ogf 0x3f ocf 0x0015, payload = register, rw (0 write, 1 read), data
#   response : HCI command complete  01 15 FC <status> <register> <rw> <data...>
#   frequency: register 0x0a, 2 bytes little endian, value = kHz - 64000
HCI=${HCI:-hci0}

cmd() { sudo hcitool -i "$HCI" cmd 0x3f 0x15 "$@" | sed -n '4p'; }
rd()  { cmd "$1" 0x01 "$2" | awk '{for (i = 7; i <= NF; i++) printf "%s", $i}'; }
wr()  {
	r=$(cmd "$@")
	[ "$(echo "$r" | awk '{print $4}')" = "00" ] || { echo "write $1 failed: $r" >&2; return 1; }
}

REG_RDS_SYS=0x00 REG_FM_CTRL=0x01 REG_SCH_TUNE=0x09 REG_FREQ=0x0a
REG_RSSI=0x0f REG_SNR=0xdf

khz() { awk -v m="$1" 'BEGIN { printf "%d", m * 1000 + 0.5 }'; }

tune() {
	v=$(( $(khz "$1") - 64000 ))
	wr $REG_FREQ 0x00 "$(printf '0x%02x' $((v & 0xff)))" "$(printf '0x%02x' $((v >> 8)))"
	wr $REG_SCH_TUNE 0x00 0x01	# preset mode: tune to exactly this frequency
}

status() {
	f=$(cmd $REG_FREQ 0x01 0x02 | awk '{print $8 $7}')
	k=$(( 0x$f + 64000 ))
	printf '%s MHz  rssi 0x%s  snr 0x%s  ctrl 0x%s\n' \
		"$(awk -v k=$k 'BEGIN { printf "%.1f", k / 1000 }')" \
		"$(rd $REG_RSSI 0x01)" "$(rd $REG_SNR 0x01)" "$(rd $REG_FM_CTRL 0x01)"
}

sweep() {
	k=$(khz "${1:-87.5}"); end=$(khz "${2:-108.0}")
	while [ "$k" -le "$end" ]; do
		m=$(awk -v k=$k 'BEGIN { printf "%.1f", k / 1000 }')
		tune "$m"; sleep 0.15
		printf '%s  rssi 0x%s  snr 0x%s\n' "$m" "$(rd $REG_RSSI 0x01)" "$(rd $REG_SNR 0x01)"
		k=$((k + 100))
	done
}

case "$1" in
on)	wr $REG_RDS_SYS 0x00 0x01 && wr $REG_FM_CTRL 0x00 0x02 ;;	# stereo auto
off)	wr $REG_RDS_SYS 0x00 0x00 ;;
tune)	tune "$2" && sleep 0.3 && status ;;
status)	status ;;
sweep)	sweep "$2" "$3" ;;
*)	sed -n '2,10p' "$0"; exit 1 ;;
esac
