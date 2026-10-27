#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The picture settings' core options, on the PC. Two things have to be true and
# neither can be seen from the player alone: every setting names a value, and
# every key and value is one gambatte actually recognises. The first is
# tests/video_options_tb.c; the second is checked here against the core's own
# source, because a renamed option is a setting that silently does nothing --
# exactly the fault core-log entry 30 found on the wire, one layer up.
#
#   tools/test_video_options.sh
#
# Needs a host C compiler and the fetched core. Exit 1 on any failed check.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
core="$root/third_party/gambatte-libretro/libgambatte/libretro"
[[ -f $core/libretro.cpp ]] || {
	echo "no $core/libretro.cpp: run tools/fetch_gambatte.sh" >&2
	exit 2
}

cc=${CC:-cc}
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

"$cc" -std=c11 -O1 -Wall -Wextra -Werror \
	-fsanitize=address,undefined -fno-sanitize-recover=all \
	-I "$root/firmware/include" \
	-o "$work/video_options_tb" "$root/tests/video_options_tb.c"
"$work/video_options_tb"

# Every key the player answers and every value it can answer with, each of
# which has to be a string the core knows. The keys are read with var.key and
# the values compared by libretro.cpp or listed by libretro_core_options.h.
# The values are asked of the mapping itself rather than listed here, so that
# a mistyped value in the header fails this instead of passing it.
keys=(
	gambatte_gb_colorization
	gambatte_gb_internal_palette
	gambatte_gbc_color_correction
	gambatte_gbc_color_correction_mode
)
mapfile -t values < <("$work/video_options_tb" --values)
[[ ${#values[@]} -gt 0 ]] || { echo "FAIL the mapping answers nothing" >&2; exit 1; }

failures=0
for k in "${keys[@]}"; do
	if ! grep -q "var.key *= *\"$k\"" "$core/libretro.cpp"; then
		echo "FAIL the core does not read the option $k" >&2
		failures=$((failures + 1))
	fi
done
for v in "${values[@]}"; do
	if ! grep -qF "\"$v\"" "$core/libretro.cpp" "$core/libretro_core_options.h"; then
		echo "FAIL the core does not know the value \"$v\"" >&2
		failures=$((failures + 1))
	fi
done

if [ "$failures" -ne 0 ]; then
	echo "TinyGBC VIDEO OPTIONS FAILED $failures" >&2
	exit 1
fi
printf 'TinyGBC VIDEO OPTIONS PASS keys=%s values=%s\n' \
	"${#keys[@]}" "${#values[@]}"
