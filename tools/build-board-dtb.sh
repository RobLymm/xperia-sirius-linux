#!/bin/sh
# Build the board device tree from a kernel source tree.
#
#   PHONE=192.0.2.10 tools/build-board-dtb.sh <kernel-tree> <out-name>
#
# <kernel-tree> is a v6.16.12-msm8974 source tree. The board file needs no
# patches; SoC-level device tree patches (CPU frequency scaling, for example)
# are picked up if they are applied. Only arch/arm/boot/dts, include/dt-bindings,
# include/uapi/linux/input-event-codes.h and scripts/dtc/include-prefixes are
# used. include/dt-bindings/input/linux-event-codes.h is a symlink to the uapi
# header, and a partial extract that misses it fails in cpp.
#
# Preprocessing runs here with cpp. dtc runs on the phone, which has it, so no
# cross toolchain is needed. The DTB comes back as devicetree/<out-name>.dtb
# and stays on the phone as /tmp/<out-name>.dtb for tools/phone-build-img.sh.
#
# Environment:
#   PHONE       hostname or address of the phone over SSH   (required)
#   PHONE_USER  user to log in as                           (default: user)
set -u

TREE="${1:?usage: PHONE=<addr> $0 <kernel-tree> <out-name>}"
NAME="${2:?usage: PHONE=<addr> $0 <kernel-tree> <out-name>}"
: "${PHONE:?set PHONE to your phone's hostname or address}"
TARGET="${PHONE_USER:-user}@$PHONE"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BOARD=qcom-msm8974pro-sony-xperia-shinano-sirius.dts
PRE=$(mktemp) || exit 1
trap 'rm -f "$PRE"' EXIT

[ -f "$TREE/include/uapi/linux/input-event-codes.h" ] || {
	echo "missing $TREE/include/uapi/linux/input-event-codes.h"; exit 1; }

cp "$ROOT/devicetree/$BOARD" "$TREE/arch/arm/boot/dts/qcom/" || exit 1
( cd "$TREE" && cpp -nostdinc -I include -I arch/arm/boot/dts/qcom \
	-I scripts/dtc/include-prefixes -undef -x assembler-with-cpp \
	"arch/arm/boot/dts/qcom/$BOARD" -o "$PRE" ) || { echo "cpp failed"; exit 1; }

scp -q -o BatchMode=yes "$PRE" "$TARGET:/tmp/$NAME.pre" || { echo "scp failed"; exit 1; }
ssh -o BatchMode=yes "$TARGET" "dtc -I dts -O dtb -o /tmp/$NAME.dtb /tmp/$NAME.pre 2>/tmp/$NAME.dtc.log; \
	rc=\$?; grep -i ' error' /tmp/$NAME.dtc.log; echo \"\$(grep -c -i warning /tmp/$NAME.dtc.log) dtc warnings\"; exit \$rc" \
	|| { echo "dtc failed"; exit 1; }
scp -q -o BatchMode=yes "$TARGET:/tmp/$NAME.dtb" "$ROOT/devicetree/$NAME.dtb" || { echo "fetch failed"; exit 1; }

echo "devicetree/$NAME.dtb  $(stat -c %s "$ROOT/devicetree/$NAME.dtb") bytes"
echo "compare with the previous build:  tools/dt-equiv.py <previous>.dtb devicetree/$NAME.dtb"
