#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Fetches µCity 1.3 (GPL-3.0), a Game Boy Color city builder by AntonioND, as
# the benchmark cartridge: a Color Game Boy image that is a real game rather
# than a test ROM, small enough to build into the program, and fetched at a
# fixed release so the measurement is reproducible. It goes into
# third_party/roms, which git ignores; nothing of it is carried here.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
url=https://github.com/AntonioND/ucity/releases/download/v1.3/ucity.gbc
sha256=9422ee2ca7b7ea1d46b58b2a429fff3f354dfd3e732dee1e7ae6220f148ce6e0
out="${UCITY_ROM:-$root/third_party/roms/ucity.gbc}"

if [[ ! -f "$out" ]] || [[ "$(sha256sum "$out" | cut -d' ' -f1)" != "$sha256" ]]; then
    mkdir -p "$(dirname "$out")"
    tmp="$out.part"
    curl -sSL --fail --max-time 120 -o "$tmp" "$url"
    got=$(sha256sum "$tmp" | cut -d' ' -f1)
    [[ "$got" == "$sha256" ]] || {
        rm -f "$tmp"
        echo "ucity: SHA-256 is $got, want $sha256" >&2; exit 1; }
    mv "$tmp" "$out"
fi
echo "ucity $sha256 at $out"
