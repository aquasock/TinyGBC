#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The pad mapping on the PC (firmware/include/tinygbc_pad.h), which the player
# shares with this test, so a mapping fault is a failure here rather than a
# play session. What it pins: the default mapping is byte for byte the fixed
# decode the player had before the mapping existed, over all 65,536
# combinations of the report's two bytes; every Game Boy button is reachable
# from every offered source; an index past the table names no button; one
# source can answer to two buttons; and HOME is not offered at all, because a
# tap of it belongs to the desk layer.
#
#   tools/test_pad_map.sh
#
# Needs only a host C compiler. Exit 1 on any failed check.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cc=${CC:-cc}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

"$cc" -std=c11 -O1 -Wall -Wextra -Werror \
	-fsanitize=address,undefined -fno-sanitize-recover=all \
	-I "$root/firmware/include" \
	-o "$work/pad_map_tb" "$root/tests/pad_map_tb.c"

"$work/pad_map_tb"
