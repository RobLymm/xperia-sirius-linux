#!/bin/sh
# Compile a device tree on the phone, refresh devicetree/latest.{dts,dtb},
# and build a matching boot image.
#
#   PHONE=192.0.2.10 tools/update-latest.sh devicetree/sirius-working.dts [image-name]
#
# The phone compiles its own device tree because it already has dtc and
# mkbootimg-osm0sis, which saves setting up a cross toolchain just to turn a
# .dts into a boot image.
#
# The phone boots what fastboot writes to the boot partition, not
# /boot/boot.img, so the image this produces must be flashed to take effect.
#
# Environment:
#   PHONE       hostname or address of the phone over SSH   (required)
#   PHONE_USER  user to log in as                           (default: user)
#   CMDLINE     kernel command line baked into the image
set -e

SRC="$1"
[ -f "$SRC" ] || { echo "usage: PHONE=<addr> $0 <something.dts> [image-name]"; exit 1; }

: "${PHONE:?set PHONE to your phone's hostname or address}"
PHONE_USER="${PHONE_USER:-user}"
TARGET="$PHONE_USER@$PHONE"

BASE=$(basename "$SRC" .dts)
IMG="${2:-boot-$BASE.img}"
CMDLINE="${CMDLINE:-cma=768M msm.vram=512m msm.allow_vram_carveout=1}"
ROOT=$(cd "$(dirname "$0")/.." && pwd)

echo "==> copying $SRC to $TARGET"
scp -q -o BatchMode=yes "$SRC" "$TARGET:$BASE.dts"

# Single-quoted so $HOME and $USER expand on the phone, not here.
echo "==> compiling and packing (cmdline: $CMDLINE)"
ssh -o BatchMode=yes "$TARGET" "
  set -e
  cd \$HOME
  dtc -I dts -O dtb -o '$BASE.dtb' '$BASE.dts' 2>&1 | grep -iE ' error' && exit 1
  sudo \$HOME/phone-build-img.sh \$HOME/'$BASE.dtb' '$CMDLINE' /tmp/dispb/'$IMG' \
    | grep -E 'REPACK|_OK|MISMATCH'
  sudo cp /tmp/dispb/'$IMG' \$HOME/ && sudo chown \$USER \$HOME/'$IMG'
"

echo "==> fetching results"
mkdir -p "$ROOT/images"
scp -q -o BatchMode=yes "$TARGET:$BASE.dtb" "$ROOT/devicetree/$BASE.dtb"
scp -q -o BatchMode=yes "$TARGET:$IMG" "$ROOT/images/$IMG"

echo "==> updating the latest.* convention"
cd "$ROOT/devicetree"
ln -sf "$(basename "$SRC")" latest.dts
cp "$BASE.dtb" latest.dtb

echo
echo "latest.dts -> $(readlink latest.dts)"
echo "latest.dtb    $(stat -c %s latest.dtb) bytes"
echo "image         images/$IMG"
echo
echo "flash with:  fastboot flash boot images/$IMG"
