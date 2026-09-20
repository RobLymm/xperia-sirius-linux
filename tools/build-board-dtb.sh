#!/bin/sh
# Build the board device tree the phone boots, from a patched kernel tree.
#
#   tools/build-board-dtb.sh <kernel-tree> <out-name> [board.dts]
#
# The third argument is which device tree to build. It defaults to the base
# board file, which is NOT what the phone runs: the flashed tree comes from
# the codec variant, which #includes fmaudio, which #includes the board file.
# Building the default and comparing against the phone shows 34 differences
# that are only the audio nodes the variant adds. Pass the variant, or
# sirius-as-flashed.dts, which composes everything the phone has.
#
# <kernel-tree> is a v6.16.12-msm8974 source tree with the aport's DT patches
# applied. That is 0001, 0009, 0012, **0014 and 0016** -- the last two were
# missing from this note and both change qcom-msm8974.dtsi. For a tree that
# matches the phone, also apply
# publish/drivers/camera/0002-ARM-dts-qcom-msm8974-add-the-camss-node.patch.
# A prepared tree lives at ~/dtbuild/linux-6.16.12-msm8974. Only arch/arm/boot/dts, include/dt-bindings,
# include/uapi/linux/input-event-codes.h and scripts/dtc/include-prefixes are
# needed; include/dt-bindings/input/linux-event-codes.h is a symlink to that
# uapi header, and a partial extract that misses it fails in cpp.
#
# Preprocessing runs here (cpp); dtc runs on the phone, which has it. The DTB
# comes back to devicetree/<out-name>.dtb. Pack and flash it with
# tools/phone-build-img.sh as usual.
#
# Environment:
#   PHONE       hostname or address of the phone over SSH   (required)
#   PHONE_USER  user to log in as                           (default: user)
set -u
TREE="${1:?usage: PHONE=<addr> $0 <kernel-tree> <out-name> [board.dts]}"
NAME="${2:?usage: PHONE=<addr> $0 <kernel-tree> <out-name> [board.dts]}"
: "${PHONE:?set PHONE to your phone's hostname or address}"
TARGET="${PHONE_USER:-user}@$PHONE"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BOARD="${3:-qcom-msm8974pro-sony-xperia-shinano-sirius.dts}"

[ -f "$TREE/include/uapi/linux/input-event-codes.h" ] || {
	echo "missing $TREE/include/uapi/linux/input-event-codes.h"; exit 1; }

# The variants #include one another, so copy them all, plus the camera
# sensor fragment that sirius-as-flashed.dts pulls in.
for f in "$ROOT"/devicetree/qcom-msm8974pro-sony-xperia-*.dts \
         "$ROOT"/devicetree/sirius-as-flashed.dts; do
	[ -f "$f" ] && cp "$f" "$TREE/arch/arm/boot/dts/qcom/"
done
[ -f "$ROOT/publish/drivers/camera/sensor-nodes.dtsi" ] &&
	cp "$ROOT/publish/drivers/camera/sensor-nodes.dtsi" "$TREE/arch/arm/boot/dts/qcom/"
[ -f "$TREE/arch/arm/boot/dts/qcom/$BOARD" ] || { echo "no such board file: $BOARD"; exit 1; }
( cd "$TREE" && cpp -nostdinc -I include -I arch/arm/boot/dts/qcom \
	-I scripts/dtc/include-prefixes -undef -x assembler-with-cpp \
	"arch/arm/boot/dts/qcom/$BOARD" -o "/tmp/$NAME.pre" ) || { echo "cpp failed"; exit 1; }

scp -q -o BatchMode=yes "/tmp/$NAME.pre" "$TARGET:/tmp/$NAME.pre" || { echo "scp failed"; exit 1; }
ssh -o BatchMode=yes "$TARGET" "dtc -I dts -O dtb -o /tmp/$NAME.dtb /tmp/$NAME.pre 2>/tmp/$NAME.dtc.log; \
	rc=\$?; grep -i ' error' /tmp/$NAME.dtc.log; exit \$rc" || { echo "dtc failed"; exit 1; }
scp -q -o BatchMode=yes "$TARGET:/tmp/$NAME.dtb" "$ROOT/devicetree/$NAME.dtb" || { echo "fetch failed"; exit 1; }

echo "devicetree/$NAME.dtb  $(stat -c %s "$ROOT/devicetree/$NAME.dtb") bytes"
echo "on the phone as /tmp/$NAME.dtb; pack with:"
echo "  ssh \$TARGET 'sudo phone-build-img.sh /tmp/$NAME.dtb \"<cmdline suffix>\" /tmp/boot-$NAME.img'"
echo "compare with the previous build:  tools/dt-equiv.py <previous>.dtb devicetree/$NAME.dtb"
