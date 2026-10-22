#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Put this tree's AE350 SoC image on the board's card and program the FPGA
# with it, through TinyTang's shell on the two-wire console.
#
#   tools/deploy_tinygbc.sh                        # build/ae350_soc/ae350_soc.bin
#   tools/deploy_tinygbc.sh path/to/another.bin
#
# The board must be in two-wire mode, running TinyTang, with its console at a
# shell prompt; TinyTang's own tools check that before sending anything. They
# are looked for in $TINYTANG_TOOLS, then the usual checkouts. The card path
# and the port are $TINYGBC_CARD_PATH and $TINYGBC_PORT.
#
# This loads the core and leaves it running. A program is then loaded over the
# UART with tools/load_ae350.py --tinytang.
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
image=${1:-$root/build/ae350_soc/ae350_soc.bin}
remote=${TINYGBC_CARD_PATH:-/root/cores/console138k/tinygbc.bin}
port=${TINYGBC_PORT:-/dev/ttyACM0}

[[ -f $image ]] || { echo "no image at $image: run tools/build_ae350_soc.sh" >&2; exit 1; }

# A .bin is the .fs's data bits packed eight to a byte, most significant bit
# first, with nothing added (tools/fs_to_bin.py). The .fs file is larger than
# its own data -- it carries a // comment header -- so the check is to repack
# it and compare, not to compare file sizes.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
fs=${image%.bin}.fs
if [[ -f $fs ]]; then
	python3 "$root/tools/fs_to_bin.py" "$fs" -o "$tmp/check.bin" >/dev/null
	cmp -s "$tmp/check.bin" "$image" ||
		{ echo "$image is not $fs repacked; rebuild rather than deploy" >&2; exit 1; }
fi

tools=${TINYTANG_TOOLS:-}
if [[ -z $tools ]]; then
	for candidate in "$HOME/.copilot/repos/TinyTang/tools" \
			 "$HOME/GIT/TinyTang/tools" "$root/../TinyTang/tools"; do
		if [[ -f $candidate/tinytang_put.py ]]; then
			tools=$candidate
			break
		fi
	done
fi
[[ -n $tools ]] || { echo "TinyTang's host tools not found; set TINYTANG_TOOLS" >&2; exit 1; }

echo "image $(sha256sum "$image" | cut -d' ' -f1) $(stat -c %s "$image") bytes"
python3 "$tools/tinytang_put.py" "$image" "$remote" --port "$port"

# The desk layer mirrors the console over whatever core is loaded, so it is
# dropped before the FPGA is reprogrammed underneath it, as TinyTang's own
# boot.tdsh does. tangload then reports either "core loaded" or "failed".
log=$tmp/run.log
python3 "$tools/tinytang_run.py" "osd desk off" "tangload $remote" \
	--port "$port" --seconds "${TINYGBC_WAIT:-30}" | tee "$log"
grep -q 'tangload: core loaded' "$log" ||
	{ echo "tangload did not report 'core loaded'" >&2; exit 1; }
echo "deployed $remote"
