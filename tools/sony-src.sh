#!/bin/sh
# Print one file from Sony's published kernel, sonyxperiadev/kernel.
#
# The clone is a partial clone (filter=blob:none), so any of the 42,256 files
# in the tree can be printed whether or not it is in the sparse checkout: git
# fetches the blob on demand. Use this rather than widening the checkout for a
# file you only want to read once.
#
# Every audit finding in docs/sony-source-audit.md cites a file, a branch and a
# commit. This prints the commit to stderr so it can be copied into the finding
# while the file itself goes to stdout and stays pipeable.
#
#   sony-src.sh drivers/input/touchscreen/max1187x.c
#   sony-src.sh -b 3.5.1 drivers/media/.../sony_camera_v4l2.c | sed -n '1,80p'
#   sony-src.sh -l camera_v2/sensor          # list matching paths
#   sony-src.sh -B                           # list the branches and commits
#
# Copyright (c) 2026 Rob Watson <rob@mediaeden.com>
# SPDX-License-Identifier: GPL-2.0-only

set -eu

REPO=${SONY_KERNEL:-}
if [ -z "$REPO" ]; then
	# Default to the clone beside this checkout, whichever of the two trees
	# this script was run from.
	for d in \
		"$(dirname "$0")/../../upstream-src/sony-kernel" \
		"$(dirname "$0")/../upstream-src/sony-kernel"
	do
		[ -d "$d/.git" ] && REPO=$d && break
	done
fi
[ -n "$REPO" ] && [ -d "$REPO/.git" ] || {
	echo "sony-src: no Sony kernel clone found; set SONY_KERNEL" >&2
	exit 2
}

# Branch shorthands. The sirius files are not all on one branch: the newest
# 8x74 branch drops sony_camera_v4l2.c, and the Broadcom Bluetooth work is on a
# much later branch made for other devices.
resolve_branch() {
	case "$1" in
	default|3.5.2.2|"") echo "origin/aosp/LNX.LA.3.5.2.2-03010-8x74.0" ;;
	3.5.1|camera)       echo "origin/aosp/LNX.LA.3.5.1-01110-8x74.0" ;;
	5.5|bt|fm)          echo "origin/aosp/LA.UM.5.5.r1" ;;
	*)                  echo "$1" ;;
	esac
}

BRANCH=$(resolve_branch default)
MODE=show

while getopts "b:lBh" opt; do
	case "$opt" in
	b) BRANCH=$(resolve_branch "$OPTARG") ;;
	l) MODE=list ;;
	B) MODE=branches ;;
	h|?) sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	esac
done
shift $((OPTIND - 1))

case "$MODE" in
branches)
	for b in default 3.5.1 5.5; do
		r=$(resolve_branch "$b")
		printf '%-12s %-45s %s\n' "$b" "${r#origin/}" \
			"$(git -C "$REPO" rev-parse --short "$r" 2>/dev/null || echo '(not fetched)')"
	done
	exit 0
	;;
list)
	[ $# -ge 1 ] || { echo "sony-src: -l needs a pattern" >&2; exit 2; }
	git -C "$REPO" ls-tree -r --name-only "$BRANCH" | grep -- "$1"
	exit 0
	;;
esac

[ $# -eq 1 ] || { echo "usage: sony-src.sh [-b branch] <path>" >&2; exit 2; }

commit=$(git -C "$REPO" rev-parse --short "$BRANCH")
echo "sony-src: ${BRANCH#origin/} $commit $1" >&2
git -C "$REPO" show "$BRANCH:$1"
