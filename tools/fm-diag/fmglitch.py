#!/usr/bin/env python3
"""Characterise the periodic glitch: what happens every ~1152 frames?"""
import sys
import numpy as np

FS = 48000
path = sys.argv[1]
P = int(sys.argv[2]) if len(sys.argv) > 2 else 1152
raw = np.fromfile(path, dtype="<i2")
raw = raw[: len(raw) // 2 * 2]
L = raw[0::2].astype(np.int64)
R = raw[1::2].astype(np.int64)
N = len(L)
M = (L + R) / 2

# 1. identical consecutive frames: spacing
rep = np.where((L[1:] == L[:-1]) & (R[1:] == R[:-1]))[0] + 1
gaps = np.diff(rep)
v, c = np.unique(gaps, return_counts=True)
o = np.argsort(-c)[:8]
print(f"identical frames: {len(rep)}; top gaps:", ", ".join(f"{v[i]}x{c[i]}" for i in o))
print(f"  gaps that are multiples of {P} (+-1): {np.sum((gaps % P <= 1) | (gaps % P >= P - 1))} of {len(gaps)}")
# level at repeats: are they in quiet passages?
lv = np.abs(M[rep]).mean() if len(rep) else 0
print(f"  mean |mid| at repeats {lv:.0f} vs overall {np.abs(M).mean():.0f}")

# 2. per-block phase of the strongest second difference
d2 = np.abs(M[2:] - 2 * M[1:-1] + M[:-2])
nb = (len(d2)) // P
ph = np.argmax(d2[: nb * P].reshape(nb, P), axis=1)
h, _ = np.histogram(ph, bins=48, range=(0, P))
print(f"phase of max |d2| in each {P}-block: histogram (48 bins):", h.tolist())
# track the phase drift: median phase per 1-second span
spans = []
for s in range(0, nb, FS // P):
    seg = ph[s: s + FS // P]
    spans.append(int(np.median(seg)))
print("  median phase per second:", spans[:40])

# 3. fold L and R separately at P using the dominant phase, print the folded glitch shape
# (use d2 sign-preserving to see drop vs insert shape)
dL = L[1:] - L[:-1]
dR = R[1:] - R[:-1]
# align: find global phase where mean |dL| over blocks is largest
fold = np.abs(dL[: nb * P]).reshape(nb, P).mean(axis=0)
p0 = int(np.argmax(fold))
print(f"fold |dL| at P={P}: peak {fold.max():.0f} at phase {p0}, median {np.median(fold):.0f}, "
      f"neighbours {[int(fold[(p0 + k) % P]) for k in range(-3, 4)]}")
foldR = np.abs(dR[: nb * P]).reshape(nb, P).mean(axis=0)
pR = int(np.argmax(foldR))
print(f"fold |dR| at P={P}: peak {foldR.max():.0f} at phase {pR}, median {np.median(foldR):.0f}, "
      f"neighbours {[int(foldR[(pR + k) % P]) for k in range(-3, 4)]}")

# 4. show raw frames around a few glitch instants (blocks 10, 200, 800), 6 frames each side
print("frames around glitch (L,R) pairs:")
for b in (10, 200, 800, 1400):
    if b >= nb:
        continue
    c = b * P + p0 + 1
    print(f"  block {b} frame {c}:", [(int(L[i]), int(R[i])) for i in range(c - 5, c + 6)])

# 5. drop-vs-insert test: predicted next sample by linear extrapolation vs actual, at glitch phase
#    a dropped frame: actual jumps ahead (error correlates with slope); an inserted/repeated
#    frame: actual lags. Measure mean of (L[c+1]-L[c]) * sign(L[c]-L[c-1]) at phase vs elsewhere.
def step_stat(c):
    x = M
    idx = np.arange(0, nb) * P + c
    idx = idx[(idx > 2) & (idx < N - 2)]
    slope = x[idx] - x[idx - 1]
    step = x[idx + 1] - x[idx]
    return np.mean(step * np.sign(slope)), np.mean(np.abs(step))
for k in range(-2, 4):
    s, a = step_stat(p0 + k)
    print(f"  phase {p0 + k}: mean signed step {s:8.1f}   mean |step| {a:8.1f}")
s, a = step_stat(p0 + P // 2)
print(f"  reference phase {p0 + P // 2}: mean signed step {s:8.1f}   mean |step| {a:8.1f}")
