#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Simulates the retargeted video unit and checks the frame it draws. Run from
# anywhere: the testbench reads the desk font by a path relative to the
# repository root, so this changes there first.
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
mkdir -p build
iverilog -g2012 -o build/video_gb_tb \
    fpga/ae350_soc/tb/video_gb_tb.sv fpga/ae350_soc/video.sv 2>&1 |
    grep -v 'warning: Timescale' || true
[[ -x build/video_gb_tb ]] || { echo "compile failed" >&2; exit 1; }
./build/video_gb_tb | tee build/video_gb_tb.log
grep -q '^PASS$' build/video_gb_tb.log || { echo "simulation failed" >&2; exit 1; }
