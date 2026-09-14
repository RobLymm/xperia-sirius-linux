#!/bin/sh
# Build and install the fmrepair ALSA plugin on the phone (needs alsa-lib-dev).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
CC=${CC:-$(command -v clang || command -v cc || command -v gcc)}
PLUGDIR=/usr/lib/alsa-lib
CONFDIR=/etc/alsa/conf.d
$CC -O2 -Wall -shared -fPIC -DPIC -o "$HERE/libasound_module_pcm_fmrepair.so" \
	"$HERE/pcm_fmrepair.c" -lasound
sudo install -m 0755 "$HERE/libasound_module_pcm_fmrepair.so" $PLUGDIR/
sudo install -m 0644 "$HERE/60-sirius-fm.conf" $CONFDIR/
echo "installed: $PLUGDIR/libasound_module_pcm_fmrepair.so, $CONFDIR/60-sirius-fm.conf"
