#!/bin/sh
# Prepare the on-phone kernel tree for building out-of-tree modules against
# the kernel that is currently running.
#
#   kbuild-prep.sh [kernel-tree]        default ~/kbuild/linux-6.16.12
#
# Replaces the older ~/kbuild/prep.sh, which ran four bare `make` commands.
# LLVM=1 is not optional on this device: without it olddefconfig re-detects
# GCC, silently drops CONFIG_CFI_CLANG, and every module built afterwards
# loads and then fails. The script checks that it survived rather than
# trusting it.
#
# It does NOT produce Module.symvers. That needs a full vmlinux link, so it
# has to come out of the kernel package build:
#
#     .../chroot_native/home/pmos/build/src/linux-<ver>/Module.symvers
#
# copied out before the next pmbootstrap build zaps the chroot. Do not
# reconstruct it from the installed modules with harvest-symvers.py after a
# config change: those modules carry the CRCs of the *previous* kernel, so
# the result compiles and then refuses to load.
set -e
K=${1:-$HOME/kbuild/linux-6.16.12}
cd "$K"

echo "== config from the running kernel =="
[ -f Module.symvers ] && cp Module.symvers /tmp/symvers.keep
zcat /proc/config.gz > .config

echo "== olddefconfig (LLVM=1) =="
make LLVM=1 -j3 olddefconfig >/dev/null

grep -q '^CONFIG_CFI_CLANG=y' .config || {
	echo "CONFIG_CFI_CLANG was dropped: LLVM=1 did not take. Stopping."; exit 1; }
echo "   CONFIG_CFI_CLANG survived"

echo "== modules_prepare =="
make LLVM=1 -j3 modules_prepare

# Adding a config option by hand needs syncconfig, not just olddefconfig:
# include/config/auto.conf is what the Makefiles read, and a module whose
# CONFIG_ is missing there is skipped silently, with no error.
echo "== syncconfig (so auto.conf matches .config) =="
make LLVM=1 -j3 syncconfig >/dev/null

[ -f /tmp/symvers.keep ] && cp /tmp/symvers.keep Module.symvers &&
	echo "   Module.symvers restored ($(wc -l < Module.symvers) symbols)"
echo "== PREP DONE =="
