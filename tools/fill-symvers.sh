#!/bin/sh
# Add missing vmlinux symbols to Module.symvers.
#
#   fill-symvers.sh <symbol> [<symbol> ...]
#
# harvest-symvers.py covers every vmlinux export that some installed module
# already imports. A new driver can use one that nothing else does, and modpost
# then reports it undefined. genksyms computes the same CRC the running kernel
# holds -- proven by 18 exact matches and no mismatch against the harvested set
# -- so building the object that exports the symbol and reading the #SYMVER
# lines out of its .o.cmd fills the gap exactly.
#
# A symbol can be exported from more than one file, only one of which this
# configuration builds: mm/nommu.c and mm/vmalloc.c both export
# remap_vmalloc_range, and nommu.c sorts first while being the wrong one. So
# every candidate is tried until one yields a CRC.
#
# Namespaced exports (EXPORT_SYMBOL_NS_GPL(sym, "NS")) keep their namespace, so
# that modpost still checks the module declares MODULE_IMPORT_NS for it.
#
# Run from the top of the kernel tree.
set -u
[ -f Module.symvers ] || { echo "run from the kernel tree root"; exit 1; }
[ $# -gt 0 ] || { echo "usage: $0 <symbol> [<symbol> ...]"; exit 1; }

SEARCH="drivers kernel lib mm fs net block crypto security sound arch/arm"
added=0

for sym in "$@"; do
	grep -q "	$sym	" Module.symvers && continue

	srcs=$(grep -rlE "EXPORT_SYMBOL[A-Z_]*\($sym[,)]" --include='*.c' \
		$SEARCH 2>/dev/null)
	[ -n "$srcs" ] || { echo "NOSRC   $sym"; continue; }

	for src in $srcs; do
		obj="${src%.c}.o"
		cmd="$(dirname "$obj")/.$(basename "$obj").cmd"
		[ -f "$cmd" ] || make -j"${J:-3}" LLVM=1 "$obj" >/dev/null 2>&1 || true
		[ -f "$cmd" ] || continue
		grep -q "#SYMVER $sym " "$cmd" || continue

		line=$(grep -hoE "EXPORT_SYMBOL[A-Z_]*\($sym[,)][^)]*" "$src" | head -1)
		ns=$(printf '%s' "$line" | sed -n 's/.*,[[:space:]]*"\([^"]*\)".*/\1/p')

		# take every export this object offers, not just the one asked for
		grep -o "#SYMVER [a-zA-Z0-9_]* 0x[0-9a-f]*" "$cmd" \
		| while read -r _ s c; do
			grep -q "	$s	" Module.symvers && continue
			sns=""
			grep -qE "EXPORT_SYMBOL_NS[A-Z_]*\($s," "$src" 2>/dev/null && sns="$ns"
			printf '%s\t%s\tvmlinux\tEXPORT_SYMBOL_GPL\t%s\n' "$c" "$s" "$sns" \
				>> Module.symvers
		done
		break
	done

	if grep -q "	$sym	" Module.symvers; then
		echo "added   $sym"
		added=$((added + 1))
	else
		echo "ABSENT  $sym  (tried: $(echo $srcs | tr '\n' ' '))"
	fi
done
echo "--- $added added; Module.symvers now $(wc -l < Module.symvers) lines"
