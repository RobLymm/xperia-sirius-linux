#!/bin/sh
# Build one kernel directory as an external module against the running kernel,
# filling in any vmlinux symbol versions it needs.
#
#   [KDIR=<kernel tree>] [J=<jobs>] [EXTRA_SYMVERS="<a> <b>"] \
#       kbuild-mod.sh <dir-under-kernel-tree> [CONFIG_X=m ...]
#
# Two passes. The first runs modpost with KBUILD_MODPOST_WARN=1 so every
# unresolved vmlinux symbol is reported instead of only the first ten, and
# feeds the list to fill-symvers.sh. The second pass is strict, so anything
# still unresolved is a real error rather than a gap in Module.symvers.
#
# KDIR defaults to ~/kbuild/linux-6.16.12, the tree prepared on the phone.
# EXTRA_SYMVERS carries the Module.symvers of directories built earlier, for a
# directory that uses their exports.
#
# LLVM=1 is not optional: this is a CONFIG_CFI_CLANG kernel, and without it
# olddefconfig re-detects GCC, drops CFI, and the modules will not load.
set -e
KDIR=${KDIR:-$HOME/kbuild/linux-6.16.12}
J=${J:-3}
FILL=${FILL:-$(dirname "$0")/fill-symvers.py}
D="${1:?usage: kbuild-mod.sh <dir> [CONFIG_X=m ...]}"
shift
cd "$KDIR"

log=$(mktemp) || exit 1
trap 'rm -f "$log"' EXIT

echo "== $D: pass 1, collecting unresolved symbols =="
KBUILD_MODPOST_WARN=1 make -j"$J" LLVM=1 M="$D" "$@" \
	KBUILD_EXTRA_SYMBOLS="${EXTRA_SYMVERS:-}" modules 2>&1 \
	| tee "$log" | grep -vE '^(  CC|  LD|make)' || true

syms=$(grep -oE '"[A-Za-z_][A-Za-z0-9_]*" \[' "$log" | tr -d '"[ ' | sort -u | tr '\n' ' ')

if [ -n "$syms" ]; then
	echo "== filling $(echo "$syms" | wc -w) symbol(s) =="
	# shellcheck disable=SC2086
	J="$J" python3 "$FILL" $syms
fi

echo "== $D: pass 2, strict =="
make -j"$J" LLVM=1 M="$D" "$@" KBUILD_EXTRA_SYMBOLS="${EXTRA_SYMVERS:-}" modules
