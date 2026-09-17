#!/bin/sh
# Take what this port needs off a Sony Xperia Z2's own stock Android system,
# and keep a copy of everything else that cannot be downloaded again.
#
# Run this on the phone, as root, with postmarketOS already installed. The
# stock Android system partition is left untouched: it is mounted read only.
#
#   sudo sh extract-from-stock.sh              install what is needed, back up the rest
#   sudo sh extract-from-stock.sh --backup-only  only back up, install nothing
#
# Why anything has to be extracted at all: postmarketOS packages the modem,
# audio DSP and Wi-Fi firmware, but nothing packages the Bluetooth firmware
# for this chip, and the Bluetooth chip is also where the FM radio lives. One
# file has to come off your own phone. Everything else this script copies is
# a backup, or a reference you will want later.
set -u

BACKUP="${BACKUP_DIR:-/home/$(logname 2>/dev/null || echo root)/stock-backup}"
ONLY_BACKUP=no
[ "${1:-}" = "--backup-only" ] && ONLY_BACKUP=yes

say() { printf '%s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || die "run this with sudo"

# ---------------------------------------------------------------- find things
SYS=/dev/disk/by-partlabel/system
[ -e "$SYS" ] || die "no partition labelled 'system'. If you have already
       overwritten it, the stock firmware is gone and you will need a
       firmware image for your exact model from Sony's Xperia Firmware
       pages or from XperiFirm, unpacked with Flashtool."

MNT=$(mktemp -d)
trap 'umount "$MNT" 2>/dev/null; rmdir "$MNT" 2>/dev/null' EXIT
mount -o ro "$SYS" "$MNT" || die "could not mount $SYS read only"

[ -d "$MNT/etc/firmware" ] || die "$SYS is mounted but has no etc/firmware:
       this does not look like a stock Sony system partition"

MODEL=$(sed -n 's/^ro.product.model=//p' "$MNT/build.prop" 2>/dev/null)
BUILD=$(sed -n 's/^ro.build.display.id=//p' "$MNT/build.prop" 2>/dev/null)
say "Stock system found: model ${MODEL:-unknown}, build ${BUILD:-unknown}"
case "$MODEL" in
D650*) ;;
"") say "  (no model string; carrying on)" ;;
*) say "  WARNING: D6502, D6503 or D6543 expected for an Xperia Z2." ;;
esac

# ------------------------------------------------------------------- back up
mkdir -p "$BACKUP/firmware" "$BACKUP/modem-nv" "$BACKUP/fota"
say ""
say "Backing up to $BACKUP"

cp -a "$MNT/etc/firmware/." "$BACKUP/firmware/" 2>/dev/null
say "  firmware:  $(find "$BACKUP/firmware" -type f | wc -l) files"

# The modem's calibration and settings. These are unique to your phone and
# cannot be downloaded from anywhere. Losing them costs you the modem.
for part in modemst1 modemst2 fsg TA; do
	dev=/dev/disk/by-partlabel/$part
	[ -e "$dev" ] || { say "  modem-nv:  no $part partition, skipped"; continue; }
	dd if="$dev" of="$BACKUP/modem-nv/$part.img" bs=1M 2>/dev/null
	say "  modem-nv:  $part.img ($(du -h "$BACKUP/modem-nv/$part.img" | cut -f1))"
done

# The FOTA kernel partition carries Sony's own device trees for every board
# variant. It is the best reference there is for anything not yet working:
# panel timings, audio routing, charging limits.
if [ -e /dev/disk/by-partlabel/FOTAKernel ]; then
	dd if=/dev/disk/by-partlabel/FOTAKernel of="$BACKUP/fota/FOTAKernel.img" \
		bs=1M 2>/dev/null
	say "  fota:      FOTAKernel.img ($(du -h "$BACKUP/fota/FOTAKernel.img" | cut -f1))"
	say "             carve the device trees out of it with:"
	say "               tools/split-fota-dtbs.sh $BACKUP/fota/FOTAKernel.img"
fi

if [ "$ONLY_BACKUP" = yes ]; then
	say ""
	say "Backup only, nothing installed. Copy $BACKUP somewhere off the phone."
	exit 0
fi

# ------------------------------------------------------------------- install
say ""
say "Installing the one file that is not packaged anywhere"

# Bluetooth, and with it the FM radio: the Broadcom BCM4335C0's patch RAM
# image. Sony ships it as BCM43xx.hcd; the mainline hci_uart_bcm driver asks
# for it by chip name.
SRC="$MNT/etc/firmware/BCM43xx.hcd"
DST=/lib/firmware/brcm/BCM4335C0.hcd
if [ -f "$SRC" ]; then
	mkdir -p /lib/firmware/brcm
	if [ -f "$DST" ] && cmp -s "$SRC" "$DST"; then
		say "  $DST already correct"
	else
		install -m 0644 "$SRC" "$DST"
		say "  $DST installed ($(du -h "$DST" | cut -f1))"
	fi
else
	say "  WARNING: no BCM43xx.hcd in the stock firmware. Bluetooth and the"
	say "  FM radio will not work without it."
fi

# Wi-Fi: postmarketOS packages the Xperia Z3's firmware, which this chip
# accepts, but the driver asks for it under this board's own name.
LEO=/lib/firmware/brcm/brcmfmac4339-sdio.sony,xperia-leo.bin
OURS=/lib/firmware/brcm/brcmfmac4339-sdio.sony,xperia-sirius.bin
if [ -f "$LEO" ] && [ ! -e "$OURS" ]; then
	ln -s "$(basename "$LEO")" "$OURS"
	say "  $OURS linked to the packaged Z3 firmware"
fi

say ""
say "Done. Keep a copy of $BACKUP somewhere that is not this phone:"
say "  the modem calibration in modem-nv/ is the only part of your phone"
say "  that cannot be replaced."
