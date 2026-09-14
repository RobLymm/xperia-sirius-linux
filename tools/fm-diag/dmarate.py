#!/usr/bin/env python3
"""Measure the LPASS capture DMA rate: timestamp the instants hw_ptr changes
(period boundaries) at the start and after N seconds."""
import sys, time
dev = sys.argv[1] if len(sys.argv) > 1 else "3"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 30
path = f"/proc/asound/card0/pcm{dev}c/sub0/status"

def hwptr():
    with open(path) as f:
        for line in f:
            if line.startswith("hw_ptr"):
                return int(line.split(":")[1])
    return None

def edge():
    p0 = hwptr()
    while True:
        p = hwptr()
        if p is None or p != p0:
            return time.monotonic(), p

deadline = time.monotonic() + 20
while hwptr() in (None, 0) and time.monotonic() < deadline:
    time.sleep(0.05)
t0, p0 = edge()
time.sleep(secs)
t1, p1 = edge()
print(f"hw_ptr {p0} -> {p1}: {p1 - p0} frames in {t1 - t0:.6f} s = {(p1 - p0) / (t1 - t0):.3f} Hz")
