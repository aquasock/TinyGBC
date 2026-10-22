## 1 COMMIT Unreleased 2026-10-21T20:20:36-07:00

#### Coming From:

Unreleased (none: this is the initial commit, with no earlier HEAD).

#### Purpose:

Answer whether the A25 can run gambatte fast enough for a Color Game Boy game at the Game Boy's 59.7275 frames a second, before any FPGA work is committed to.

#### Outcome:

`firmware/apps/gb_bench` links gambatte d9d6cd06382d1ced30de34d56d3609452323dab1's libretro core and stands in for the frontend the way `nes_bench` drives FCEUmm in TinyNES: it loads a cartridge image built into the program, answers the core's environment calls itself, scripts the pad, and times `retro_run` against the Game Boy frame period of 16,742.7 us, reporting identical lines on both builds so that a PC run can be held against an AE350 run. The environment gives the core RGB565, declines its core options, its logging and its VFS, and points the system directory at a path with nothing in it, so no boot ROM is used and the core keeps its default integer resampler at 32,768 Hz; `VIDEO_RGB565` and the rest of the core are built with upstream's own defines. The cartridge is uCity 1.3, the open-source Game Boy Color city builder, 131,072 bytes of MBC5 with the CGB flag `0xC0`, fetched by `tools/fetch_ucity.sh` at a fixed release and checked by SHA-256 `9422ee2ca7b7ea1d46b58b2a429fff3f354dfd3e732dee1e7ae6220f148ce6e0`; gambatte is fetched at its pinned commit by `tools/fetch_gambatte.sh` and never vendored, so this tree carries neither. On the PC the program is deterministic and really emulating: two 1,800-frame runs both ended `VIDEO=aaddaa41 AUDIO=a36b5633 SAMPLES=987948`, and the frames it dumps decode to a coherent Color Game Boy tilemap with a UI panel rather than noise, while 987,948 samples over 1,800 frames is 548.9 a frame, which at 59.7275 frames a second is the 32,768 Hz the core reports. The AE350 program built from the same sources is 468,880 bytes of text including the 128 KiB cartridge, 21,252 of data and 58,656 of bss, and gambatte's own code is thus about 338 KiB, less than half of FCEUmm's 753 KiB program. TinyNES's AE350 SoC image, SHA-256 `69195778a66d006eda79432051d0ef8d523c7985eeb4077b4a7c61105111e1f4`, was put on the card as `/root/cores/console138k/gb_bench_ae350.bin` with TinyTang's `tangput` and loaded with `tangload`, which reported ID `0001081b`; `tools/load_ae350.py --tinytang` then loaded `gb_bench` over the two-wire console in 1,800 frames averaging 4,110 us a frame, minimum 3,697 and maximum 7,283, against the 16,742.7 us period, with no frame over and 407.3 percent, and every checksum line equal to the PC's, `VIDEO=aaddaa41 AUDIO=a36b5633`. The question is therefore answered in the affirmative with room to spare: gambatte runs a Color Game Boy title that is entirely CPU work at about 2.6 times full speed, where FCEUmm on the NES reached 1.54 times. Three limits belong with that number. The harness shows no picture, so this is throughput and not display work. It cannot say whether uCity ever switched the CPU to double speed, which gambatte does not report through libretro. And it ran with no scripted input, so the scene measured is the map view the game reaches by itself, which may be lighter than play. Two names are inherited from TinyNES and will stay until this tree has its own boot ROM: the runtime's exit line is `TinyNES EXIT` and the boot ROM's loader prints `TinyNES LOAD`. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 1 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, and allowed Status values; the one departure is Coming From, which names the absence of an earlier HEAD because this is the initial commit.

#### Next Steps:

The port needs a picture next, and that is where the work stops being reusable: the video unit's raster, clocks and pixel format are built around the NES's 256x240 at 60.0988 Hz, while the Game Boy is 160x144 at 59.7275 Hz from a 4.194304 MHz dot clock in 15-bit colour, and the I2S path's 48,828.125 Hz ACR has to be re-derived for the Game Boy's rates. Before that work starts, the harness can settle two things cheaply: a run whose scripted pad reaches uCity's simulation, so the number reflects play rather than the idle map, and a run of a title known to use double speed, since double speed is the case that would halve the margin. TinyGBC also still borrows TinyNES's AE350 SoC image, its boot ROM and its loader, so its own `fpga/` and boot ROM are the boundary at which `TinyNES LOAD` and `TinyNES EXIT` become TinyGBC's, and at which the project stops depending on another tree being built.

#### Files Modified:

- .gitignore
- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/apps/gb_bench/rom.S
- firmware/common/uart.c
- firmware/common/uart.h
- firmware/include/tinygbc.h
- firmware/rt/crt0.S
- firmware/rt/include/sys/dirent.h
- firmware/rt/link.ld
- firmware/rt/rt.mk
- firmware/rt/syscalls.c
- tests/gb_bench_expected.txt
- tools/fetch_gambatte.sh
- tools/fetch_ucity.sh
- tools/load_ae350.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 2 COMMIT Unreleased 2026-10-21T20:34:23-07:00

#### Coming From:

Unreleased 4f2309c

#### Purpose:

Measure gambatte on the A25 with the user's own cartridge, The Legend of Zelda - Oracle of Seasons, a banked 1 MiB Color Game Boy game rather than the 128 KiB homebrew image of entry 1.

#### Outcome:

The user put their own cartridge in place of entry 1's uCity: Legend of Zelda, The - Oracle of Seasons (USA, Australia), 1,048,576 bytes with SHA-256 `862a51368fb30539279d336b3fe193b43876d2cb15c87a36f5da517804ab3971`, the CGB flag `0xC0`, MBC5 with RAM and battery, and the internal title `ZELDA DIN`. Driving it took longer than the emulation did, and changed the harness twice. The script table held 64 presses, and a run long enough to cross this game's opening needs hundreds, so the program answered `TinyGBC GB BAD INPUT` until the table was raised to 2,048. More revealing, the first 12,000 frames of the first attempt were byte-identical to a run with no input at all: a START pressed at frame 120 or at frame 300 lands on the Nintendo logo, where the game ignores it, so every later press of A fell on a title screen that only accepts START. Rather than assume the pad path, it was proved by running uCity with right held and with down held and watching both change the checksums away from a run with no input; the script that works alternates START and A every 60 frames from frame 600, 647 presses in all, and is stored as `tests/zelda_input.txt` so the run can be repeated without rediscovering it. With that script, holding right from frame 22,000 changes both the picture and the sound where holding it earlier changed nothing. On the PC the program is deterministic over 24,000 frames, two runs both ending `VIDEO=900c74b5 AUDIO=0630f3c5` with 13,167,010 samples. The AE350 program is 1,394,524 bytes of text, which is gambatte plus the megabyte cartridge, 21,252 of data and 82,464 of bss. The board had re-enumerated since entry 1, so the first load failed for want of a `TinyNES LOAD` line; `tangload` of the image already on the card, SHA-256 `69195778a66d006eda79432051d0ef8d523c7985eeb4077b4a7c61105111e1f4`, restored it, and `tools/load_ae350.py --tinytang` then ran the program: 24,000 frames averaged 4,565 us, minimum 215 and maximum 9,222, with no frame over the 16,742.7 us period, 366.7 percent, 13,167,010 samples and every checksum equal to the PC's. Oracle of Seasons therefore runs at about 3.7 times full speed on the A25 against uCity's 4.07 in entry 1, which is the direction expected: the larger game and its banked code cost more, and even the slowest frame, 9,222 us, left more than half the period unused. One limit belongs with the number: the script reaches the game's menus, name entry and opening but not confirmed player control, so the figure describes this cartridge's boot, title, file select and intro rather than a measured overworld, and the harness has no display, so the screens after frame 18,000 are text and menu boxes that checksums alone cannot identify further. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entry 1 was not rewritten, and validated this entry as number 2 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and no ineligible paths in Files Modified.

#### Next Steps:

Turning this into a gameplay number needs a known input path rather than a guessed one, and there are two cheap ways to get it: drive the menus by hand once with a picture attached and write the presses down, or give the harness a savestate so a state taken in the overworld can be loaded and run for a fixed stretch. The double-speed question of entry 1 is still open and matters more with a title of this size, since double speed is what would halve the remaining margin. The port itself still needs a picture, and that remains the point at which the video unit's raster and clocks, the 15-bit colour, the audio rate and the pad mapping stop being reusable from the NES and start being TinyGBC's own.

#### Files Modified:

- firmware/apps/gb_bench/main.cpp
- tests/gb_bench_expected.txt
- tests/zelda_input.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
