#!/usr/bin/env python3
"""Check that two device tree blobs describe the same hardware.

    tools/dt-equiv.py [--rename old=new ...] before.dtb after.dtb

Use it after a source change that should not change the hardware
description: reordering nodes, replacing numbers with dt-bindings macros,
renaming labels. Such changes renumber phandles, so a byte comparison
(tools/fdtdiff.py) reports differences that are not real. This compares node
paths, property names and property values, and treats two 32-bit cells as
equal when they are the same number or when both are phandles of the same
node path.

--rename maps a node name in after.dtb back to its name in before.dtb, for a
deliberate rename (e.g. --rename touchscreen@48=maxim_max1187x_tsc@48).

Either file may be a boot image with the DTB appended to the kernel. /chosen
and /memory are ignored, because the bootloader rewrites them, so a running
/sys/firmware/fdt can be compared too.

Limitation: a value that is not a list of cells but happens to be 4n bytes
long is still compared cell by cell. Two different strings can only be
reported equal if every differing 4-byte chunk is a phandle of the same node
in both trees, which has not been seen in practice.

Before trusting a new result from a changed version of this script, change
one number and one phandle target in a copy of the source and check that it
reports both.
"""
import struct, sys

MAGIC = b'\xd0\x0d\xfe\xed'

def load(path):
    b = open(path, 'rb').read()
    if b[:4] != MAGIC:
        i = b.rfind(MAGIC)
        b = b[i:i + struct.unpack('>I', b[i + 4:i + 8])[0]]
    off_struct, off_strings = struct.unpack('>II', b[8:16])
    sz_strings = struct.unpack('>I', b[32:36])[0]
    strings = b[off_strings:off_strings + sz_strings]
    nodes, path, i = {}, [], off_struct
    while True:
        tok = struct.unpack('>I', b[i:i + 4])[0]; i += 4
        if tok == 1:
            end = b.index(b'\0', i)
            path.append(b[i:end].decode()); i = (end + 4) & ~3
            nodes.setdefault('/'.join(path) or '/', {})
        elif tok == 2:
            path.pop()
        elif tok == 3:
            ln, nameoff = struct.unpack('>II', b[i:i + 8]); i += 8
            name = strings[nameoff:strings.index(b'\0', nameoff)].decode()
            nodes['/'.join(path) or '/'][name] = b[i:i + ln]; i = (i + ln + 3) & ~3
        elif tok == 9:
            break
    return nodes

def skip(node):
    top = node.split('/')[1] if node.count('/') else ''
    return top.startswith('chosen') or top.startswith('memory')

def phandles(nodes):
    out = {}
    for n, props in nodes.items():
        for k in ('phandle', 'linux,phandle'):
            if k in props:
                out[struct.unpack('>I', props[k])[0]] = n
    return out

def main(argv):
    renames = {}
    while argv and argv[0] == '--rename':
        after, before = argv[1].split('=', 1)
        renames[after] = before
        argv = argv[2:]
    if len(argv) != 2:
        sys.exit(__doc__)
    a = load(argv[0])
    raw = load(argv[1])
    b = {'/'.join(renames.get(s, s) for s in n.split('/')): p for n, p in raw.items()}
    pa = phandles(a)
    pb = {k: '/'.join(renames.get(s, s) for s in v.split('/')) for k, v in phandles(raw).items()}

    na = {n for n in a if not skip(n)}
    nb = {n for n in b if not skip(n)}
    problems = 0
    for n in sorted(na - nb):
        print('only in %s: %s' % (argv[0], n)); problems += 1
    for n in sorted(nb - na):
        print('only in %s: %s' % (argv[1], n)); problems += 1
    for n in sorted(na & nb):
        ka = set(a[n]) - {'phandle', 'linux,phandle'}
        kb = set(b[n]) - {'phandle', 'linux,phandle'}
        for k in sorted(ka ^ kb):
            print('property only in one: %s %s' % (n, k)); problems += 1
        for k in sorted(ka & kb):
            va, vb = a[n][k], b[n][k]
            if va == vb:
                continue
            if len(va) == len(vb) and len(va) % 4 == 0:
                ca = struct.unpack('>%dI' % (len(va) // 4), va)
                cb = struct.unpack('>%dI' % (len(vb) // 4), vb)
                if all(x == y or (x in pa and y in pb and pa[x] == pb[y])
                       for x, y in zip(ca, cb)):
                    continue
            print('value differs: %s %s' % (n, k)); problems += 1
    print('%d nodes compared, %d differences' % (len(na & nb), problems))
    return 1 if problems else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
