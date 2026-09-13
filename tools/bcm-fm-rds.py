#!/usr/bin/env python3
"""Tune the Z2's Broadcom FM tuner and decode RDS (PI code and station name).

    sudo python3 bcm-fm-rds.py 98.9 [seconds]

Uses hcitool for the HCI vendor command 0xFC15, like bcm-fm.sh. RDS arrives
from register 0x80 as 3-byte tuples: byte 0 high nibble = block (0 A, 1 B,
2 C, 3 D, 4 C'), bits 2-3 = error quality, bytes 1-2 = the 16-bit block.
Protocol from Sony's drivers/bluetooth/broadcom/v4l2_fm_driver.
"""
import subprocess, sys, time

def fm(*payload):
    args = ["hcitool", "-i", "hci0", "cmd", "0x3f", "0x15"] + ["0x%02x" % b for b in payload]
    out = subprocess.run(args, capture_output=True, text=True).stdout
    ev = out.split("> HCI Event", 1)[1].split("\n", 1)[1]
    data = [int(x, 16) for x in ev.split()]
    # 01 15 FC status reg rw data...
    if len(data) < 6 or data[3] != 0:
        raise RuntimeError("FM command failed: " + out)
    return data[6:]

def wr(reg, *vals): fm(reg, 0x00, *vals)
def rd(reg, n):     return fm(reg, 0x01, n)

mhz = float(sys.argv[1]); secs = float(sys.argv[2]) if len(sys.argv) > 2 else 20
v = int(round(mhz * 1000)) - 64000
wr(0x00, 0x03)                    # FM on + RDS on
wr(0x01, 0x02)                    # stereo auto, west band
wr(0x02, 0x02)                    # RDS_CTL0: RDS (not RBDS), flush FIFO
wr(0x14, 0x10)                    # RDS FIFO waterline, in tuples
wr(0x0a, v & 0xff, v >> 8)        # frequency
wr(0x09, 0x01)                    # tune to it
time.sleep(0.5)
rssi = rd(0x0f, 1)[0]; snr = rd(0xdf, 1)[0]
print(f"{mhz:.1f} MHz  rssi {rssi - 256 if rssi > 127 else rssi} dBm  snr {snr}")

ps = [" "] * 8; seen = [False] * 4; pi = None; blocks = {}
end = time.time() + secs; good = bad = 0
while time.time() < end:
    data = rd(0x80, 240)
    for i in range(0, len(data) - 2, 3):
        t, hi, lo = data[i], data[i + 1], data[i + 2]
        blk, q = t >> 4, (t >> 2) & 3
        if q == 3:
            bad += 1; blocks.clear(); continue
        good += 1
        word = (hi << 8) | lo
        if blk == 0:
            blocks = {0: word}; pi = word
        elif blk in (1, 2, 3, 4) and 0 in blocks:
            blocks[blk] = word
            if blk == 3 and 1 in blocks:
                b = blocks[1]; group = b >> 12; version_b = (b >> 11) & 1
                if group == 0:           # 0A/0B: programme service name
                    seg = b & 3
                    ps[seg * 2] = chr(hi) if 32 <= hi < 127 else "?"
                    ps[seg * 2 + 1] = chr(lo) if 32 <= lo < 127 else "?"
                    seen[seg] = True
                blocks = {}
    if all(seen):
        break
    time.sleep(0.4)

print(f"RDS blocks good {good}, unrecoverable {bad}")
print(f"PI  : {pi:04X}" if pi is not None else "PI  : none")
print(f"name: '{''.join(ps)}'" + ("" if all(seen) else "  (incomplete)"))
