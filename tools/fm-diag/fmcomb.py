#!/usr/bin/env python3
"""Find periodic artefacts in an S16_LE stereo 48 kHz FM capture.

1. Comb spacing in the 16-24 kHz band (FM broadcast audio ends at 15 kHz, so
   regular lines up there come from the digital path, not the programme).
2. Synchronous averaging: fold the second-difference envelope at every
   integer period 24..9600 frames and score how much a repeating click stands
   out; a dropped/repeated/zeroed frame every N frames scores at N.
3. Time-domain hunt for the largest events and what they look like.
"""
import sys
import numpy as np

FS = 48000
path = sys.argv[1]
raw = np.fromfile(path, dtype="<i2")
raw = raw[: len(raw) // 2 * 2]
L = raw[0::2].astype(np.float64)
R = raw[1::2].astype(np.float64)
N = len(L)
M = (L + R) / 2
print(f"{path}: {N} frames = {N / FS:.2f} s")

# ---- 1. HF comb spacing ----
seg = M - M.mean()
n = len(seg)
nfft = 1 << (n - 1).bit_length()
w = np.hanning(n)
P = np.abs(np.fft.rfft(seg * w, nfft)) ** 2
f = np.fft.rfftfreq(nfft, 1 / FS)
hf = (f >= 16000) & (f <= 23900)
Ph, fh = P[hf], f[hf]
dbh = 10 * np.log10(Ph / Ph.max() + 1e-30)
res = f[1] - f[0]
# local maxima, at least 5 dB above the surrounding 200 Hz median
peaks = []
half = int(100 / res)
for i in range(half, len(dbh) - half):
    if dbh[i] > dbh[i - 1] and dbh[i] >= dbh[i + 1]:
        floor = np.median(dbh[i - half:i + half])
        if dbh[i] - floor > 8:
            peaks.append((fh[i], dbh[i] - floor))
peaks.sort(key=lambda p: -p[1])
top = sorted(peaks[:60])
print(f"  HF band (16-23.9 kHz): {len(peaks)} lines >8 dB over local floor; resolution {res:.3f} Hz")
print("  strongest lines (Hz:+dB):", ", ".join(f"{a:.1f}:{b:.0f}" for a, b in top[:40]))
if len(top) > 3:
    fr = np.array([a for a, _ in top])
    d = np.diff(np.sort(fr))
    d = d[(d > 2) & (d < 3000)]
    if len(d):
        hist, edges = np.histogram(d, bins=np.arange(0, 3000, 2))
        k = np.argsort(-hist)[:5]
        print("  most common line spacings (Hz x count):", ", ".join(f"{edges[i]:.0f}-{edges[i + 1]:.0f}x{hist[i]}" for i in k if hist[i] > 1))

# whole-spectrum HF floor: is the 16-24 kHz band flat noise, comb, or empty?
tot = P.sum()
for lo, hi in ((15000, 16000), (16000, 18000), (18000, 20000), (20000, 22000), (22000, 24000)):
    m = (f >= lo) & (f < hi)
    print(f"  {lo}-{hi} Hz: {10 * np.log10(P[m].sum() / tot + 1e-30):6.1f} dB of total")

# ---- 2. synchronous averaging of the |d2| envelope ----
d2 = np.abs(M[2:] - 2 * M[1:-1] + M[:-2])
d2 = d2 - d2.mean()
best = []
for Pn in range(24, 9601):
    k = len(d2) // Pn
    if k < 4:
        break
    fold = d2[: k * Pn].reshape(k, Pn).mean(axis=0)
    # contrast: peak of fold vs its std, normalised for averaging count
    score = (fold.max() - np.median(fold)) / (fold.std() + 1e-9)
    best.append((score, Pn, int(np.argmax(fold))))
best.sort(reverse=True)
print("  folded |d2| contrast, best periods (score, period frames = Hz, phase):")
for s, Pn, ph in best[:10]:
    print(f"    {s:6.2f}  {Pn:5d} = {FS / Pn:8.3f} Hz  phase {ph}")

# also fold the raw mid waveform (a dropped/inserted frame gives a fixed-shape residue)
seg = M - M.mean()
bestw = []
for Pn in (480, 512, 960, 1024, 1170, 1920, 2048, 4096, 6000, 8192):
    k = len(seg) // Pn
    if k < 4:
        continue
    fold = seg[: k * Pn].reshape(k, Pn).mean(axis=0)
    bestw.append((np.abs(fold).max(), Pn))
print("  folded waveform peak at candidate DMA periods:", ", ".join(f"{Pn}:{v:.0f}" for v, Pn in sorted(bestw, reverse=True)))

# ---- 3. what do the biggest second-difference events look like? ----
d2s = M[2:] - 2 * M[1:-1] + M[:-2]
idx = np.argsort(-np.abs(d2s))[:5]
print("  largest |d2| events (frame: 9-sample mid context):")
for i in sorted(idx):
    c = i + 1
    ctx = M[max(0, c - 4): c + 5].astype(int)
    print(f"    {c:8d} ({c / FS:6.3f}s): {ctx.tolist()}")
