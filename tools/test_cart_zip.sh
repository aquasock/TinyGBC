#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# The player's zip reader on the PC (firmware/common/cart_zip.c) over archives
# this script makes for itself, so the test depends on nothing but the fetched
# benchmark cartridge. Every archive is built from that one .gbc, which is
# also the reference each accepted archive is compared with byte for byte.
#
#   tools/test_cart_zip.sh
#
# Needs python3 (for zipfile) and a host C compiler. Exit 1 on any refusal that
# should have been accepted, any acceptance that should have been refused, or
# any byte that differs from the reference.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
rom="$root/third_party/roms/ucity.gbc"
decoder="$root/firmware/third_party/libretro-common"
[[ -f $rom ]] || { echo "no $rom: run tools/fetch_ucity.sh" >&2; exit 2; }

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cc=${CC:-cc}
"$cc" -std=c11 -O1 -Wall -Wextra -Werror \
	-fsanitize=address,undefined -fno-sanitize-recover=all \
	-I "$root/firmware/common" \
	-I "$decoder/include" \
	-I "$root/third_party/gambatte-libretro/libgambatte/libretro-common/include" \
	-x c - -x none "$root/firmware/common/cart_zip.c" \
	-x c -w "$decoder/encodings/encoding_deflate.c" \
	-o "$work/tb" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cart_zip.h"

static unsigned char *slurp(const char *path, size_t *len)
{
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char *p = malloc(n > 0 ? (size_t)n : 1);
	if (p && fread(p, 1, (size_t)n, f) != (size_t)n) {
		free(p);
		p = NULL;
	}
	fclose(f);
	*len = (size_t)n;
	return p;
}

/* ok <zip> <reference>: unpack and compare.  refuse <zip>: a refusal is the
 * pass.  Either way the byte after the last pair ends the run. */
int main(int argc, char **argv)
{
	int bad = 0;

	for (int i = 1; i < argc;) {
		const char *mode = argv[i++];
		const char *path = argv[i++];
		size_t zlen = 0;
		unsigned char *zip = slurp(path, &zlen);
		if (zip == NULL) {
			printf("MISSING %s\n", path);
			bad++;
			continue;
		}
		if (!cart_is_zip(zip, zlen)) {
			printf("NOT A ZIP %s\n", path);
			bad++;
			free(zip);
			continue;
		}
		unsigned char *rom = NULL;
		size_t len = 0;
		char name[128];
		const char *why = cart_unzip(zip, zlen, 4u << 20, &rom, &len,
					     name, sizeof name);
		if (strcmp(mode, "refuse") == 0) {
			if (why == NULL) {
				printf("ACCEPTED %s, should refuse\n", path);
				bad++;
				free(rom);
			} else {
				printf("refused %s: %s\n", path, why);
			}
			free(zip);
			continue;
		}
		if (why != NULL) {
			printf("REFUSED %s: %s\n", path, why);
			bad++;
			free(zip);
			continue;
		}
		size_t rlen = 0;
		unsigned char *ref = slurp(argv[i++], &rlen);
		if (ref == NULL || rlen != len || memcmp(ref, rom, len)) {
			printf("DIFFERS %s (%zu bytes inflated, %zu expected)\n",
			       path, len, rlen);
			bad++;
		} else {
			printf("ok %s as %s, %zu bytes\n", path, name, len);
		}
		free(ref);
		free(rom);
		free(zip);
	}
	return bad != 0;
}
EOF

# The fixtures: stored and deflated, both extensions, an ignored extra entry,
# and the four refusals. The archives go into $work and are named by case.
python3 - "$rom" "$work" <<'EOF'
import sys, zipfile
rom, work = sys.argv[1], sys.argv[2]
image = open(rom, "rb").read()

def make(name, entries, method):
    with zipfile.ZipFile(f"{work}/{name}", "w", method) as z:
        for arc, data in entries:
            z.writestr(arc, data)

make("deflated.zip", [("test.gbc", image)], zipfile.ZIP_DEFLATED)
make("stored.zip", [("test.gbc", image)], zipfile.ZIP_STORED)
make("lower.zip", [("test.gb", image)], zipfile.ZIP_DEFLATED)
make("upper.zip", [("TEST.GB", image)], zipfile.ZIP_DEFLATED)
make("nested.zip", [("roms/test.gbc", image), ("readme.txt", b"hello\n")],
     zipfile.ZIP_DEFLATED)
make("two.zip", [("a.gbc", image), ("b.gb", image)], zipfile.ZIP_DEFLATED)
make("nocart.zip", [("readme.txt", b"hello\n")], zipfile.ZIP_DEFLATED)

# A stored archive whose entry CRC-32 is wrong: flip a bit in the central
# directory's CRC field, which is the copy the reader checks.
raw = bytearray(open(f"{work}/stored.zip", "rb").read())
at = raw.find(b"PK\x01\x02")
assert at >= 0
raw[at + 16] ^= 0x01
open(f"{work}/badcrc.zip", "wb").write(raw)

# A stored archive cut short of its data.
open(f"{work}/short.zip", "wb").write(open(f"{work}/stored.zip", "rb").read()[:-64])
EOF

"$work/tb" ok "$work/deflated.zip" "$rom" \
	ok "$work/stored.zip" "$rom" \
	ok "$work/lower.zip" "$rom" \
	ok "$work/upper.zip" "$rom" \
	ok "$work/nested.zip" "$rom" \
	refuse "$work/two.zip" \
	refuse "$work/nocart.zip" \
	refuse "$work/badcrc.zip" \
	refuse "$work/short.zip"
