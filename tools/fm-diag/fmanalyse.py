#!/usr/bin/env python3
"""Look for frame-level faults (dropped / repeated / zeroed / swapped frames)
and periodic glitches in an S16_LE stereo 48 kHz FM capture."""
import sys
import numpy as np

FS = 48000
path = sys.argv[1]
raw = np.fromfile(path, dtype="<i2")
raw = raw[: len(raw) // 2 * 2]
L = raw[0::2].astype(np.int64)
R = raw[1::2].astype(np.int64)
N = len(L)
print(f"{path}: {N} frames = {N / FS:.3f} s")
for name, c in (("L", L), ("R", R)):
    rms = np.sqrt(np.mean(c.astype(float) ** 2))
    print(f"  {name}: peak {np.abs(c).max():6d}  rms {20 * np.log10(max(rms, 1) / 32768):6.1f} dBFS"
          f"  mean {c.mean():8.1f}  distinct(1st s) {len(np.unique(c[:FS]))}")
M = (L + R) / 2.0
S = (L - R) / 2.0
print(f"  L/R corr {np.corrcoef(L, R)[0, 1]:.3f}   side/mid rms {np.sqrt(np.mean(S**2)) / max(np.sqrt(np.mean(M**2)), 1e-9):.3f}")

# --- exact-repeat and zero frames (DMA re-read / dropout insertion) ---
rep = np.where((L[1:] == L[:-1]) & (R[1:] == R[:-1]))[0] + 1
zero = np.where((L == 0) & (R == 0))[0]
print(f"  identical consecutive frames: {len(rep)} ({len(rep) / N * 100:.3f}%)   all-zero frames: {len(zero)}")

def spacing_report(idx, label, top=8):
    if len(idx) < 3:
        print(f"  {label}: too few events ({len(idx)})")
        return
    gaps = np.diff(idx)
    vals, counts = np.unique(gaps, return_counts=True)
    order = np.argsort(-counts)[:top]
    print(f"  {label}: {len(idx)} events, mean gap {gaps.mean():.1f} frames ({FS / max(gaps.mean(), 1):.1f} Hz)")
    print("    most common gaps:", ", ".join(f"{vals[i]}x{counts[i]}" for i in order))

# --- discontinuity detection via second difference (audio is smooth) ---
for name, c in (("L", L), ("R", R), ("M", M)):
    d2 = c[2:] - 2 * c[1:-1] + c[:-2]
    ad = np.abs(d2)
    thr = 8 * np.percentile(ad, 99)
    ev = np.where(ad > thr)[0] + 1
    print(f"  {name}: |d2| p50 {np.percentile(ad, 50):.0f} p99 {np.percentile(ad, 99):.0f} max {ad.max():.0f}; events >8*p99 ({thr:.0f}): {len(ev)}")
    spacing_report(ev, f"    {name} d2 spikes")

# --- periodic-glitch search: autocorrelation of |d2| envelope over 0..4096 frames ---
d2 = M[2:] - 2 * M[1:-1] + M[:-2]
e = np.abs(d2) - np.abs(d2).mean()
n = min(len(e), FS * 10)
e = e[:n]
F = np.fft.rfft(e, 2 * n)
ac = np.fft.irfft(F * np.conj(F))[:4097]
ac /= ac[0]
best = sorted(((ac[k], k) for k in range(16, 4097)), reverse=True)[:6]
print("  |d2| autocorr peaks (lag frames, coeff):", ", ".join(f"{k}:{v:.3f}" for v, k in best))

# --- comb search in the mid spectrum: low band peaks ---
seg = M[: FS * 8] - M[: FS * 8].mean()
w = np.hanning(len(seg))
P = np.abs(np.fft.rfft(seg * w)) ** 2
f = np.fft.rfftfreq(len(seg), 1 / FS)
band = (f >= 15) & (f <= 1500)
Pb, fb = P[band], f[band]
db = 10 * np.log10(Pb / Pb.max() + 1e-20)
# local maxima above -50 dB
pk = [i for i in range(1, len(db) - 1) if db[i] > db[i - 1] and db[i] >= db[i + 1] and db[i] > -45]
pk = sorted(pk, key=lambda i: -db[i])[:15]
print("  MID spectral peaks 15-1500 Hz (Hz: dB rel max):", ", ".join(f"{fb[i]:.1f}:{db[i]:.0f}" for i in sorted(pk, key=lambda i: fb[i])))
# side spectrum peaks
segs = S[: FS * 8] - S[: FS * 8].mean()
Ps = np.abs(np.fft.rfft(segs * w)) ** 2
Psb = Ps[band]
dbs = 10 * np.log10(Psb / Psb.max() + 1e-20)
pks = [i for i in range(1, len(dbs) - 1) if dbs[i] > dbs[i - 1] and dbs[i] >= dbs[i + 1] and dbs[i] > -45]
pks = sorted(pks, key=lambda i: -dbs[i])[:15]
print("  SIDE spectral peaks 15-1500 Hz:", ", ".join(f"{fb[i]:.1f}:{dbs[i]:.0f}" for i in sorted(pks, key=lambda i: fb[i])))

# --- band energy distribution (mid vs side) ---
def bands(P, f):
    tot = P.sum()
    out = []
    for lo, hi in ((0, 100), (100, 1000), (1000, 3000), (3000, 8000), (8000, 15000), (15000, 24000)):
        m = (f >= lo) & (f < hi)
        out.append(f"{lo}-{hi}:{P[m].sum() / tot * 100:.1f}%")
    return " ".join(out)
print("  MID band energy:", bands(P, f))
print("  SIDE band energy:", bands(Ps, f))

# --- L/R half-frame offset test: is R[n] closer to L[n] or L[n+1]? ---
c0 = np.corrcoef(L[1:-1], R[1:-1])[0, 1]
cp = np.corrcoef(L[2:], R[1:-1])[0, 1]   # R[n] vs L[n+1]
cm = np.corrcoef(L[:-2], R[1:-1])[0, 1]  # R[n] vs L[n-1]
print(f"  corr(L[n],R[n]) {c0:.3f}  corr(L[n+1],R[n]) {cp:.3f}  corr(L[n-1],R[n]) {cm:.3f}")

# --- windowed L/R correlation over time: does the pairing flip? ---
win = 4800
cs = []
for i in range(0, N - win, win):
    a, b = L[i:i + win].astype(float), R[i:i + win].astype(float)
    if a.std() > 0 and b.std() > 0:
        cs.append(np.corrcoef(a, b)[0, 1])
cs = np.array(cs)
print(f"  windowed L/R corr (100 ms): min {cs.min():.3f} median {np.median(cs):.3f} max {cs.max():.3f}; windows < 0.5: {(cs < 0.5).sum()}/{len(cs)}")
