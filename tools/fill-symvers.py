#!/usr/bin/env python3
"""Add missing vmlinux symbol versions to a kernel tree's Module.symvers.

    fill-symvers.py <symbol> [<symbol> ...]

harvest-symvers.py covers every vmlinux export that some installed module
already imports. A new driver can use one that nothing else does, and modpost
then reports it undefined. genksyms in this tree computes the same CRC the
running kernel holds, so building the object that exports the symbol and
reading the #SYMVER lines out of its .o.cmd fills the gap exactly.

The trap is that a symbol is often exported from several files, only one of
which this configuration builds. Taking the first is wrong and the result is
silent: clk_round_rate is exported by both drivers/clk/clk.c and
drivers/sh/clk/core.c, the SuperH one sorts first, and its CRC differs, so the
module loads far enough to fail with "disagrees about version of symbol".
Likewise mm/nommu.c against mm/vmalloc.c.

So a candidate is only accepted if kbuild would actually build it: walk from
its directory up to the tree root and check that each parent Makefile pulls
the child in with an obj- rule whose CONFIG is set. If two accepted candidates
disagree the symbol is reported rather than guessed at.

Namespaced exports (EXPORT_SYMBOL_NS_GPL(dma_buf_fd, "DMA_BUF")) keep their
namespace, so that modpost still checks the module declares MODULE_IMPORT_NS.

Run from the top of the kernel tree.
"""
import os
import re
import subprocess
import sys

SEARCH = ["drivers", "kernel", "lib", "mm", "fs", "net", "block", "crypto",
          "security", "sound", "arch/arm"]
JOBS = os.environ.get("J", "3")


def config():
    values = {}
    with open(".config") as fh:
        for line in fh:
            m = re.match(r"(CONFIG_\w+)=(.*)", line)
            if m:
                values[m.group(1)] = m.group(2)
    return values


def dir_is_built(path, cfg):
    """Would kbuild descend into this object's directory with this config?"""
    d = os.path.dirname(path)
    while d:
        parent = os.path.dirname(d)
        name = os.path.basename(d)
        mk = os.path.join(parent, "Makefile") if parent else "Makefile"
        if not os.path.exists(mk):
            return True                      # no Makefile to judge by
        rule = None
        with open(mk, errors="replace") as fh:
            for line in fh:
                if re.search(r"^\s*obj-\S+\s*[+:]?=.*(^|\s)%s/" % re.escape(name), line):
                    rule = line
                    break
        if rule is not None:
            m = re.search(r"obj-\$\((CONFIG_\w+)\)", rule)
            if m and cfg.get(m.group(1)) not in ("y", "m"):
                return False
        d = parent
    return True


def symvers_has(sym):
    with open("Module.symvers") as fh:
        return any(line.split("\t")[1:2] == [sym] for line in fh)


def exports_of(obj):
    cmd = os.path.join(os.path.dirname(obj), "." + os.path.basename(obj) + ".cmd")
    if not os.path.exists(cmd):
        return {}
    text = open(cmd, errors="replace").read()
    return dict((m.group(1), m.group(2)) for m in
                re.finditer(r"#SYMVER (\w+) (0x[0-9a-f]+)", text))


def main(symbols):
    if not os.path.exists("Module.symvers"):
        sys.exit("run from the kernel tree root")
    cfg = config()
    added = 0

    for sym in symbols:
        if symvers_has(sym):
            continue

        pattern = r"EXPORT_SYMBOL[A-Z_]*\(%s[,)]" % re.escape(sym)
        found = subprocess.run(["grep", "-rlE", pattern, "--include=*.c"] + SEARCH,
                               capture_output=True, text=True).stdout.split()
        if not found:
            print("NOSRC   %s" % sym)
            continue

        usable = [s for s in found if dir_is_built(s, cfg)]
        skipped = [s for s in found if s not in usable]
        if not usable:
            print("NOBUILT %s  (no candidate is built here: %s)"
                  % (sym, " ".join(found)))
            continue

        results = {}
        for src in usable:
            obj = src[:-2] + ".o"
            if not os.path.exists(os.path.join(os.path.dirname(obj),
                                               "." + os.path.basename(obj) + ".cmd")):
                subprocess.run(["make", "-j" + JOBS, "LLVM=1", obj],
                               capture_output=True)
            crcs = exports_of(obj)
            if sym in crcs:
                results[src] = crcs

        if not results:
            print("ABSENT  %s  (built %s, genksyms gave no CRC)"
                  % (sym, " ".join(usable)))
            continue

        distinct = set(c[sym] for c in results.values())
        if len(distinct) > 1:
            print("AMBIG   %s  %s -- not added, choose by hand"
                  % (sym, ", ".join("%s=%s" % (s, c[sym])
                                    for s, c in results.items())))
            continue

        src, crcs = next(iter(results.items()))
        source = open(src, errors="replace").read()
        with open("Module.symvers", "a") as fh:
            for s, c in sorted(crcs.items()):
                if symvers_has(s):
                    continue
                m = re.search(r"EXPORT_SYMBOL_NS[A-Z_]*\(%s,\s*\"([^\"]*)\""
                              % re.escape(s), source)
                fh.write("%s\t%s\tvmlinux\tEXPORT_SYMBOL_GPL\t%s\n"
                         % (c, s, m.group(1) if m else ""))
        note = "" if not skipped else "  (not built here: %s)" % " ".join(skipped)
        print("added   %s  from %s%s" % (sym, src, note))
        added += 1

    total = sum(1 for _ in open("Module.symvers"))
    print("--- %d added; Module.symvers now %d lines" % (added, total))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: fill-symvers.py <symbol> [<symbol> ...]")
    main(sys.argv[1:])
