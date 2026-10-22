# TinyGBC

A Game Boy Color port in the form TinyNES took: gambatte on the AE350 inside
the Tang Console 138K.

**Status: feasibility cycle, not a port yet.** There is no FPGA work in this
tree and no picture. What exists is `gb_bench`, the harness that answers the
one question the port depends on: can the A25 run gambatte fast enough for a
Color Game Boy game at 59.7275 frames a second?

## gb_bench

`firmware/apps/gb_bench` links gambatte's libretro core and stands in for the
frontend, the way `nes_bench` links FCEUmm's core in TinyNES. It loads a
cartridge image built into the program, runs a scripted pad, and times
`retro_run` against the Game Boy frame period (70,224 dots at 4.194304 MHz =
16,742.7 us), reporting the same lines on the AE350 and on the PC so the two
can be compared:

```
TinyGBC GB CORE=gambatte ROM=<bytes> CRC=<crc32> CGB=<0|1> AV=160x144 FPS=... START=<us>
TinyGBC GB F=<frame> AVG=<us> MAX=<us> VIDEO=<crc> AUDIO=<crc>
TinyGBC GB FRAMES=<n> AVG=<us> MIN=<us> MAX=<us> OVER=<n> SPEED=<x.y>% SAMPLES=<n> VIDEO=<crc> AUDIO=<crc>
```

The core is built with `VIDEO_RGB565`, its default integer (blipper) audio
resampler at 32,768 Hz, no boot ROM, and no core options, so a PC run and an
AE350 run of the same program and cartridge produce the same `VIDEO` and
`AUDIO` checksums. On the PC, `GB_DUMP=<dir>` also writes the CHECK frames as
PPM.

## Building and running

```
tools/fetch_gambatte.sh                 # gambatte at a pinned commit
tools/fetch_ucity.sh                    # uCity 1.3, the benchmark cartridge
cd firmware/apps/gb_bench
make ROM=../../../third_party/roms/ucity.gbc host   # build/firmware/apps/gb_bench/host/gb_bench
make ROM=../../../third_party/roms/ucity.gbc        # the AE350 program
```

`FRAMES=<n>`, `INPUT="<first>+<count>=<buttons> ..."` and `CHECK=<n>` are
build-time options. The A25 program needs the AE350 SoC bitstream and the
boot ROM's loader, which are TinyNES's until this tree has its own.

## Licensing

This project's code is MIT. gambatte is GPL-2.0 and is fetched, never vendored:
`tools/fetch_gambatte.sh` pins the commit and `third_party/` is git-ignored, so
only patches to the core would be carried here. Programs built from it,
including `gb_bench`, are GPL-2.0 and stay in `build/`. The benchmark
cartridge, uCity, is GPL-3.0 and is fetched the same way.
