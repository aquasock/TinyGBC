#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulates the SDRAM controller's video line fetch against a behavioural
# SDRAM and checks a whole 160 halfword line, its indexes, its data and a CPU
# read through the line buffer. The video unit's own testbench models this
# channel, so this is the only test of ahb_sdram itself.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
mkdir -p build
iverilog -g2012 -o build/sdram_video_tb \
    fpga/ae350_soc/tb/sdram_video_tb.sv fpga/ae350_soc/ahb_sdram.sv 2>&1 |
    grep -v 'warning: Timescale' || true
[[ -x build/sdram_video_tb ]] || { echo "compile failed" >&2; exit 1; }
./build/sdram_video_tb | tee build/sdram_video_tb.log
grep -q '^PASS$' build/sdram_video_tb.log || { echo "simulation failed" >&2; exit 1; }
