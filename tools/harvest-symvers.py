#!/usr/bin/env python3
"""Build a Module.symvers for the running kernel.

The kernel source tree here was never fully built, so it has no
Module.symvers, and modpost then rejects every vmlinux symbol an
out-of-tree module uses. The CRCs that matter are the *running* kernel's,
and every module already installed carries them: `modprobe
--dump-modversions` prints the CRC each one recorded for every symbol it
imports. The union over all installed modules covers the vmlinux exports
in practical use.

Symbols exported by a module rather than by vmlinux are dropped, so that
modpost cannot attribute them to vmlinux and lose a dependency.
"""
import os
import re
import subprocess
import sys

MODDIR = "/lib/modules/6.16.12"
OUT = sys.argv[1] if len(sys.argv) > 1 else "Module.symvers"

kos = []
for root, _, files in os.walk(MODDIR):
    for f in files:
        if ".ko" in f:
            kos.append(os.path.join(root, f))

crcs = {}            # symbol -> crc, as imported by some installed module
mod_exports = set()  # symbols exported by a module rather than by vmlinux

for ko in kos:
    try:
        out = subprocess.run(["modprobe", "--dump-modversions", ko],
                             capture_output=True, text=True, timeout=30).stdout
    except Exception:
        out = ""
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0].startswith("0x"):
            crcs.setdefault(parts[1], parts[0])
    # A module's own exports are the strings in __ksymtab_strings. They are
    # not global ELF symbols, so nm does not show them.
    try:
        rd = subprocess.run(["readelf", "-p", "__ksymtab_strings", ko],
                            capture_output=True, text=True, timeout=30).stdout
    except Exception:
        rd = ""
    for line in rd.splitlines():
        m = re.match(r"\s*\[\s*\w+\]\s+(\S+)\s*$", line)
        if m:
            mod_exports.add(m.group(1))

vm = {s: c for s, c in crcs.items() if s not in mod_exports}
with open(OUT, "w") as fh:
    for s in sorted(vm):
        fh.write("%s\t%s\tvmlinux\tEXPORT_SYMBOL_GPL\t\n" % (vm[s], s))

print("modules scanned              : %d" % len(kos))
print("symbols harvested            : %d" % len(crcs))
print("exported by modules (dropped): %d" % len(mod_exports & set(crcs)))
print("written to %s : %d vmlinux symbols" % (OUT, len(vm)))
