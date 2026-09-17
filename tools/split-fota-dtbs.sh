#!/bin/sh
# Carve Sony's own device trees out of a FOTAKernel partition image.
#
#   sh split-fota-dtbs.sh FOTAKernel.img [output-directory]
#
# Sony's recovery kernel has a device tree appended for every board variant
# the same build supports. They are the best reference there is for anything
# this port has not finished: panel timings and init sequences, the whole
# downstream audio tree, charging limits, camera wiring. Each one starts with
# the four bytes d0 0d fe ed and declares its own length.
#
# Needs dtc (apk add dtc) to turn them into readable text. Without dtc you
# still get the .dtb files.
set -u
IMG="${1:?usage: split-fota-dtbs.sh FOTAKernel.img [output-directory]}"
OUT="${2:-./fota-dtbs}"
[ -f "$IMG" ] || { echo "no such file: $IMG" >&2; exit 1; }
mkdir -p "$OUT"

python3 - "$IMG" "$OUT" <<'PY'
import struct, sys, os

img, out = sys.argv[1], sys.argv[2]
data = open(img, "rb").read()
magic = b"\xd0\x0d\xfe\xed"

found = 0
pos = 0
while True:
    pos = data.find(magic, pos)
    if pos < 0:
        break
    # totalsize is the second big-endian word of the header
    (size,) = struct.unpack_from(">I", data, pos + 4)
    if not (0x100 <= size <= 0x200000) or pos + size > len(data):
        pos += 4
        continue
    path = os.path.join(out, "fota-%02d.dtb" % found)
    with open(path, "wb") as f:
        f.write(data[pos:pos + size])
    print("%s  %d bytes" % (path, size))
    found += 1
    pos += size

print("%d device trees written to %s" % (found, out))
PY

if command -v dtc >/dev/null 2>&1; then
	echo
	echo "Decompiling, and showing which board each one is for:"
	for f in "$OUT"/*.dtb; do
		dtc -I dtb -O dts -o "${f%.dtb}.dts" "$f" 2>/dev/null || continue
		model=$(sed -n 's/^[[:space:]]*model = "\(.*\)";/\1/p' "${f%.dtb}.dts" | head -1)
		board=$(sed -n 's/.*qcom,board-id = <\(.*\)>;/\1/p' "${f%.dtb}.dts" | head -1)
		printf '  %-22s %-24s board-id %s\n' "$(basename "${f%.dtb}.dts")" "${model:-?}" "${board:-?}"
	done
	echo
	echo "The Xperia Z2 is the one whose model is \"SoMC Sirius ROW\"."
else
	echo
	echo "Install dtc (apk add dtc) to decompile these into readable text."
fi
