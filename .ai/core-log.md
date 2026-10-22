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
## 3 COMMIT Unreleased 2026-10-21T20:44:12-07:00

#### Coming From:

Unreleased d21c72a

#### Purpose:

Measure gambatte on the A25 from a savestate the user took while walking the overworld, so that the number describes play rather than the menus and opening that entry 2 could only reach with a guessed input script.

#### Outcome:

The harness gained a playable host build and a way to start a run from a state. `make ROM=<file> PLAY=1 host` builds `gb_play`, which links the same core and opens a 640x576 X11 window, Xlib standing in for SDL2 because this machine has no SDL2 headers; it runs at 59.7275 Hz, takes the arrows as the d-pad, `Z` as B, `X` as A, `Enter` as Start and `Right Shift` as Select, and writes `gb_play.state` on `F5`, on `SIGUSR1`, on `SIGINT` or `SIGTERM`, on quit, and a rolling `gb_play.state.auto` every 600 frames, so a state is never far behind what is being played. `STATE=<file>` embeds a savestate through `state.S` the way `ROM=` embeds the cartridge and calls `retro_unserialize` after `retro_load_game`; with `STATE` unset the embedded file is empty and the program skips it, so entry 1 and entry 2's commands are unchanged. The user played from boot in that window and pressed `F5` at frame 7872: 59,629 bytes, SHA-256 `49d4a6971587ce6bcc6ca10ca321dbdfeffe225d7fa776fdb44aead0b90484cf`. The state was then proved interactive rather than assumed to be, since three runs of 1,800 frames from it ended `VIDEO=ba486472` with no input, `VIDEO=a0c145db` with right held and `VIDEO=ce76d09a` with down held. The gameplay run is 3,600 frames from that state with right, down, left and up walked in 600-frame blocks, `tests/zelda_walk.txt`, 120 presses; on the PC it is deterministic, two runs ending `VIDEO=42c0e370 AUDIO=b0c1088a` with 1,975,050 samples. The AE350 program is 1,477,622 bytes of text, 22,220 of data and 82,488 of bss, and the same run on it averaged 5,063 us a frame, minimum 4,265 and maximum 6,118, with no frame over the 16,742.7 us period, 330.6 percent, the same 1,975,050 samples and checksums equal to the PC's. Oracle of Seasons therefore plays at about 3.3 times full speed on the A25, and play costs about 11 percent more than the boot-to-intro figure of entry 2, 5,063 against 4,565 us, so entry 2's number understated play by roughly that much and this one replaces it as the figure to quote. The worst gameplay frame still used little more than a third of the frame period. The savestate is a derivative of the cartridge and stays in `build/`, not in this tree; the run is reproducible from the cartridge, the state and `tests/zelda_walk.txt`, and the state is identified by its SHA-256 above. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 and 2 were not rewritten, and validated this entry as number 3 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Player control is now measured, so the remaining unknown in the throughput question is double speed, which gambatte does not report through libretro and which a state taken inside a game known to use it would settle. The port still needs a picture, and that is the next real work: a raster and clock generator for 160x144 at 59.7275 Hz from a 4.194304 MHz dot clock instead of the NES's 256x240 at 60.0988 Hz, direct 15-bit colour in place of the palette-indexed path, an audio rate derived for the Game Boy rather than the NES's 48,828.125 Hz, and the pad mapped onto the Wii Classic Controllers that already feed TinyNES. Until then `gb_bench` remains a throughput harness that borrows TinyNES's AE350 SoC image, boot ROM and loader.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/apps/gb_bench/state.S
- tests/gb_bench_expected.txt
- tests/zelda_walk.txt

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---
## 4 COMMIT Unreleased 2026-10-21T21:14:25-07:00

#### Coming From:

Unreleased 0dd2625

#### Purpose:

Bring the AE350 SoC into this tree and retarget its video unit from the NES's 256x240 indexed picture to the Game Boy's 160x144 frame in 15-bit colour, so that the port stops borrowing another tree's gateware.

#### Outcome:

`fpga/ae350_soc` and `fpga/hdmi` are TinyNES's, imported whole with `firmware/boot` and the build tools that drive them, and the rename that follows from no longer sharing a header is `tinynes.h` to `tinygbc.h` in the boot ROM, the runtime and `memtest.c`, with the build script's `TINY_NES_*` environment variables becoming `TINYGBC_*`. The first build showed that TinyNES's seed does not transfer: seed 11, which is TinyNES's pick for its own tree, failed here at 72.15 MHz on the AHB bus, so a sweep of seeds 1 to 8 ran, of which seed 1 failed at 74.04 and seeds 2, 3, 5, 6 and 7 were clean between 76.68 and 80.46 MHz with the HDMI pixel clock between 81.74 and 89.06, and no clean seed reported a hold violation; seed 4 had the best bus margin at 80.46 and became the default. The video unit is the part that is now TinyGBC's own. Two 160x144 frame buffers of RGB555 sit in the top 256 KiB of the SDRAM, line y at `FB(b) + 512 * y` and pixel (x, y) at `2 * x`, one 16-bit word per pixel with red in bits 4:0, green in 9:5 and blue in 14:10; the line buffer doubled to 512 halfwords and is indexed by pixel rather than by pair; the SDRAM video channel fetches 160 halfwords a line instead of 128, so `vindex` in `ahb_sdram.sv` widened to eight bits and `vdone` fires at 159; and the scaler now draws each Game Boy pixel five rows tall and five columns wide for an exact 800x720 picture, or six columns for 960 at 4:3, with the 8:7 setting collapsing onto 1:1 because the Game Boy's pixels are square. The palette and emphasis stores stay in the register map but the picture reads neither, so `rgb3` is `rgb15` of the frame-buffer word and the diagnostics lost the emphasis index. `firmware/rt/link.ld` keeps the heap below the top 256 KiB instead of 128, and `tinygbc.h` publishes the new base, stride and pixel size. With seed 4 the retargeted design places and routes at 84.10 MHz on the bus and 101.21 MHz on the pixel clock, both better than the imported design's 80.46 and 89.06 because the palette and emphasis block RAMs left the video unit; the AE350's dedicated PLL hop is present, all ten of the video unit's block RAMs are on one clock each, and the pixel clock landed on SPINE16 and SPINE25, the pair this board is known to run. The bitstream packs to 4,666,860 bytes with SHA-256 `66f960f020d4a22336941fa29aeb842361e4faac73311953bf5cb62cee90846d`, 79,360 bytes smaller than the NES image it descends from. Nothing was loaded onto the board, by the user's instruction, and nothing was simulated: this entry establishes that the retargeted unit synthesises, meets both clocks and packs onto the part, not that it draws a Game Boy frame, and the A25 still has no code that writes one. Two inherited names remain in the boot ROM and the runtime, `TinyNES LOAD` and `TinyNES EXIT`, because `tools/load_ae350.py` looks for them. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 3 were not rewritten, and validated this entry as number 4 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The picture needs two things before it can be called working: a way to write frames into it from the A25, which is `gb_bench` converting gambatte's RGB565 output to RGB555 into the frame buffer not being shown and handing it over at the vertical blank, exactly as `nes_bench` does for FCEUmm; and a simulation of the retargeted unit that drives a known frame through the fetch handshake and checks the colours that reach the RGB output, since the build proves only that it synthesises and times. After those, deploying is a `tangload` and a run of the same benchmark, whose checksums already match the PC's, so any picture difference is the video unit's. The audio rate and the Wii pad mapping are still NES-shaped and remain their own cycles.

#### Files Modified:

- README.md
- firmware/boot/Makefile
- firmware/boot/crt0.S
- firmware/boot/link.ld
- firmware/boot/load.c
- firmware/boot/load.h
- firmware/boot/main.c
- firmware/common/memtest.c
- firmware/common/memtest.h
- firmware/include/tinygbc.h
- firmware/rt/link.ld
- fpga/ae350_soc/ae350_soc.cst
- fpga/ae350_soc/ae350_soc.sdc
- fpga/ae350_soc/ae350_soc.sv
- fpga/ae350_soc/ahb_boot_rom.sv
- fpga/ae350_soc/ahb_ram.sv
- fpga/ae350_soc/ahb_sdram.sv
- fpga/ae350_soc/desk_font.hex
- fpga/ae350_soc/desk_link.sv
- fpga/ae350_soc/exts_uart.sv
- fpga/ae350_soc/i2s_out.sv
- fpga/ae350_soc/video.sv
- fpga/ae350_soc/wii_pads.sv
- fpga/hdmi/LICENSE-MIT
- fpga/hdmi/audio_clock_regeneration_packet.sv
- fpga/hdmi/audio_info_frame.sv
- fpga/hdmi/audio_sample_packet.sv
- fpga/hdmi/auxiliary_video_information_info_frame.sv
- fpga/hdmi/hdmi.sv
- fpga/hdmi/hdmi_silent.sv
- fpga/hdmi/packet_assembler.sv
- fpga/hdmi/packet_picker.sv
- fpga/hdmi/serializer.sv
- fpga/hdmi/source_product_description_info_frame.sv
- fpga/hdmi/tmds_channel.sv
- tools/build_ae350_soc.sh
- tools/designs/ae350_soc.json
- tools/fs_to_bin.py
- tools/sweep.py

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---
## 5 COMMIT Unreleased 2026-10-21T21:35:10-07:00

#### Coming From:

Unreleased ee8e687

#### Purpose:

Verify the retargeted video unit by simulation, since entry 4 proved only that it synthesises and times, and rebuild the bitstream from whatever the simulation found.

#### Outcome:

`tools/sim_video_gb.sh` runs `fpga/ae350_soc/tb/video_gb_tb.sv` under Icarus Verilog and fails unless the testbench prints `PASS`. The testbench drives the unit the way the top level does: a 1650x750 raster on `pix_clk` at 74.25 MHz, an AHB write of CTRL to select the frame buffers, and an SDRAM video model that answers each request with 160 halfwords of colour for the line `vaddr` names, line L and pixel X carrying `{L[4:0], X[4:0], 5'd1}` so every source pixel is identifiable, and it captures the RGB output into a frame image. The first run found a defect that the synthesis and timing could not: `vindex` was still seven bits wide in `video.sv`'s port list even though `ahb_sdram.sv` had been widened to eight, so only 128 of the 160 words a line carries could address the line buffer and pixels 128 to 159 of every line aliased onto 0 to 31. With the port widened to eight bits the testbench passes eleven checks: the centre pixel's colour is exact, a 5x5 block of one source pixel is uniform across three columns and one row beyond the centre, the next column carries the next source pixel, the next row carries the next source line, a point 40 columns left of centre carries the source pixel 8 columns left, both side bars are black, and the frame is fetched as 144 lines of 160 halfwords. Two measurements from the simulation are worth carrying forward because nothing states them. The RGB output corresponds to the raster eight clocks earlier, not the four that the unit's own comment claims, which is how the testbench associates a colour with a screen position; the mapping is right but the comment is not. And the picture settles two frames after the CTRL write rather than one: the frame after the write still shows colour bars, and the picture appears from the second. Rebuilding with seed 4 and the corrected port gives 92.26 MHz on the bus against 75 and 101.05 on the pixel clock against 74.25, both better than entry 4's 84.10 and 101.21, with the dedicated AE350 hop present, ten video block RAMs each on one clock, and the pixel clock again on SPINE16 and SPINE25; the bitstream is 4,762,092 bytes with SHA-256 `9be1b0b7a842485702ebd0dee3b6ae8d77200e5eb3f62994dcaf7f8bf3513f1e`, and entry 4's `66f960f0` image, built before this fix, is superseded. Nothing was loaded onto the board and the A25 still writes no frames, so the picture path is now verified in simulation and by synthesis but has never been through a display. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 4 were not rewritten, and validated this entry as number 5 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The A25 needs to put a frame in the buffer: `gb_bench` converting gambatte's RGB565 to RGB555 into the buffer not being shown, writing the D-cache back and handing it over at the vertical blank, which is what `nes_bench` does for FCEUmm in TinyNES and what makes the checksums already held in `tests/gb_bench_expected.txt` a test of the video unit as well as the core. Deploying is then a `tangload` of the bitstream above and a run of that program, and the first thing to expect is the two-frame settle the simulation measured.

#### Files Modified:

- fpga/ae350_soc/tb/video_gb_tb.sv
- fpga/ae350_soc/video.sv
- tools/sim_video_gb.sh

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 6 COMMIT Unreleased 2026-10-22T06:33:09-07:00

#### Coming From:

Unreleased db9db2a

#### Purpose:

Close the five discrepancies entry 5 left open in this tree's naming, records, comments and deployment path, so the next cycle starts from a tree that describes itself accurately.

#### Outcome:

The boot ROM and the runtime stopped borrowing TinyNES's names: `firmware/boot/main.c` prints `TinyGBC AE350 boot ROM`, `TinyGBC AE350 C OK`, `TinyGBC SDRAM OK 32M` and `TinyGBC LOAD`, `firmware/boot/load.c` prints `TinyGBC RUN`, and `firmware/rt/syscalls.c` prints `TinyGBC EXIT`, with `tools/load_ae350.py`'s waits, expectations, WANT and LINK recognition and docstrings following, `firmware/boot/load.h`'s include guard renamed, and README no longer claiming the boot ROM is TinyNES's. `fpga/ae350_soc/video.sv`'s `FB_BASE` comment now states what the parameter is, bits 24:18 holding 0x41fc0000, the lower of the two 128 KiB buffers that make up the top 256 KiB of the SDRAM, instead of naming bits 24:17 and the top 128 KiB. `tools/deploy_tinygbc.sh` is new and is the deployment path this tree lacked: it repacks the `.fs` with `tools/fs_to_bin.py` and compares the result with the `.bin` byte for byte rather than by size, because the `.fs` carries a comment header and is not eight times its own data; it finds TinyTang's host tools in `$TINYTANG_TOOLS`, `~/.copilot/repos/TinyTang/tools` or `~/GIT/TinyTang/tools`, sends the image with `tinytang_put.py` to `/root/cores/console138k/tinygbc.bin`, drops the desk layer, runs `tangload` and requires the line `tangload: core loaded`. The boot ROM built to 2,646 bytes of text and `gb_bench` to 498,470 of text, 22,220 of data and 82,488 of bss. With seed 4 the SoC places and routes as entry 5 left it, 92.26 MHz on the bus and 101.05 on the pixel clock, ten video block RAMs each on one clock and the pixel clock through SPINE16 and SPINE25, and packs to a 38,125,541-byte `.fs` with SHA-256 `71e2b03a2eca4ca030091f951b54e872b96e6ebcca11df8a98e22edccfca8067` and a 4,762,092-byte `.bin` with `a2a334cf14374870ff0737661f3db31ebc25b14bd6f53100ddb5c3e601e5a3b1`; `tools/sim_video_gb.sh` still passes eleven checks with no failure over 144 fetches. Deployment ran on the board in two-wire mode, after one attempt found no `/dev/ttyACM0` because the console had re-enumerated and the user reconnected it: `tangput` wrote 4,762,092 bytes and `tangload` reported `ID=0001081b` and `core loaded`. `tools/load_ae350.py --tinytang` then loaded `gb_bench` in 2.79 s at 186.4 KB/s, the ROM printed `TinyGBC AE350 C OK UNC=0491cd60 CAC=001f6b4a`, `TinyGBC SDRAM OK 32M`, `TinyGBC LOAD` and `TinyGBC RUN 40000000`, the benchmark ran 1,800 frames averaging 4,086 us with a 7,332 us maximum, no frame over, 409.7 percent and 987,948 samples, and it ended `TinyGBC EXIT 0` with `VIDEO=aaddaa41 AUDIO=a36b5633`, equal to `tests/gb_bench_expected.txt`. In the reference library DEV-011 was revised to name `firmware/include/tinygbc.h` and the frame buffers at 0x41fc0000 and 0x41fe0000, with a Revised note citing this entry, and `last_reviewed` moved to 2026-10-22. The picture path itself is untouched: no code writes a frame buffer yet, so the core shows its fabric colour bars and the next cycle's work stands as entry 5 described it. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 5 were not rewritten, and validated this entry as number 6 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The A25 still has to put a frame in a buffer: `gb_bench` converting gambatte's RGB565 into the unit's BGR555, red in bits 4:0, green 9:5 and blue 14:10, which is not libretro's 0RGB1555 and swaps red with blue if written straight, into the buffer not shown, writing the D-cache back and handing the buffer over at the vertical blank, with the two-frame settle entry 5 measured accounted for and handovers skipped while the display has not taken the last one, since the harness runs at about 400 percent of the Game Boy's frame rate. The channels that are still NES-shaped come after the picture: the core's 32,768 Hz sound against the DAC's 48,828.125 Hz, and the Wii pad mapping onto gambatte's input.

#### Files Modified:

- README.md
- firmware/boot/Makefile
- firmware/boot/link.ld
- firmware/boot/load.c
- firmware/boot/load.h
- firmware/boot/main.c
- firmware/include/tinygbc.h
- firmware/rt/rt.mk
- firmware/rt/syscalls.c
- fpga/ae350_soc/video.sv
- tools/deploy_tinygbc.sh
- tools/load_ae350.py

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 7 COMMIT Unreleased 2026-10-22T06:39:26-07:00

#### Coming From:

Unreleased 1f455e4

#### Purpose:

Give the A25 a path that puts gambatte's frame into the SDRAM frame buffer and onto HDMI, which is the picture the port has built, timed and simulated but never fed.

#### Outcome:

`make DISPLAY=1` builds `gb_bench` with `GB_DISPLAY`: every frame the core hands over is converted from its RGB565 into the video unit's 15-bit format, red in bits 4:0, green 9:5 and blue 14:10, which is not libretro's 0RGB1555 and would swap red with blue if written straight through, and is written into the frame buffer that `TN_VIDEO_STATUS`'s buffer bit does not name, at `TN_VIDEO_FB` with the 512-byte line stride against the core's 320-byte pitch, after which the D-cache is written back and `TN_VIDEO_CTRL` takes the enable and the buffer. Handover happens at most once per vertical blank, because the harness runs at about four times the Game Boy's frame rate and flipping faster would only discard pictures, and the picture is expected to settle two frames after the write (entry 5). The default build is untouched: it is 498,470 bytes of text, the same as entry 6's, and its PC run still ends `VIDEO=aaddaa41 AUDIO=a36b5633 SAMPLES=987948`, the checksums `tests/gb_bench_expected.txt` holds. The display build is 499,116 bytes of text and ends with a new line, `TinyGBC GB DISPLAY FB=<crc> PACE=<0|1>`, which on the AE350 also carries `SHOWN`, `SHOW`, `LATE`, `PIXELS` and `BITS`; FB is the checksum of the converted destination, so it covers what the video unit fetches and can be compared between the PC and the AE350 although only the AE350 has a buffer to write. On the PC two runs of the display build both ended `FB=d6e86438` with the core's checksums unchanged, which is recorded in `tests/gb_bench_expected.txt`, and `PACE=1` with 60 frames took 1.00 s, one emulated frame per 16.7 ms. Both AE350 builds compile with `-Wall -Wextra -Werror`. The three open nits were folded in: README now quotes seed 4's current clocks, 92.26 MHz on the bus and 101.05 on the pixel clock, `tools/build_ae350_soc.sh` says the 80.46 and 89.06 figures belong to the imported design, and `firmware/include/tinygbc.h` no longer cites a `tests/test_loader_host.py` that this tree does not have. Nothing was deployed: the user asked for deployment to wait for permission, so the board still runs the entry 6 core, which is also this cycle's core because no RTL changed. The video simulation was not re-run for that reason. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 6 were not rewritten, and validated this entry as number 7 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Deployment is the open step and waits on the user's permission: `tools/deploy_tinygbc.sh` then `tools/load_ae350.py --tinytang` with the display build, after which the picture is watched with `PACE=1` and measured with `DISPLAY=1` alone. The first handover is expected to be invisible because of the two-frame settle, and a black rather than a wrong-coloured picture would point at the buffer or the writeback instead of at the conversion. After that, the two channels still shaped for the NES are the core's 32,768 Hz sound against the DAC's 48,828.125 Hz, which is a resampler question in the harness rather than an I2S divisor one, and the Wii pad mapped onto gambatte's input.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc.h
- tests/gb_bench_expected.txt
- tools/build_ae350_soc.sh

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 8 COMMIT Unreleased 2026-10-22T07:08:53-07:00

#### Coming From:

Unreleased f2c8652

#### Purpose:

Deploy the display build entry 7 produced, find out why the picture it drew was stripes rather than the game, and fix the fetch path that caused it.

#### Outcome:

Deploying entry 7's image and running the display build reproduced the player's picture data exactly, `FB=d6e86438` against the PC's, while the video unit reported `PIXELS=288000` and `LATE=52354` and the capture card showed 5-row white bands on black: the unit was drawing `0x7fff` on alternate source lines, not the game. The cause was `fpga/ae350_soc/ahb_sdram.sv`'s video read loop: `step` was seven bits and `S_VREAD` stopped at 127, so a line fetch read 128 of its 160 halfwords and `vdone`, which waits for index 159, never fired. `fd` therefore never followed `fr_req`, the fetch never completed again, and because `fr_req` keeps toggling the late detector fired on every other line, exactly the 72 per frame measured. Entry 4 widened `vindex` and moved the `vdone` comparison to 159 but left the loop at 127, and nothing could catch it: `video_gb_tb.sv` models the vstart/vdata channel rather than instantiating the controller, so no test ever ran `ahb_sdram`. The fix is an eight-bit `step`, a nine-bit column `vcol` that starts at `{op_addr[9:8], 7'd0}` and runs to 159, and the loop ending there; a line starts on a 256-word boundary, so its 160 words never cross the row. `gb_bench` also stopped writing a buffer whose handover was still waiting for its blank, which could tear the frame the unit had begun fetching. The missing test now exists: `fpga/ae350_soc/tb/sdram_video_tb.sv` and `tools/sim_sdram_video.sh` instantiate the controller with a behavioural SDRAM and check 160 halfwords, their indexes, their data, one `vdone` and a CPU read through the line buffer; it passes on the fixed loop and, when the loop is put back to 127, fails with 128 halfwords and no `vdone`, so the check is known to bite. With seed 4 the rebuild places at 82.99 MHz on the bus and 96.65 on the pixel clock, ten video block RAMs each on one clock and the pixel clock through SPINE16 and SPINE25, and packs to a 4,666,860-byte `.bin` with SHA-256 `0fcad2cc881c2f14e31f9142e05a6bb908e9ff68ea3f121110fb95d63c94f4e0` and a `.fs` with `37438e8f7e97fa3d6f1393cb0b892960348a3402eba8c5b857be8c0c8090c879`. After `tangload` reported `ID=0001081b` and `core loaded`, the paced run gave `SHOWN=1663 SHOW=2546 VF=1810 LATE=0 PIXELS=541475 BITS=7fff PIXELS0=0` and the unpaced one `SHOWN=552 VF=551 LATE=0 PIXELS=541400`, both with `FB=d6e86438` and the core's own checksums unchanged at 404.8 and 405 percent. The picture was then read from the capture card rather than judged by eye: frames from the fourth on show the uCity map view, 21,863 lit source pixels against the PC frame 1799's 22,031, matching that frame best among the last twenty with a six-pixel horizontal offset that belongs to the capture path, and a saturated red that stays red at (255,118,0) and a blue that stays blue at (0,0,255), so the red-in-4:0 packing is not swapped and the residual tone difference is the card's conversion. `tools/sim_video_gb.sh` still passes its eleven checks. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 7 were not rewritten, and validated this entry as number 8 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The user should confirm the picture on the monitor, which is the one thing the capture path cannot settle. After that the port's remaining NES-shaped channels are the audio rate, gambatte's 32,768 Hz against the DAC's 48,828.125 Hz, and the Wii pad onto gambatte's input; the capture card's six-pixel offset and the overlay behaviour after a `tangload` are worth a look when the picture is next chased.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- fpga/ae350_soc/ahb_sdram.sv
- fpga/ae350_soc/tb/sdram_video_tb.sv
- tools/sim_sdram_video.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 9 COMMIT Unreleased 2026-10-22T07:14:25-07:00

#### Coming From:

Unreleased d73f707

#### Purpose:

Measure the board playing Oracle of Seasons from the savestate taken on the previous evening, with a scripted walk moving the character and the picture shown on HDMI.

#### Outcome:

The board was rebooted, so `tools/deploy_tinygbc.sh` re-loaded entry 8's image (`0fcad2cc`, `ID=0001081b`, `core loaded`), and `gb_bench` was built with `STATE=build/firmware/apps/gb_play/gb_play.state`, `INPUT` set to `tests/zelda_walk.txt`, `FRAMES=3600` and `DISPLAY=1 PACE=1`. The cartridge is Oracle of Seasons, 1,048,576 bytes, CRC-32 `d7e9f5d7`, CGB; the program is 1,478,376 bytes of text. A PC run of the same build gave `VIDEO=278bd312 AUDIO=45ac9983 SAMPLES=1975050` and `FB=c48a9cea`, and the board reproduced all four, so the walk it played is the walk the PC played. On the A25 the savestate loaded in 76.7 ms, and the 3,600 frames averaged 5,030 us each, minimum 4,372 and maximum 5,890, with no frame over the 16,742.7 us period: 332.8 percent of the Game Boy's rate with the character walking, against 330.6 percent for entry 3's walk from its own state. The video unit handed over 3,297 frames across 3,619 display frames, 60.3 seconds of play at 60 Hz, at 2,527 us a handover, with `LATE=0`, `PIXELS=500650` for the last whole frame and `BITS=7fff`. The state on disk is not the one entry 3 recorded: it is the same 59,629 bytes but hashes `c0c54bcea6757d48e8bf9568c54f77732dc382570c8d54490e6ed9af5ec6df49` rather than `49d4a697`, because the play build writes its state on exit as well as on F5, so the file is a slightly later frame of the same overworld. That the scripted walk reaches the game was checked on the PC rather than assumed: frames 300 and 600 of this state differ in 16,739 source pixels, and 600 and 1500 in 313. No frames were captured from the capture card this cycle: it left the bus when HDMI was moved back to the monitor, so the picture was seen by the user rather than measured. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 8 were not rewritten, and validated this entry as number 9 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The user's confirmation of the picture and the walk on the monitor is what remains of this cycle. The next work is the two channels still shaped for the NES: the core's 32,768 Hz sound against the DAC's 48,828.125 Hz, which is a resampler choice in the harness, and the Wii pad mapped onto gambatte's input, which would let the game be played rather than scripted.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 10 COMMIT Unreleased 2026-10-22T07:26:57-07:00

#### Coming From:

Unreleased 2a586b3

#### Purpose:

Play the Game Boy's sound on the board, which the harness had never fed to the DAC at all.

#### Outcome:

`make AUDIO=1` gives `gb_bench` an audio path: gambatte's 32,768 Hz sample pairs are resampled to the 48,828.125 Hz both sinks run at (AUD-001, DEV-020) and written into the I2S FIFO, which is libretro's arrangement of the job, the core reporting its rate and the frontend converting to the sink's. The ratio is exactly 0.67108864 input samples per output sample, held as a Q32 phase step (`2882303762`), and each output pair is interpolated linearly between the two input pairs either side of it; the core's own `AUDIO` checksum is untouched, so the PC and the AE350 still compare. Two corrections came out of running it. The level check first sat after the frame's 817 pairs had been pushed, where the FIFO always looked healthy while it emptied during the next frame, and the emulator's pacing is a hair slower than the DAC's clock so it does empty: priming to 1,600 against a 2,048-deep FIFO then overflowed on the first push, and what works is a prime of 1,100, a reserve kept near 1,500 by repeating the newest pair two at a time when it falls short, and a report of every pair so repeated. With the state, the walk, `DISPLAY=1 PACE=1 AUDIO=1` and 3,600 frames, the board played `SENT=2945483 PAIRS=2943053 FIX=2223 DROPS=0 UNDERRUNS=0 LEVEL=885 US=158`: no underrun and no drop over 60.3 seconds, 2,223 repeated pairs out of 2.94 million (0.08 percent), and 158 us a frame of resampler cost. The emulation was untouched by all of it, `AVG=5054` against entry 9's 5,030 with `OVER=0` and 331.2 percent, and `VIDEO=278bd312 AUDIO=45ac9983 FB=c48a9cea LATE=0 PIXELS=500650 BITS=7fff` equal to the PC and to entry 9. An unpaced run produces sound three times too fast for the DAC and drops by design, which the unit's drop counter will show. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 9 were not rewritten, and validated this entry as number 10 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The user has to hear it: the Pmod I2S2 and HDMI should both carry the game at the right pitch, and the first thing to expect if not is a rate that is right but a channel swapped, since the resampler interpolates each channel independently. After the ears, the last NES-shaped piece is the Wii pad mapped onto gambatte's input, which would let the game be played rather than scripted, and the audio could then be judged under play rather than under a fixed walk.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 11 COMMIT Unreleased 2026-10-22T07:33:16-07:00

#### Coming From:

Unreleased 8703c22

#### Purpose:

Record the user's acceptance of the picture and the sound, which entry 10 could not carry because it was already committed when the listening happened.

#### Outcome:

The demo was played again on the board through `tools/load_ae350.py --tinytang` with the build entry 10 left, and it reproduced entry 10's figures: `SENT=2945480 PAIRS=2943053 FIX=2216 DROPS=0 UNDERRUNS=0 LEVEL=881 US=158` with `LATE=0 PIXELS=500650 FB=c48a9cea`, ending `TinyGBC EXIT 0`. Listening to that run on both outputs, the user reported the sound perfect on the Pmod I2S2 and on HDMI, and passed the cycle. Shown the frames captured from the board earlier in the day, the user had also accepted the picture. The user test is therefore recorded as passed for the picture that entries 7 and 8 produced and for the sound entry 10 produced, which closes the acceptance that those three entries carried as not run; entry 9's walk was not judged separately, but the picture and sound it produced come from the same path. Nothing was built or changed in this cycle. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 10 were not rewritten, and validated this entry as number 11 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The last channel still shaped for the NES is the pad: mapping the Wii Classic Controller, already read by the fabric on PMOD0 and used by TinyNES, onto gambatte's input, so the game can be played rather than scripted. That would also let the picture and the sound be judged under play. After it, the port has a picture, a sound and an input, and what remains belongs to the SoC rather than the machine: the inherited desk layer and link mode, and whether the audio's 0.08 percent of repeated pairs is worth removing.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 12 COMMIT Unreleased 2026-10-22T07:43:20-07:00

#### Coming From:

Unreleased f05d1fc

#### Purpose:

Record what Adafruit's Wii breakout adapter does in the pad path, before the pad cycle is built on it.

#### Outcome:

The reference library gained WII-002, with a fast-index line and a routing row: Adafruit's Wii Nunchuck Breakout Adapter (4836) takes VIN as both supply and logic reference, so it is fed the host's logic voltage, gives a regulated 3.3 V output good for 100 mA, level shifts SCL and SDA for 3 to 5 V logic with a 10 kOhm pull-up on each, and keeps the controller's fixed 0x52 address; a Classic Controller uses the same connector and protocol as a Nunchuck. The record also settles the CYBERDECK question the user raised: the Cyberdeck is a Raspberry Pi HAT whose STEMMA ports are on the Pi's SDA and SCL nets, and the 40-pin header it presents must not be mated to this board's 2x20 headers, which are the SDRAM connectors of BRD-008. On the board side the adapter needs nothing from the fabric: this tree's SoC already reads four I2C pairs on PMOD0, SCL on Digilent pins 1 to 4 and SDA on 7 to 10, each LVCMOS33 with the pin pulled up, and its outputs only pull low; the adapter's own 10 kOhm pull-ups sit on the host side. The user reported connecting the adapter to a PMOD socket with a Classic Controller and running TinyNES, whose fabric reads the same pads through the same pins, with the controller working, so the path is confirmed on this hardware even though this tree's own core has not read it yet. No engineering file changed. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 11 were not rewritten, and validated this entry as number 12 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The pad cycle is firmware only, because the fabric already speaks the controller's protocol and the adapter changes nothing electrically: `gb_bench` polls PMOD0's first bus, answers gambatte's input from it with the scripted table kept as the reproducible fallback, and adds an AE350 `PLAY=1` build that runs until HOME and SELECT are held, keeping the picture paced and the sound playing so the game can be played rather than scripted. The first check on hardware is the pad's own identity register, since that is what says the controller was found through the adapter.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---

## 13 COMMIT Unreleased 2026-10-22T09:42:37-07:00

#### Coming From:

Unreleased dd378bb

#### Purpose:

Give the port a real pad, so the game is played with a Wii Classic Controller rather than a scripted table.

#### Outcome:

The controller first answered on PMOD0 pair 1 through Adafruit's adapter, `PRESENT|CLASSIC` with identity `00 00 A4 20` and no failed transfers, which proved the adapter path. Moving it to the Cyberdeck's 40-pin header then failed at the board: with that header mated to the empty SDRAM connector and its 3.3 V rail fed from a PMOD, the board would not power up at all and recovered as soon as the HAT was lifted, so those header pins are not inert I/O; block RAM and the SDRAM byte lanes and address lines then passed, and BRD-008 was revised with the observation. The pad moved instead to USB-A port 2, whose data pair is two free LVCMOS33 pins: `ae350_soc.sv` gained `usb_sda` and `usb_scl`, `wii_pads` bus 0 moved onto D+ (H13) and D- (G13), and buses 1 to 3 stayed on PMOD0's remaining pairs, so PMOD0's first pair is no longer read. The design packs to a 4,762,092-byte `.bin` with SHA-256 `6f3ed57f1064630e94555a957fbd8dc779fcd6c501b418155d310d07d2afb8bf`, at 87.44 MHz on the bus and 103.82 on the pixel clock, with the AE350 hop, the ten single-clock video block RAMs and the SPINE16+SPINE25 pixel clock unchanged. The adapter is fed from VBUS at 5 V with no added parts: its 10K pull-ups divide against the dock's 15K USB host pull-downs, so the pin sees about 3.0 V against a 2.31 V input threshold, and the first probe on that wiring returned `PRESENT|CLASSIC` with 25,124 reads and no failures. The button decode was then corrected the hard way: the WiiBrew table this first build followed is wrong for this controller, and the user's play test showed R moving the character down, A up and Y left. `PADTRACE` was added to print every change of the pad's raw report, the user pressed one button at a time in a stated order, and the measured layout is byte 4 bit 1 R, 2 Plus, 3 Home, 4 Minus, 5 L, 6 Down, 7 Right and byte 5 bit 0 Up, 1 Left, 2 ZR, 3 X, 4 A, 5 Y, 6 B, 7 ZL, all active low, recorded as WII-003; the analog travel of L and R sets no bit, only their click. `gb_bench` gained `PAD=1`, `PADTRACE=1` and `FRAMES=0`, the last meaning run until HOME and Minus are held, and the play build with `DISPLAY=1 PACE=1 AUDIO=1 PAD=1 FRAMES=0` was loaded and played: the session ended with 3,459,165 audio pairs, 0 drops and 0 underruns, `LATE=0`, `PIXELS=500275 BITS=7fff`, 3,873 frames handed over in 4,250 display frames and 344 percent of full speed, and the user passed the pad. The scripted benchmark build is unaffected, its checksums unchanged, because the pad decode only replaces the table's input when a controller answers. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 12 were not rewritten, and validated this entry as number 13 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The left stick is still undecoded: its bytes 0 and 1 move in the trace, but neither axis has been mapped or given a deadzone, and doing it needs one more guided trace with the stick held in each direction. After that the port has a picture, a sound and a pad, and what is left belongs to the machine rather than the emulator: a savestate write path on the A25, so a play session can be resumed, and the inherited desk layer and link mode that still sit over the picture.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- fpga/ae350_soc/ae350_soc.cst
- fpga/ae350_soc/ae350_soc.sv

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 14 COMMIT Unreleased 2026-10-22T11:28:43-07:00

#### Coming From:

Unreleased e2a00b6

#### Purpose:

Find out why the pad went silent when the cable was rebuilt, and make the next such fault a measurement instead of a guess.

#### Outcome:

The silence was two faults and one mistake of mine. The mistake: a build started to swap SDA and SCL was interrupted at the tool level but finished anyway, replacing `build/ae350_soc/ae350_soc.bin` with a bitstream whose pins were crossed, while the source had already been reverted; every deploy after that loaded `2ec691d3`, so a new cable, a new adapter, the other USB port and the old hardware in a control test all failed identically, and it was found only by comparing the hash the deploy reported against the hash the source builds. Rebuilt from the reverted source the image came back byte-identical to the one that had worked, `6f3ed57f`, which is its own confirmation. The pad still failed with it, and the fault there was the old cable: its data pair was open. To stop guessing, `wii_pads` gained a register at offset 0x008, `TN_PAD_LINES`, that reads the four buses' pad levels, SDA in bits 7:4 and SCL in 3:0; its first build trapped with an AHB error because the new address was decoded in the read path but not in the access-validity term, and with `is_lines` added to that term it reads `ff` on an idle PMOD bus and `00` on the USB pair while nothing powered is on it, where the dock's 15K pull-downs hold both lines low. With a cable whose green went to the adapter's SDA and white to its SCL, the pair read `ff`, the probe returned `PRESENT|CLASSIC` with identity `00 00 A4 20` and 14,494 reads without a failure, and the play build then ran with the pad, the picture and the sound; the user reported every button working, which closes the mapping entry 13 measured. Two lessons are worth carrying: a tool-level interruption does not stop a build, so the deployed image's hash is checked against the source that claims to produce it before any conclusion is drawn from the hardware, and a bus with no pull-ups is distinguishable from a bus that is merely not answering, which is what the line-level register makes visible. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 13 were not rewritten, and validated this entry as number 14 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The left stick is still the one input not decoded: its bytes 0 and 1 move in the trace but no axis has been mapped or given a deadzone, and one guided trace with the stick held in each direction settles it. Then the port has a picture, a sound and a complete pad, and what is left belongs to the machine: a savestate write path on the A25 so a session resumes rather than restarts, and the inherited desk layer and link mode that still sit over the picture.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc.h
- fpga/ae350_soc/wii_pads.sv

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 15 COMMIT Unreleased 2026-10-22T14:42:29-07:00

#### Coming From:

Unreleased db87238

#### Purpose:

Find and fix the horizontal wrap the user saw once the picture was on a monitor: the leftmost 32 columns of every line repeating its rightmost 32, visible only where tiles animate, as water does.

#### Outcome:

The wrap was a seven-bit wire. `ae350_soc.sv` carried the SDRAM fetch's halfword index between `ahb_sdram` and `video` on `wire [6:0] vid_index`, so indices 128 to 159 arrived as 0 to 31 and the last 32 halfwords of every line overwrote the first 32, while the data itself was correct. Neither testbench could see it: `video_gb_tb` instantiates the video unit and drives `vindex` itself, and `sdram_video_tb` instantiates the controller, so the wire between them was in neither. The measurement that found it was taken on the board: `make LBTEST=1` fills the frame buffer with a pattern that names its own pixel, shows it, reads the line buffer back through `TN_VIDEO_LB`, and two new debug registers at `0xe8010020` and `0xe8010024` report the halfword count and the first and last `(index, data)` pair of the last fetch. Before the fix that read `LBDEBUG=a0000000 LBDEBUG2=00009f1f` - 160 words, first pair index 0 data 0, last pair index 31 data 159 - which named the fault as the index rather than the data, after the buffer had been split into two 256-entry arrays and the controller's index moved into a packed shift register, neither of which changed the symptom. Widening the wire to eight bits then cost timing: with an honest eight-bit index the tool could no longer halve the line buffer, and all eight seeds of a fresh sweep failed at placement, 42 to 53 MHz on the bus. Trimming the buffers from 256 entries to the 160 a line actually uses fixed the pixel clock on every seed, 76.71 to 92.29 MHz against 74.25, and after routing seven of the eight cleared the bus too, seed 9 best at 83.86 MHz bus and 98.35 pixel. Seed 9 was built and deployed as `fe24481837968d2f44c8ccf3016883f39955456a7ea6cebfd3e957cfdde62fed`, and the same LBTEST then read `LBDEBUG2=00009f9f` - last pair index 159, data 159 - with `LB0=0`, `LB32=32`, `LB127=127`, `LB128=128` and `LB159=159`; `first_bad=6` is one entry still holding game data, which the pattern write did not reach and which the picture did not show. The play build with the savestate, sound and pad then ran on the board and the user passed it. `video_gb_tb` gained four checks at the picture's left edge (source columns 0, 1, 31 and 32) and now checks fifteen things with no failure; `sdram_video_tb` still passes. The lesson worth carrying: a tool that narrows an index to fit what it can prove only makes the design fast by making it wrong, and a signal that crosses between two modules needs a testbench that instantiates both. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and that entries 1 to 14 were not rewritten, and validated this entry as number 15 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The line buffer is now 160 entries per parity and closes timing at seed 9, so the seed default in `tools/build_ae350_soc.sh` should move from 4 to 9 when this cycle's image is kept. Left is the left stick, whose bytes 0 and 1 move in the pad trace but no axis has been mapped or deadzoned, and a savestate write path on the A25 so a session resumes rather than restarts.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc.h
- fpga/ae350_soc/ae350_soc.sv
- fpga/ae350_soc/ahb_sdram.sv
- fpga/ae350_soc/tb/video_gb_tb.sv
- fpga/ae350_soc/video.sv

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 16 COMMIT Unreleased 2026-10-22T15:23:04-07:00

#### Coming From:

Unreleased ebf76c4

#### Purpose:

Start a cartridge from a window on TinyTang's desktop, the way TinyNES does, by making the player accept a cartridge over the UART and giving the desk launcher a library page that sends one.

#### Outcome:

TinyGBC's player can now be started by a window on TinyTang's desktop. The player had never linked as a cartridge player: `firmware/boot/load.h` declared `tn_load_reset` and `tn_load_poll` without `extern "C"`, so main.cpp's C++ translation unit referenced mangled names and the link failed on both; the header now carries the same guards `uart.h` does. A new `CARD=1` option in `firmware/apps/gb_bench/Makefile` builds the single artifact the launcher needs, implying `CART=1 DISPLAY=1 PACE=1 AUDIO=1 PAD=1 FRAMES=0`, and names it `tinygbc-play.elf` under `build/firmware/apps/gb_card/`: 393,752 bytes in one LOAD segment at `0x40000000`, with the heap reaching `0x41fc0000`, so the two-megabyte cartridge buffer that `receive_cart` mallocs fits above the program and below the frame buffers. `tools/deploy_tinygbc.sh` now puts that file at `/root/ae350/tinygbc-play.elf` beside the core image, and still verifies a repacked `.fs` before it deploys anything. On the TinyTang side, edited in place with the user's permission and therefore not in this repository, the desk launcher turned out to recognise only `TinyNES LOAD`, `TinyNES WANT`, `TinyNES LINK` and `TinyNES EXIT`, while this tree's boot ROM and player say `TinyGBC`, so the handshake would have stopped at its first line; `ports/bl616/tang_fpga_uart.c` there gained `ae350_says()`, which matches either product name and hands back the fields past it, and the five call sites use it, with the NVRAM and settings requests left TinyNES-only until the player has saves. The window itself is `ports/bl616/tinygbc_app.cpp` there: a Library page listing the card's `.gbc` and `.gb` files from the first of `/root/media/gb`, `/root/media/gbc`, `/root/media/gbcolor` and `/root/media` that exists, launching through `tang_core_load` and `tang_ae350_run` and putting `desktop.bin` back afterwards, with Input, Video and Saves as labelled placeholders; zip archives are not listed because gambatte has no archive reader, unlike TinyNES's cores. The firmware built clean as `tinytang 0b0685a-dirty.45da5d7`, which `platform` reports after the flash and the power cycle, and `tools/deploy_tinygbc.sh` put the seed-9 image `fe24481837968d2f44c8ccf3016883f39955456a7ea6cebfd3e957cfdde62fed` and the player on the card, with `ucity.gbc` copied to `/root/media/gb` as the test cartridge. The user launched the game from the window, played it, and came back to the console, so the core load, the cartridge handover, the link and the exit all work; the default seed in `tools/build_ae350_soc.sh` moved from 4 to 9 as entry 15 asked, and README gained a section on the launcher. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 15 were not rewritten, and validated this entry as number 16 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The window's Input, Video and Saves pages are placeholders because the player has nothing behind them: the next work is a battery save and a savestate on the A25, asked for and sent back over the same `WANT` channel the cartridge uses, so the library can keep a `.sav` beside each `.gbc` and `TinyGBC NVRAM WANT` and the settings blob can join the four lines the launcher already understands. The left stick still moves in the pad trace with no axis mapped or deadzoned, and the TinyTang edits are uncommitted in that repository, where the user's own Phosphor changes are also in progress.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/boot/load.h
- tools/build_ae350_soc.sh
- tools/deploy_tinygbc.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 17 COMMIT Unreleased 2026-10-22T16:06:20-07:00

#### Coming From:

Unreleased c1fdb5b

#### Purpose:

Let a cartridge arrive as a .zip, the way TinyNES takes one, so the launcher's library can hold archives as well as bare images.

#### Outcome:

gambatte's copy of libretro-common is trimmed to what the core draws on and has no inflate, so the decoder was vendored: `encodings/encoding_deflate.c`, `include/encodings/deflate.h` and `include/retro_endianness.h` from libretro-common (MIT, RetroArch team, headers intact), taken from `libretro/libretro-fceumm` at `ac0d00a7fc01166f84c7bac01d1084ebfd2b865e` -- the same pin TinyNES's `tools/fetch_fceumm.sh` uses for the same job -- under `firmware/third_party/libretro-common/`, whose README records that provenance and why `retro_inline.h` and `retro_common_api.h` are deliberately not duplicated: gambatte's fetched copies are already in the include path and supply the macros these two files need. On top of it, `firmware/common/cart_zip.c` and its header are TinyNES's reader adapted to this cartridge: the central directory is read rather than the local header, one entry ending `.gb` or `.gbc` in any case and any folder, stored or deflated, other entries ignored, two cartridges refused, and the declared size and CRC-32 both checked against what came out. `receive_cart()` now inflates an archive, frees it, and hands gambatte the buffer of its own; `CART_MAX` went from two to four megabytes so the largest Game Boy Color image and the archive holding it each have room, and the CART build's text went from 371,532 to 378,780 bytes, 400,504 of them sent as one segment at `0x40000000`. `tools/test_cart_zip.sh` builds the reader for the PC with `-Werror` and the address and undefined-behaviour sanitizers and runs it over archives it makes itself from `third_party/roms/ucity.gbc`: five accepted (deflated, stored, `.gb`, `.GB`, and a nested entry beside an ignored `.txt`) and four refused (two cartridges, no cartridge, a flipped CRC-32 in the central directory, and an archive cut short), all as expected and with no sanitizer report. The three zips in the user's folder were checked by hand against Python's `zipfile`: all three inflate byte for byte, 1,048,576 bytes each, which is evidence for the cycle rather than a committed fixture. TinyTang's TinyGBC window now lists `.zip` as well (that repository, commit `14f27e9`), and its Library sends the archive untouched. The player was deployed to `/root/ae350/tinygbc-play.elf` (3,214,996 bytes, SHA-256 `520945950f42e8024fd4c70da854d7054f67e9492900af10603b8c1cb081e714`) beside the seed-9 core `fe244818...`, `zelda-ladx.zip` (500,878 bytes) was put in `/root/media/gb`, firmware `6b080b4-dirty.4c13dc7` (648,960 bytes, SHA-256 `ede0f0732eca09a44b6e5d38b127c7cf978b69a2a15a1555b73aa5da501d8a06`) was flashed and the board power-cycled, and the user launched the zip from the window and passed. README's launcher section now covers the archive, its Licensing section names the one vendored decoder, and the placement paragraph was corrected from seed 4 to seed 9, which entry 16 changed without the prose following. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 16 were not rewritten, and validated this entry as number 17 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The whole archive is held in RAM while it is unpacked, so a zip larger than four megabytes, or one holding a ROM that large, is refused rather than streamed; that is the limit to raise first if an eight-megabyte cartridge ever appears. The window's Input, Video and Saves pages still wait on a battery save and a savestate on the A25, which would also let `TinyGBC NVRAM WANT` and the settings blob join the four lines the deck answers in both product names. The left stick still moves in the pad trace with no axis mapped or deadzoned.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp
- firmware/common/cart_zip.c
- firmware/common/cart_zip.h
- firmware/third_party/libretro-common/README.md
- firmware/third_party/libretro-common/encodings/encoding_deflate.c
- firmware/third_party/libretro-common/include/encodings/deflate.h
- firmware/third_party/libretro-common/include/retro_endianness.h
- tools/test_cart_zip.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 18 COMMIT Unreleased 2026-10-22T16:58:12-07:00

#### Coming From:

Unreleased 1e62100

#### Purpose:

Build the Game Boy library the card's TinyGBC folder needs in the shape the NES library already has, and keep the tool that does it.

#### Outcome:

`tools/make_gb_library.py` builds the tree from MiSTer's `games/Gameboy` and `games/GBC` folders in the shape the card's `/root/media/nes` has, which is the shape TinyDesk's Files app imposes (TDESK-017: 64 entries a folder with `..` included, 47 bytes a name): one folder per first character of the title, region tags gone from the names, at most 61 files a folder, and a letter that outgrows that cut into balanced folders labelled by the prefixes they hold, which is how the NES library's B became `Ba` and `Be-Bz` and its S `Sa-Sp` and `Sq-Sz`. The two sources hold 600 and 543 zips; two Game Boy titles had a USA and a Japan release apiece and the USA one was kept, so the sets are 598 and 543 titles, and 29 titles exist in both sets as different games (Aladdin, Wario Land II and 27 more), which are both kept: the Color one keeps the plain title and the monochrome one gets ` (GB)`. The result is `/home/vash/Documents/gbc`, built by `tools/make_gb_library.py --to` and outside this repository: 1,141 cartridges in 38 folders, the largest 61 files (D), every name at most 47 bytes, 289.7 MB. 47 titles were over the limit with their tags gone and each is shortened by hand in the script's `RENAMES` table, cut at a word boundary where that keeps the game's own name and at the middle where the tail is what tells one game from another (`Simpsons, The - Miniature Golf Madness`, `TMNT II - Back from the Sewers`, `E.T. - Escape from Planet Earth`). A rebuild into an empty folder compares identical to the built tree. Every one of the 1,141 archives was put through the player's own reader on the PC: all were accepted, every one inflated to an image carrying the Game Boy header logo, and each output file's SHA-256 matches exactly one source zip, 598 from the Game Boy folder and 543 from the GBC one. No hardware was involved, so the build, deployment and user-test stages do not apply to this cycle; the user copies the tree to the card's `/root/media/gb`, where the TinyGBC window's Library already browses folders. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 17 were not rewritten, and validated this entry as number 18 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The library has to be copied to the card's `/root/media/gb` and looked at in the TinyGBC window before anything about it is settled: 38 folders at the root, a `..` on every page below it, and the ` (GB)` pairs are the things to check by eye. Adding cartridges later is the same command with the same sources and a fresh destination, and a title that then needs shortening is one more line in `RENAMES`; the tool refuses to write over an existing folder so that a half-built library is never mixed with a new one.

#### Files Modified:

- tools/make_gb_library.py

#### Status:

- Build: N/A
- Deployment: N/A
- User Test: N/A

---
