#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Fetches the libretro gambatte core (GPL-2.0) at a pinned commit into
# third_party/gambatte-libretro, which git ignores. TinyGBC carries patches
# to the core, not the core itself, so anything built from it, such as the
# gb_bench program, is GPL-2.0 too and stays in build/.
#
# The checkout is accepted only as the pinned commit with no changes, so a
# local edit is refused rather than overwritten.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
url=https://github.com/libretro/gambatte-libretro.git
commit=d9d6cd06382d1ced30de34d56d3609452323dab1
dir="${GAMBATTE_DIR:-$root/third_party/gambatte-libretro}"

if [[ ! -d "$dir/.git" ]]; then
    mkdir -p "$dir"
    git -C "$dir" init -q
    git -C "$dir" remote add origin "$url"
fi
if [[ "$(git -C "$dir" rev-parse -q --verify HEAD || true)" != "$commit" ]]; then
    git -C "$dir" fetch -q --depth 1 origin "$commit"
    git -C "$dir" checkout -q --detach "$commit"
fi
[[ "$(git -C "$dir" rev-parse HEAD)" == "$commit" ]] || {
    echo "gambatte: HEAD is not $commit" >&2; exit 1; }
[[ -z "$(git -C "$dir" status --porcelain)" ]] || {
    echo "gambatte: $dir has changes; commit them as a patch or discard them" >&2
    exit 1; }
echo "gambatte $commit in $dir"
