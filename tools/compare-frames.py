#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Compare two screenshots of the same frame and say where they differ.

usage: compare-frames.py A.ppm B.ppm [DIFF.png] [--tolerance N]

Takes the plain PPM files that "grim -t ppm" writes, so nothing has to decode
PNG on the phone. Prints one JSON line: how many pixels differ, by how much,
and the rectangle holding the differences. With a third argument it also
writes a picture of the difference: the first image in grey, differing pixels
in red.

A tolerance of a few counts as "the same": the two renderers round colours
differently in gradients and text, and those single step differences are not
the glitches being looked for.
"""
import json
import struct
import sys
import zlib


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"P6"):
        raise SystemExit("%s is not a binary PPM; use grim -t ppm" % path)
    fields, pos = [], 2
    while len(fields) < 3:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while data[pos:pos + 1] not in (b"\n", b""):
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    width, height, maxval = fields
    if maxval != 255:
        raise SystemExit("%s: only 8 bit PPM is supported" % path)
    return width, height, data[pos + 1:pos + 1 + width * height * 3]


def write_png(path, width, height, rows):
    def chunk(kind, payload):
        return (struct.pack(">I", len(payload)) + kind + payload
                + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    body = zlib.compress(b"".join(b"\x00" + bytes(r) for r in rows), 6)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                + chunk(b"IDAT", body) + chunk(b"IEND", b""))


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tolerance = 4
    for a in sys.argv[1:]:
        if a.startswith("--tolerance"):
            tolerance = int(a.split("=", 1)[1]) if "=" in a else 4
    if len(args) < 2:
        raise SystemExit(__doc__)
    a_path, b_path = args[0], args[1]
    diff_path = args[2] if len(args) > 2 else None

    aw, ah, a = read_ppm(a_path)
    bw, bh, b = read_ppm(b_path)
    if (aw, ah) != (bw, bh):
        raise SystemExit("different sizes: %dx%d and %dx%d" % (aw, ah, bw, bh))

    differing = 0
    worst = 0
    minx, miny, maxx, maxy = aw, ah, -1, -1
    rows = []
    for y in range(ah):
        base = y * aw * 3
        row = bytearray(aw * 3) if diff_path else None
        for x in range(aw):
            i = base + x * 3
            d = max(abs(a[i] - b[i]), abs(a[i + 1] - b[i + 1]), abs(a[i + 2] - b[i + 2]))
            if d > worst:
                worst = d
            if d > tolerance:
                differing += 1
                if x < minx: minx = x
                if x > maxx: maxx = x
                if y < miny: miny = y
                if y > maxy: maxy = y
                if row is not None:
                    row[x * 3] = 255
            elif row is not None:
                grey = (a[i] + a[i + 1] + a[i + 2]) // 6 + 40
                row[x * 3] = row[x * 3 + 1] = row[x * 3 + 2] = grey
        if row is not None:
            rows.append(row)

    total = aw * ah
    result = {
        "size": [aw, ah],
        "tolerance": tolerance,
        "differing_pixels": differing,
        "differing_percent": round(100.0 * differing / total, 3),
        "largest_channel_difference": worst,
        "area": None if maxx < 0 else [minx, miny, maxx - minx + 1, maxy - miny + 1],
        "a": a_path, "b": b_path,
    }
    if diff_path and differing:
        write_png(diff_path, aw, ah, rows)
        result["diff_image"] = diff_path
    print(json.dumps(result))


if __name__ == "__main__":
    main()
