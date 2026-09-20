#!/usr/bin/env python3
"""Decode the a3xx command stream where the CP stopped, from an msm devcoredump.

Usage: ib-at-hang.py DEVCOREDUMP XMLDIR [WINDOW]

XMLDIR holds adreno_common.xml, adreno_pm4.xml and a3xx.xml from Mesa's
src/freedreno/registers/adreno. The PFP reads ahead of the ME, so the
reported stop point (CP_IBn_BASE + size - CP_IBn_BUFSZ) is where the PFP was;
the packet that stalled is at or before it.
"""
import os, re, sys
import xml.etree.ElementTree as ET

dump = open(sys.argv[1]).read()
xmldir = sys.argv[2]
WINDOW = int(sys.argv[3]) if len(sys.argv) > 3 else 40

def tag(e): return e.tag.rsplit('}', 1)[-1]

def in_a3(var):
    if not var: return True
    for part in var.split(','):
        m = re.match(r'\s*A(\d)XX(-(A(\d)XX)?)?', part)
        if m:
            lo = int(m.group(1)); hi = int(m.group(4)) if m.group(4) else (99 if m.group(2) else lo)
            if lo <= 3 <= hi: return True
    return False

roots = [ET.parse(os.path.join(xmldir, f)).getroot() for f in ('adreno_common.xml', 'adreno_pm4.xml', 'a3xx.xml')]
enums, bitsets, regs, pm4dom = {}, {}, {}, {}
ops = {}
for root in roots:
    for el in root.iter():
        t = tag(el)
        if t == 'enum':
            vals = {}
            for v in el:
                if tag(v) == 'value' and v.get('value') and in_a3(v.get('variants')):
                    vals.setdefault(int(v.get('value'), 0), v.get('name'))
            enums.setdefault(el.get('name'), {}).update(vals)
            if el.get('name') == 'adreno_pm4_type3_packets':
                ops.update(vals)
        elif t == 'bitset':
            bitsets[el.get('name')] = el
    for dom in root:
        if tag(dom) != 'domain': continue
        dn = dom.get('name')
        if dn in ('A3XX', 'AXXX'):
            for el in dom:
                t = tag(el)
                if t == 'reg32' and in_a3(el.get('variants')):
                    off = int(el.get('offset'), 0)
                    n = int(el.get('length', '1'), 0); st = int(el.get('stride', '1'), 0)
                    for i in range(n):
                        regs.setdefault(off + i * st, (el.get('name') + ('[%d]' % i if n > 1 else ''), el))
                elif t == 'array' and el.get('length') and in_a3(el.get('variants')):
                    off = int(el.get('offset'), 0); st = int(el.get('stride'), 0); n = int(el.get('length'), 0)
                    for i in range(n):
                        for r in el:
                            if tag(r) == 'reg32':
                                regs.setdefault(off + i * st + int(r.get('offset'), 0),
                                                ('%s[%d].%s' % (el.get('name'), i, r.get('name')), r))
        else:
            pm4dom.setdefault(dn, dom)

def fields(el, val):
    """Decode val with the bitfields of a reg32/bitset element."""
    bfs = [b for b in el if tag(b) == 'bitfield']
    if el.get('type') in bitsets:
        bfs += [b for b in bitsets[el.get('type')] if tag(b) == 'bitfield']
    if not bfs:
        return None
    out = []
    for b in bfs:
        if not in_a3(b.get('variants')): continue
        if b.get('pos') is not None:
            lo = hi = int(b.get('pos'))
        else:
            lo = int(b.get('low')); hi = int(b.get('high'))
        v = (val >> lo) & ((1 << (hi - lo + 1)) - 1)
        ty = b.get('type', 'uint')
        if ty == 'boolean':
            if v: out.append(b.get('name'))
        elif ty in enums:
            out.append('%s=%s' % (b.get('name'), enums[ty].get(v, v)))
        elif v:
            out.append('%s=%s' % (b.get('name'), hex(v) if ty in ('hex', 'address', 'waddress') else v))
    return ' '.join(out) or '0'

def regname(r): return regs[r][0] if r in regs else 'reg_0x%04x' % r
def regfmt(r, v):
    s = '%s = 0x%08x' % (regname(r), v)
    if r in regs:
        f = fields(regs[r][1], v)
        if f is not None: s += '  (%s)' % f
    return s
REG = {name: off for off, (name, _) in regs.items()}

def a85(txt):
    txt = re.sub(r'\s', '', txt); out = []; i = 0
    while i < len(txt):
        if txt[i] == 'z':
            out.append(0); i += 1; continue
        v = 0
        for ch in txt[i:i + 5]: v = v * 85 + ord(ch) - 33
        out.append(v & 0xffffffff); i += 5
    return out

bos = []
for m in re.finditer(r'^  - iova: (0x[0-9a-f]+)\n    size: (\d+)\n(.*?)(?=^  - |^\S)', dump, re.M | re.S):
    dm = re.search(r'data: !!ascii85 \|\n(.*)', m.group(3), re.S)
    nm = re.search(r'name: ([^\n]*)', m.group(3))
    bos.append((int(m.group(1), 16), int(m.group(2)), a85(dm.group(1)) if dm else None,
                nm.group(1).strip() if nm else ''))
rm = re.search(r'ringbuffer:\n  - id: 0\n    iova: (0x[0-9a-f]+)\n.*?    wptr: (\d+)\n.*?data: !!ascii85 \|\n(.*?)(?=^  - |^\S)', dump, re.S | re.M)
ring_iova, ring_wptr, ring = int(rm.group(1), 16), int(rm.group(2)), a85(rm.group(3))
reg = {int(o, 16) // 4: int(v, 16) for o, v in re.findall(r'\{ offset: (0x[0-9a-f]+), value: (0x[0-9a-f]+) \}', dump)}

def bo_of(addr):
    hits = [b for b in bos if b[0] <= addr < b[0] + b[1]]
    withdata = [b for b in hits if b[2] is not None]
    return (withdata or hits or [None])[0]

def read(addr, n):
    b = bo_of(addr)
    if b is None or b[2] is None: return None
    o = (addr - b[0]) // 4
    seg = b[2][o:o + n]
    return seg + [0] * (n - len(seg))

def packets(words):
    i = 0
    while i < len(words):
        h = words[i]; t = h >> 30
        if t == 3:
            n = ((h >> 16) & 0x3fff) + 1
            yield i, 3, (h >> 8) & 0x7f, words[i + 1:i + 1 + n]; i += 1 + n
        elif t == 0:
            n = ((h >> 16) & 0x3fff) + 1
            yield i, 0, (h & 0x7fff, bool(h & 0x8000)), words[i + 1:i + 1 + n]; i += 1 + n
        elif t == 2:
            yield i, 2, None, []; i += 1
        else:
            yield i, 1, h, words[i + 1:i + 3]; i += 3

IB_OPS = {k for k, v in ops.items() if v in ('CP_INDIRECT_BUFFER_PFE', 'CP_INDIRECT_BUFFER_PFD', 'CP_INDIRECT_BUFFER')}

def find_ib_ref(words, target, limit=None):
    best = None
    for i, t, op, p in packets(words):
        if limit is not None and i >= limit: break
        if t == 3 and op in IB_OPS and len(p) >= 2 and p[0] == target:
            best = (i, p[1])
    return best

def show_packet(i, t, op, p, state, mark=''):
    if t == 3:
        name = ops.get(op, 'CP_0x%02x' % op)
        line = '%s%5d  %s' % (mark, i, name)
        dom = pm4dom.get(name)
        dec = []
        for k, w in enumerate(p):
            el = None
            if dom is not None:
                for r in dom:
                    if tag(r) == 'reg32' and int(r.get('offset'), 0) == k and in_a3(r.get('variants')):
                        el = r; break
            f = fields(el, w) if el is not None else None
            dec.append('0x%x' % w + (' [%s]' % f if f else ''))
        print(line + '  ' + ', '.join(dec[:8]) + (' …(%d words)' % len(p) if len(p) > 8 else ''))
    elif t == 0:
        r, one = op
        for k, v in enumerate(p):
            rr = r if one else r + k
            state[rr] = v
        if len(p) <= 6:
            print('%s%5d  write %s' % (mark, i, '; '.join(regfmt(r if one else r + k, v) for k, v in enumerate(p))))
        else:
            print('%s%5d  write %d registers from %s' % (mark, i, len(p), regname(r)))
    elif t == 2:
        print('%s%5d  type2 filler' % (mark, i))
    else:
        print('%s%5d  type1 0x%08x' % (mark, i, op))

def check_draw(p, state):
    """Report vertex fetch state for a CP_DRAW_INDX and whether reads stay inside BOs."""
    c0 = state.get(REG.get('VFD_CONTROL_0'), 0)
    nfetch = (c0 >> 27) & 0x1f
    imin, imax = state.get(REG.get('VFD_INDEX_MIN')), state.get(REG.get('VFD_INDEX_MAX'))
    ninst = (p[1] >> 24) & 0xff if len(p) > 1 else 0
    print('         draw: %s instances, VFD_CONTROL_0 0x%08x (%d fetch), index min/max %s/%s, VFD_INDEX_OFFSET %s' % (
        ninst, c0, nfetch, imin, imax, state.get(REG.get('VFD_INDEX_OFFSET'))))
    for f in range(nfetch):
        i0 = state.get(REG.get('VFD_FETCH[%d].INSTR_0' % f), 0)
        addr = state.get(REG.get('VFD_FETCH[%d].INSTR_1' % f), 0)
        stride = (i0 >> 7) & 0xff; inst = bool(i0 & 0x10000); step = (i0 >> 24) & 0xff
        if inst:
            need = max(1, -(-max(ninst, 1) // max(step, 1))) * stride
        else:
            need = ((imax or 0) + 1) * stride
        b = bo_of(addr)
        where = 'no BO' if b is None else 'BO 0x%x+0x%x size %d%s' % (b[0], addr - b[0], b[1],
                 '' if addr + need <= b[0] + b[1] else '  READ PAST END by %d bytes' % (addr + need - b[0] - b[1]))
        print('           fetch %d: 0x%08x stride %d %s need %d bytes, %s' % (
            f, addr, stride, 'instanced step %d' % step if inst else 'per-vertex', need, where))
    if len(p) >= 5 and ((p[1] >> 6) & 3) == 0:
        b = bo_of(p[3])
        print('           index buffer 0x%08x size %d: %s' % (p[3], p[4], 'no BO' if b is None else
              'BO 0x%x size %d%s' % (b[0], b[1], '' if p[3] + p[4] <= b[0] + b[1] else ' PAST END')))

ib1, ib1sz = reg[0x458], reg[0x459]
ib2, ib2sz = reg[0x45a], reg[0x45b]
print('%d BOs (%d with contents); ring wptr %d' % (len(bos), sum(b[2] is not None for b in bos), ring_wptr))
for b in bos:
    if b[2] is not None and b[3]:
        pass
r1 = find_ib_ref(ring, ib1, ring_wptr)
if not r1:
    sys.exit('IB1 0x%x not referenced in the ring' % ib1)
L1 = r1[1]
w1 = read(ib1, L1)
print('IB1 0x%08x: %d words, called at ring word %d; PFP has %d words left -> stop at word %d; contents %s' % (
    ib1, L1, r1[0], ib1sz, L1 - ib1sz, 'present' if w1 else 'MISSING'))
if not w1: sys.exit()
stop1 = L1 - ib1sz
r2 = find_ib_ref(w1, ib2, stop1 + 1)
state = {}
if r2:
    L2 = r2[1]
    for i, t, op, p in packets(w1):
        if i >= r2[0]: break
        if t == 0:
            r, one = op
            for k, v in enumerate(p): state[r if one else r + k] = v
    w2 = read(ib2, L2)
    stop2 = L2 - ib2sz
    print('IB2 0x%08x: %d words, called at IB1 word %d; PFP has %d words left -> stop at word %d; contents %s' % (
        ib2, L2, r2[0], ib2sz, stop2, 'present' if w2 else 'MISSING'))
    words, stop, label = w2, stop2, 'IB2'
else:
    print('IB2 0x%08x is not called from IB1 before the stop point; showing IB1' % ib2)
    words, stop, label = w1, stop1, 'IB1'
if not words: sys.exit()
pk = list(packets(words))
k = max([n for n, (i, *_ ) in enumerate(pk) if i < max(stop, 1)] or [0])
print('\n=== %s packets %d..%d (">>" = packet holding the PFP stop word %d) ===' % (label, max(0, k - WINDOW), k + 8, stop))
for n, (i, t, op, p) in enumerate(pk):
    if n < max(0, k - WINDOW):
        if t == 0:
            r, one = op
            for kk, v in enumerate(p): state[r if one else r + kk] = v
        continue
    if n > k + 8: break
    show_packet(i, t, op, p, state, '>>' if n == k else '  ')
    if t == 3 and ops.get(op, '').startswith('CP_DRAW_INDX'):
        check_draw(p, state)
print('\ndraw packets in %s: %d; last one before stop at word %s' % (label,
      sum(1 for i, t, op, p in pk if t == 3 and ops.get(op, '').startswith('CP_DRAW')),
      max([i for i, t, op, p in pk if t == 3 and ops.get(op, '').startswith('CP_DRAW') and i < stop] or [None])))
print('trailing packets after stop: %d' % sum(1 for i, *_ in pk if i >= stop))


# Every draw in the IB that holds the stop point, with the vertex fetch state it used.
print('\n=== all draws in %s ===' % label)
print(' word  prim            count  inst-byte  fetches (instanced)  index-max  note')
st = {}
drawlist = []
if label == 'IB2':
    for i, t, op, p in packets(w1):
        if i >= r2[0]: break
        if t == 0:
            r, one = op
            for kk, v in enumerate(p): st[r if one else r + kk] = v
for i, t, op, p in pk:
    if t == 0:
        r, one = op
        for kk, v in enumerate(p): st[r if one else r + kk] = v
    elif t == 3 and ops.get(op, '').startswith('CP_DRAW') and len(p) >= 3:
        c0 = st.get(REG['VFD_CONTROL_0'], 0); nf = (c0 >> 27) & 0x1f
        ni = sum(1 for f in range(nf) if st.get(REG['VFD_FETCH[%d].INSTR_0' % f], 0) & 0x10000)
        prim = enums['pc_di_primtype'].get(p[1] & 0x3f, p[1] & 0x3f)
        vis = (p[1] >> 8) & 3
        note = []
        if ni and not (p[1] >> 24): note.append('instanced attrs, instance byte 0')
        if vis == 0: note.append('binning (ignore vis)')
        if i >= stop: note.append('after PFP stop')
        print('%5d  %-14s %6d  %9d  %7d (%d)  %9s  %s' % (i, prim, p[2], p[1] >> 24, nf, ni,
              st.get(REG['VFD_INDEX_MAX']), ', '.join(note)))
        f0 = st.get(REG['VFD_FETCH[0].INSTR_1'], 0); s0 = (st.get(REG['VFD_FETCH[0].INSTR_0'], 0) >> 7) & 0xff
        drawlist.append((i, f0, s0, ni, p[1] >> 24))

# GSK writes each draw's per-instance records one after another into one vertex
# buffer, so the gap to the next draw's first fetch address gives the real
# instance count. Mesa sends (instance_count - 1) & 0xff in CP_DRAW_INDX.
print('\n=== instance count implied by the vertex buffer layout ===')
print(' word  fetch0-addr  stride  gap-to-next  implied  sent(byte+1)  lost')
for n, (i, a, st_, ni, byte) in enumerate(drawlist):
    if not ni or not st_: continue
    nxt = next((d for d in drawlist[n + 1:] if d[3] and d[1] > a and bo_of(d[1]) == bo_of(a)), None)
    if nxt is None:
        b = bo_of(a)
        if b and b[2] is not None:
            used = len(b[2]) * 4 - (a - b[0])
            print('%5d  0x%08x  %6d  (last; data runs %d bytes past the address = at most %d records)  sent %d' % (i, a, st_, used, used // st_, byte + 1))
        continue
    gap = nxt[1] - a
    implied = gap / st_
    sent = byte + 1
    print('%5d  0x%08x  %6d  %11d  %7s  %12d  %s' % (i, a, st_, gap, ('%d' % implied) if gap % st_ == 0 else '%.2f' % implied,
          sent, ('YES, %d' % (implied - sent)) if gap % st_ == 0 and implied > sent else ''))
