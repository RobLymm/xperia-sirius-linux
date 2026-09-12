#!/bin/sh
# usage: phone-build-img.sh <dtb> <cmdline-suffix> <out.img>
set -u
DTB="$1"; SUFFIX="$2"; OUT="$3"
cd /tmp || exit 1
rm -rf dispb && mkdir dispb && cd dispb
cp /boot/boot.img orig.img
unpackbootimg -i orig.img -o . >/dev/null 2>&1
V=/boot/vmlinuz
LEO=/boot/qcom-msm8974pro-sony-xperia-shinano-leo.dtb
ORIG_CMD="$(cat orig.img-cmdline)"
NEW_CMD="$ORIG_CMD $SUFFIX"
pack() {
	mkbootimg-osm0sis --kernel "$1" --ramdisk orig.img-ramdisk --cmdline "$2" \
		--base 0x00000000 --pagesize 2048 --kernel_offset 0x00008000 \
		--ramdisk_offset 0x02000000 --second_offset 0x00f00000 \
		--tags_offset 0x01e00000 --hashtype sha1 --header_version 0 -o "$3"
}
cat "$V" "$LEO" > z-stock; pack z-stock "$ORIG_CMD" boot-stock.img
cmp orig.img boot-stock.img >/dev/null && echo "REPACK_REPRODUCES_BOOT_IMG" || echo "REPACK_MISMATCH"
cat "$V" "$DTB" > z-new; pack z-new "$NEW_CMD" "$OUT"
rm -rf chk && mkdir chk && unpackbootimg -i "$OUT" -o chk >/dev/null 2>&1
B=$(basename "$OUT")
cmp chk/$B-ramdisk orig.img-ramdisk >/dev/null && echo RAMDISK_OK
[ "$(cat chk/$B-cmdline)" = "$NEW_CMD" ] && echo CMDLINE_OK || { echo "CMDLINE_MISMATCH:"; cat chk/$B-cmdline; }
tail -c "$(stat -c %s "$DTB")" chk/$B-kernel | cmp - "$DTB" >/dev/null && echo DTB_OK
head -c "$(stat -c %s "$V")" chk/$B-kernel | cmp - "$V" >/dev/null && echo VMLINUZ_OK
echo "cmdline: $(cat chk/$B-cmdline)"
ls -la "$OUT"; sha256sum "$OUT"
cp "$OUT" $HOME/
