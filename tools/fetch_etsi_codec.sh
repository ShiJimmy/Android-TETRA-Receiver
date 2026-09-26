#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Shi Jimmy
# ---------------------------------------------------------------------------
# Linux / macOS equivalent of tools/fetch_etsi_codec.ps1
#
# Downloads the ETSI EN 300 395-2 TETRA speech codec reference software,
# applies the osmo-tetra-sq5bpf-2 patch series and installs it into the
# Android project's native source tree.
#
# Usage:  sh tools/fetch_etsi_codec.sh [tetra-root]
# ---------------------------------------------------------------------------
set -e

here=$(cd "$(dirname "$0")" && pwd)
proj=$(cd "$here/.." && pwd)
tetra=${1:-$(cd "$proj/.." && pwd)}

url="http://www.etsi.org/deliver/etsi_en/300300_300399/30039502/01.03.01_60/en_30039502v010301p0.zip"
md5exp="a8115fe68ef8f8cc466f4192572a1e3e"
dl="$proj/thirdparty-src"
zip="$dl/etsi_tetra_codec.zip"
tmp="$dl/extract"
dst="$proj/app/src/main/cpp/thirdparty/etsi_codec"
patchdir="$tetra/osmo-tetra-sq5bpf-2-master/etsi_codec-patches"

[ -d "$patchdir" ] || { echo "patch directory not found: $patchdir" >&2; exit 1; }

mkdir -p "$dl"
if [ ! -f "$zip" ]; then
    echo "==> downloading ETSI reference codec"
    if command -v curl >/dev/null 2>&1; then curl -L -o "$zip" "$url"; else wget -O "$zip" "$url"; fi
fi

md5=$( (md5sum "$zip" 2>/dev/null || md5 -q "$zip") | awk '{print $1}')
[ "$md5" = "$md5exp" ] || { echo "MD5 mismatch: $md5" >&2; exit 1; }
echo "==> md5 ok"

rm -rf "$tmp"; mkdir -p "$tmp"
( cd "$tmp" && unzip -q -L "$zip" )

# lowercase everything (unzip -L is not available everywhere)
find "$tmp" -depth | while read f; do
    b=$(basename "$f")
    lb=$(echo "$b" | tr 'A-Z' 'a-z')
    [ "$b" = "$lb" ] || mv "$f" "$(dirname "$f")/__lc__$lb" 2>/dev/null || true
done
find "$tmp" -name '__lc__*' | while read f; do mv "$f" "$(dirname "$f")/$(basename "$f" | sed 's/^__lc__//')"; done

echo "==> applying patches"
( cd "$tmp" && for p in $(cat "$patchdir/series"); do
      echo "    - $p"
      if command -v patch >/dev/null 2>&1; then patch -p1 -s < "$patchdir/$p"; else git apply -p1 "$patchdir/$p"; fi
  done )

# unify the fixed point typedefs for both 32/64 bit builds
for h in amr-code/channel.h c-code/channel.h; do
    [ -f "$tmp/$h" ] || continue
    sed -i.bak -e 's/^typedef[ \t]*short[ \t]*Word16;/#include <stdint.h>\ntypedef int16_t Word16;/' \
               -e 's/^typedef[ \t]*long[ \t]*Word32;/typedef int32_t Word32;/' "$tmp/$h"
    rm -f "$tmp/$h.bak"
done

echo "==> installing into $dst"
rm -rf "$dst"; mkdir -p "$dst"
cp -r "$tmp/amr-code" "$tmp/c-code" "$dst/"
cp -r "$patchdir" "$dst/patches"
cat > "$dst/PROVENANCE.txt" <<EOF
ETSI EN 300 395-2 TETRA speech codec reference software
Source : $url
Archive: en_30039502v010301p0.zip (md5 $md5exp)
Fetched: $(date)
The codec is not redistributed with this project; it is downloaded on demand.
EOF
echo "done"
