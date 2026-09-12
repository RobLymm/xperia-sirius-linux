#!/bin/sh
set -u
echo "=== $(date) start as $(id -un), groups: $(id -Gn)"
cd $HOME/build/gnome-software || { echo "no build dir"; exit 1; }
echo "=== $(date) abuild -r"
abuild -r 2>&1 | grep -vE '^\s*$' | tail -80
echo "=== $(date) result:"; ls -la $HOME/packages/build/armv7/ 2>/dev/null
echo "=== done"
