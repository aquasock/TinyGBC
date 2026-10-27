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

## 19 COMMIT Unreleased 2026-10-22T17:12:44-07:00

#### Coming From:

Unreleased e8b90ca

#### Purpose:

Record the user's acceptance of the Game Boy library on the card, which entry 18 left as an untested PC-side product.

#### Outcome:

The user copied `/home/vash/Documents/gbc`, the 1,141-cartridge tree entry 18 built, to the card's `/root/media/gb` and reported PASS for the cycle, so the folders, the `..` pages and the 29 ` (GB)` pairs beside their Color releases read as intended in the TinyGBC window's Library page. This supersedes entry 18's `User Test: N/A`: nothing in the library or the tool changed, and no engineering file was touched, only the result it could not record when it was written. The board was off the USB console when this entry was written, so the card was not read back by the agent and the file counts recorded here are the ones entry 18 verified on the PC. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 18 were not rewritten, and validated this entry as number 19 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The library is settled and the standing work is unchanged: a battery save and a savestate on the A25 so the window's Input, Video and Saves pages have something to set, and the left stick, which still moves in the pad trace with no axis mapped or deadzoned.

#### Files Modified:

None.

#### Status:

- Build: N/A
- Deployment: PASS
- User Test: PASS

---

## 20 COMMIT Unreleased 2026-10-22T21:34:13-07:00

#### Coming From:

Unreleased 9a5701f

#### Purpose:

Give TinyGBC one master volume and a mute for HDMI and the I2S2 together, set from a new Audio tab in TinyTang's TinyGBC window and applied by the player, matching what TinyNES gained in the same cycle.

#### Outcome:

HDMI and the I2S2 play the one sample stream (`i2s_out` hands the transmitter the pair it shifts to the DAC, DEV-020), so the volume is a gain in the player and the core, its seed and the deployed image `fe244818...` are unchanged. `firmware/include/tinygbc.h` gained the same `tn_volume_gain` and `tn_volume_apply` as TinyNES's `tinynes.h`: a gain out of 256 that is the square of the percentage's fraction, exactly 256 at 100 percent and 0 when muted, checked on the PC by TinyNES's `tests/test_volume.sh` against the identical functions. The player had no settings channel, so `firmware/apps/gb_bench/main.cpp`'s CART build now prints `TinyGBC SETTINGS WANT <address> <bytes>` after the game loads and before link mode, takes a two-byte `HostSettings`, the volume in percent and the mute, straight into memory in the loader's protocol, waiting about a second and keeping 100 and 0 when nothing comes, then prints `TinyGBC GB VOLUME=<percent>`. In the AUDIO build `audio_resample` scales each interpolated pair by `audio_gain` before the FIFO and keeps the scaled pair for the level top-up. TinyTang's half is its entry 20: a new `/gbc.ini` with an `[audio]` section, `tang_ae350_run` answering `TinyGBC SETTINGS WANT` from it, and the window's Audio tab with a slider in 5 percent steps, a percentage, a mute and Save, which creates the file on a card that has none. `make CARD=1` built `tinygbc-play.elf`, 3,216,816 bytes, SHA-256 `62f75aa9c41f5019ba7c9996df115077dff27dec3f06dd9bba4bcf3a23f7448b`, put at `/root/ae350/tinygbc-play.elf` with TinyTang's `tinytang_put.py`; the core was not redeployed. The user tested the tab, the volume and the mute on HDMI and the Line Out and reported PASS. The board was off USB when this entry was written, so `/gbc.ini` and the player's `VOLUME` line were not read back. README's launcher section describes the request and the gain. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 19 were not rewritten, and validated this entry as number 20 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The volume is taken when a game starts, so a change made over a running game waits for the next one; applying it at once would need a fabric path from the BL616 to the player during play. The settings channel now exists, so Input and Video can join the blob when the player has something to set, and the standing work is unchanged: a battery save and a savestate on the A25 for the Saves page, and the left stick, which still has no axis mapped or deadzoned.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc.h

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 21 COMMIT Unreleased 2026-10-22T22:39:29-07:00

#### Coming From:

Unreleased 745f1ad

#### Purpose:

Fix the wrap the user found at the left edge of the picture once a game core is loaded, about seven pixels of the line's far end shown at its start, which TinyGBC shares with TinyNES.

#### Outcome:

`fpga/ae350_soc/video.sv`, imported from TinyNES, delayed the transmitter's `cx`/`cy` by four clocks before its four-stage colour pipeline, while hdl-util's `hdmi.sv` takes the colour for `cx` one clock after it presents `cx` (PROT-009), so every pixel reached the transmitter seven clocks late and the line's last seven pixels, colour bars and desk layer alike, showed at its left edge; TinyNES's entry 51 traces it from the user's screenshots and confirms TinyTang's `desktop.bin` already leads by four and is unaffected. This is a different fault from entry 15's seven-bit index. The unit now registers (`cx`, `cy`) advanced by four, wrapping into the next line and frame, which also centres the Game Boy picture that sat seven pixels right. `fpga/ae350_soc/tb/video_gb_tb.sv` had modelled the same eight-clock delay and so passed the fault; it now files each RGB under the pixel `cx` named one clock earlier, and `tools/sim_video_gb.sh` passes 15 checks with the fix while the old unit fails 12 of them, and a look-ahead of three or of five fails two, so the testbench pins the value. An eight-seed sweep with the pixel clock routed first (`build/e21_lead`) left four seeds clean, all without a hold violation, and four short of 75 MHz on `bus_clk` at 66.31 to 73.60; seed 4, the best bus margin at 78.98 MHz with `pix_clk` 92.00, is the new default in `tools/build_ae350_soc.sh`, and `TINYGBC_SEED=4` rebuilt it through every gate (the AE350 hop present, 10 video block RAMs each on one clock, the pixel clock on SPINE16+SPINE25) to `ae350_soc.fs` `6b4fba0f70394628c9a4dc56a82d28d56931660ce17d7bbe683d32720bb9cd71`, the sweep's seed-4 image byte for byte, and `ae350_soc.bin` `b17ed09a5900f7d0c36aadf9bce33ca320fcd84b0dd657a6a3b801f65554397d`. `tools/deploy_tinygbc.sh` put it at `/root/cores/console138k/tinygbc.bin` with the unchanged player and loaded it, and the desktop core was loaded again afterwards. The user deferred the test to a later session, so this entry is a handoff with the board test not run. README's seed paragraph and PROT-009's verification were revised. The core-syntax audit re-read `core.md` and `core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `core.md` was not changed and entries 1 to 20 were not rewritten, and validated this entry as number 21 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The user test is outstanding: a game started from the TinyGBC window with the colour bars beginning at column 0, the desktop over the game with a clean left edge, the picture centred, and sound and the Audio tab's volume as before. If it passes, seed 4 stands as the tested default; if not, seeds 3, 7 and 8 were also clean in `build/e21_lead`. The standing work is unchanged: a battery save and a savestate on the A25 for the Saves page, and the left stick.

#### Files Modified:

- README.md
- fpga/ae350_soc/tb/video_gb_tb.sv
- fpga/ae350_soc/video.sv
- tools/build_ae350_soc.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 22 COMMIT Unreleased 2026-10-23T14:46:34-07:00

#### Coming From:

Unreleased 77e58b1

#### Purpose:

Carry TinyNES's data-format-3 change into TinyGBC's Wii pad unit so 8BitDo's Retro Receiver for the NES/SNES Classic works as controller 1, as the user asked, handed off before deployment because the user's session was ending.

#### Outcome:

TinyNES's entry 52 found that the receiver, plugged into Adafruit's adapter on USB-A, identifies as a Classic Controller (`00 00 a4 20 01 01`) but sends only data format 3, eight bytes with the buttons in bytes 6 and 7, so a six-byte read takes its triggers as every button held, and that a Classic Controller switches to format 3 when `0x03` is written to `0xfe` (TinyNES record WII-004). TinyGBC's `fpga/ae350_soc/wii_pads.sv` differed from TinyNES's only in two comment lines, so TinyNES's change was applied as a patch: the wake-up writes `0xfe = 0x03` and waits 20 ms, the identity is read as before, eight data bytes are read and packed into format 1's six so DATA, DATA2, `pad0_held` and the player keep their layout, and a clock-divider restart now lets a transfer in flight finish rather than leave a controller mid-byte. TinyGBC had no bench for the unit; TinyNES's `tests/wii_pads_tb.sv`, which models a Classic Controller that sends format 1 until told format 3 and the receiver, was added unchanged and passed (`WII PADS PASS polls=5,53 wakes=3,2`), and it failed on each of four planted faults: no `0xfe` write, a six-byte read, buttons taken from the trigger bytes and a restart that cuts a transfer short. The README describes the read and the bench. An eight-seed sweep (`build/e22_fmt3`) was clean on seven: seed 2 failed `bus_clk` at 74.23 MHz against 75; seeds 1, 3, 4, 5, 6, 7 and 8 passed with no hold violation, `bus_clk` 75.47 to 84.38 MHz and `pix_clk` 89.17 to 95.73 MHz, the default seed 4 at 80.44 and 92.28. The user asked for the best seed; seed 6, 84.38 and 89.17 MHz, has the most `bus_clk` margin, and `TINYGBC_SEED=6 tools/build_ae350_soc.sh` was started and left running when the session ended, so its gates and image are not recorded here. Nothing was deployed: the card rebuilt in TinyTang's entry 21 holds no TinyGBC core, player or cartridge. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed `.ai/core.md` was not changed and no settled entry was rewritten, and validated this entry as number 22 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Check the seed-6 build's log for its gates (the AE350 hop, the video block RAMs, the pixel clock bridges) and that `build/ae350_soc/ae350_soc.fs` matches `build/e22_fmt3/normal/seed6/design.fs`; if it fails, seed 8 (83.34 and 95.49 MHz) is the next best. Then deploy at a TinyTang shell prompt with TinyTang's tools: the image as `/root/cores/console138k/tinygbc.bin`, `build/firmware/apps/gb_card/tinygbc-play.elf` as `/root/ae350/tinygbc-play.elf`, and the cartridge `/home/vash/Desktop/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc` under `/root/media/gb/` (create the folders first). The user tests from the TinyGBC window with the Classic Controller in the adapter and with a Wii Remote and Classic Controller through the receiver; entry 21's open checks (colour bars at column 0, the desktop's left edge, the centred picture, sound and volume) can run in the same session. If it passes, make seed 6 the default in `tools/build_ae350_soc.sh` with its reason, and add a WII record for format 3 and the receiver, citing TinyNES's WII-004. TinyGBC's `.ai/core.md` Purpose names the project TinyNES; it is restricted, so only the user can have it corrected. The standing work remains a battery save and a savestate on the A25 for the Saves page, and the left stick.

#### Files Modified:

- README.md
- fpga/ae350_soc/wii_pads.sv
- tests/wii_pads_tb.sv

#### Status:

- Build: NOT RUN
- Deployment: NOT RUN
- User Test: NOT RUN

---

## 23 COMMIT Unreleased 2026-10-23T15:26:52-07:00

#### Coming From:

Unreleased 9954633

#### Purpose:

Finish the handoff entry 22 left, by proving the seed-6 build through its gates, deploying it with the player and cartridge, and passing the board test the user then ran.

#### Outcome:

The seed-6 build that entry 22 left running finished after that entry was written, and its gates were checked against the artifacts rather than assumed: `build/ae350_soc/ae350_soc.fs` is byte-identical to `build/e22_fmt3/normal/seed6/design.fs` (SHA-256 `1f1b64e583b5970e4b696fba0897b382ccf4105d9e3fde8c4ad7cac611170707`), the AE350 dedicated PLL clock hop is present in `core_clk`, the AHB bus reached 84.38 MHz and the HDMI pixel clock 89.17 MHz against 74.25, nextpnr logged `routing 'pix_clk' first`, the ten video block RAMs are each on one clock and in 9-bit mode, and the pixel clock takes SPINE16 and SPINE25, the pair this board is known to run. `ae350_soc.bin` repacks from that `.fs` at SHA-256 `78d01d47f026decbabc203805ea5fdfbf845db370dedc2d0518b765a7035ca61`, 4,666,860 bytes. `tools/deploy_tinygbc.sh` put it at `/root/cores/console138k/tinygbc.bin` and the player, SHA-256 `62f75aa9c41f5019ba7c9996df115077dff27dec3f06dd9bba4bcf3a23f7448b` at 3,216,816 bytes, at `/root/ae350/tinygbc-play.elf`, and `tangload` reported `core loaded`; the cartridge was put at `/root/media/gb`, whose folder was created, and the desktop core was loaded again afterwards so `/tang.ini`'s `i2s2_oled` declared `0xc0 = 0x0010`. The board reported `tinytang 966da0d-dirty.1704dab`, entry 25 of TinyTang's log. The first window listed no games: the cartridge's 61-character name is at or over `LIB_NAME_BYTES` (56) in TinyTang's `ports/bl616/tinygbc_app.cpp`, and `lib_listable()` drops such an entry, leaving only `0 shown, more not listed`; a 29-byte name, `Zelda - Oracle of Seasons.gbc`, was put on the card and the long one removed, and the game then listed. That limit and its near-silent handling are TinyTang's, not this repository's, and were reported to the user. The user reported PASS for the cycle, naming both the Classic Controller in Adafruit's adapter and a Wii Remote through 8BitDo's Retro Receiver as working, so entry 21's deferred board checks were accepted in the same pass. Seed 6 is the default in `tools/build_ae350_soc.sh` with its reason, the README's seed paragraph follows it, and `.ai/core-reference.md` gained WII-004 for data format 3 and the receiver, carried from TinyNES's WII-004 and confirmed here by `tests/wii_pads_tb.sv` and the user's play. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 22 were not rewritten, checked that the reference's six YAML blocks parse with the index and records naming the same 182 ids in the same order, which needed DEV-020's record moved inside the block after DEV-019, since it had been left outside the fence and the index and the block disagreed already at the previous commit, and validated this entry as number 23 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The standing work is unchanged: a battery save and a savestate on the A25 so the Saves page has something to show, the left stick, and the Input and Video pages, which stay placeholders until the player serves them. In TinyTang, the TinyGBC window hides a cartridge whose name reaches 56 bytes with only a `more not listed` hint; naming that limit in the status line is a one-string change, while raising the cap would cost about 40 bytes per entry across 256 entries, which that board's free heap does not have. No board work remains for this cycle.

#### Files Modified:

- README.md
- tools/build_ae350_soc.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 24 COMMIT Unreleased 2026-10-26T22:30:15-07:00

#### Coming From:

Unreleased 7c21769

#### Purpose:

Keep a battery-backed cartridge's save on the card, so a game started from the TinyGBC window resumes where it was saved rather than from a blank file.

#### Outcome:

TinyTang turned out to need nothing: its `ae350run` already answers `NVRAM WANT` and stores `NVRAM SAVE` hex lines for any program that names itself `TinyGBC`, mapping that name to `/root/saves/gbc/<cartridge>.sav` and creating the folder, so the cycle is the player alone. `firmware/apps/gb_bench/main.cpp`'s CART build takes gambatte's save RAM from `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` once the game has loaded, prints `TinyGBC NVRAM WANT <address> <bytes>` before the settings request and takes the card's file straight into that buffer in the loader's protocol, waiting about a second when there is none, and prints `TinyGBC NVRAM GOT` with the CRC-32 of what landed. When HOME and MINUS end the run, a save RAM whose CRC-32 differs from the one it held as the game began goes back in TinyNES's form, three `BEGIN` lines, `TinyGBC NVRAM SAVE <bytes> <crc>`, hex 32 bytes a line paced at about 4 ms each, and `TinyGBC NVRAM DONE`; an unchanged one prints `TinyGBC NVRAM SAME` and is not sent, so a run that never saved, or whose save failed to arrive and was never written, leaves the card's copy alone. A cartridge with no battery has no save RAM and nothing is asked for. The PC build gained `GB_SAVE=<file>`, which loads a file of the save's size before the first frame and writes the save RAM back at the end, printing nothing when unset; the uCity default run still ends `VIDEO=aaddaa41 AUDIO=a36b5633` as `tests/gb_bench_expected.txt` holds. With it, the save RAM taken from the user's overworld savestate (`c0c54bce...`), 8,192 bytes with CRC-32 `145107fd`, was loaded into a fresh boot of Oracle of Seasons under `tests/zelda_input.txt`: where the run without it loops on name entry, the run with it picks file 1 and is in the overworld by frame 1,500, 776 of 800 sampled frames differ, and the save came back byte-identical, the case the player reports as `SAME`. The real-time clock of MBC3 and HuC3 cartridges (`RETRO_MEMORY_RTC`) is not kept, because `time()` on the A25 is the core's cycle counter and counts from reset rather than giving the date. The same check showed that `tests/gb_bench_expected.txt`'s Oracle of Seasons boot-script figure is stale: the source before this cycle gives `VIDEO=95513058` where the file holds `900c74b5`, with `AUDIO=0630f3c5` and 13,167,010 samples unchanged. `make CARD=1` built `tinygbc-play.elf`, 3,221,912 bytes, SHA-256 `20fe58d8a0f995cc97adbf70b77c2a53ad7e77b0c90f63acaf0ddc9e1bb4084b`, and TinyTang's `tinytang_put.py` put it at `/root/ae350/tinygbc-play.elf` on a board running `tinytang 416ba34-dirty.a98ea86`; the seed-6 core `78d01d47...` was not reloaded. The user saved in Link's Awakening DX, left with HOME and MINUS, relaunched, found the save restored and passed the cycle. Read back afterwards, `/root/saves/gbc/` holds `Legend of Zelda, The - Link's Awakening DX.sav` at 32,768 bytes, the cartridge's whole RAM; TinyTang's `/root/saves/last.log` could not confirm the CRC because the board's `cat` returned only its first 3,962 of 22,613 bytes and the shell has no `tail` or `grep`. README's launcher section describes the save, and its settings file is corrected from `/gbc.ini` to `/tinygbc.ini`, which TinyTang renamed in its entry 27. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 23 were not rewritten, and validated this entry as number 24 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Savestates come next, one per cartridge on the card as TinyNES's entries 48 and 49 have them and on the same buttons; TinyTang already routes `TinyGBC STATE` lines to `/root/states/gbc/`. After that the left stick, the Input and Video pages, and the cartridge clock, which needs the date from the BL616 before a saved RTC means anything. `tests/gb_bench_expected.txt`'s Oracle of Seasons boot figure should be re-recorded once the reason for its change is known. In TinyTang, the comment at `ports/bl616/tang_fpga_uart.c` that calls the NVRAM and settings requests TinyNES-only is out of date, and `last.log` cannot be read past its first four kilobytes from the shell. `.ai/core.md`'s Purpose still names the project TinyNES and only the user can have it corrected.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 25 COMMIT Unreleased 2026-10-26T23:26:04-07:00

#### Coming From:

Unreleased 9410e53

#### Purpose:

Give the Game Boy player save states the way TinyNES has them, one per cartridge saved with HOME and L and loaded with HOME and R and kept on the card, with the TinyGBC window's Saves page showing them and the battery save.

#### Outcome:

`firmware/apps/gb_bench/main.cpp`'s CART build keeps one slot sized by `retro_serialize_size()`, a 32-byte header in TinyNES's `TNS1` layout with the magic `TGS1` (the cartridge's CRC-32, the first 32 bits of gambatte's commit, `d9d6cd06`, which the Makefile takes from `tools/fetch_gambatte.sh` as `GB_GAMBATTE_COMMIT`, a patch field of 0, the frame, the length and the state's CRC-32) before gambatte's own state. HOME with L saves and HOME with R loads, once a press and not while the desktop is shown, `desk_link.sv` being byte-identical to TinyNES's so neither is a HOME tap; R refuses a state naming another cartridge or failing its CRC. A save is copied to a send buffer and goes out in the background in TinyNES's form, two `TinyGBC STATE BEGIN` lines, `TinyGBC STATE DATA <bytes> <crc32>` and the file as bytes through `tn_put_char`, 256 a frame, while the F= report and any state line wait so the host never counts them as payload, and a send still going at exit finishes before the summary. At launch the player prints `TinyGBC STATE WANT` after the battery save and takes the card's file into the slot without loading it. The cartridge's RAM is inside gambatte's state, so after R the battery save written at exit is the state's, TinyNES's rule. The benchmark gained `STATETEST=<n>`. Its first form compared a replay from the state with a run that never loaded and found the picture identical at every point but the sound different in every frame after the load; a temporary per-frame trace showed why, gambatte's `retro_unserialize` resetting its audio resampler and output buffer by design, which swaps the 549 and 548-sample framing and restarts the filter history. The test now loads its own state at the save point so both timelines start from that reset, then checks that a state from another cartridge and one with a byte flipped are refused. It matched picture and sound at eight points on the PC, uCity at frames 300, 900 and 1,500 (182,509-byte state, its 128 KiB cartridge RAM included) and Oracle of Seasons from boot at 900, 6,000 and 18,000 and from the overworld state at 300 and 1,800 (59,629 bytes), and on the A25, with the seed-6 core `tinygbc.bin` loaded by `tangload` and the desktop core put back afterwards, uCity at 900 and the overworld at 1,800 matched with the PC's checksums, a save taking 13.6 and 5.0 ms and a load 8.9 and 3.2 ms. The uCity default run still ends `VIDEO=aaddaa41 AUDIO=a36b5633`. TinyTang's transport needed nothing; its TinyGBC window's Saves page was ported from TinyNES's in TinyTang's entry 47, built as `a879a92-dirty.3c5e535` and flashed with `tinytang_flash.py`. `make CARD=1` built `tinygbc-play.elf`, 3,239,232 bytes, SHA-256 `08bb91582e368e2ec2f62f94bf1b90cc11151b9d2724af7fb81279b683fb36d8`, put at `/root/ae350/tinygbc-play.elf`. After a power cycle the user tested saving, loading, a state across a relaunch and the Saves page, and passed the cycle. Read back afterwards, `platform` reported `a879a92-dirty.3c5e535`, and `/root/states/gbc/` held `Pokemon Pinball (USA, Australia) (Rumble Version).state` at 59,661 bytes, its 59,629-byte state and the header, with that cartridge's 8,192-byte `.sav` beside entry 24's Link's Awakening DX save. README describes the states, and `.ai/core-reference.md` gained EMU-009 for gambatte's state, its contents, its size and the reset outside it, with an index line and a routing row, the EMU topic's description widened from the NES to both cores and `last_reviewed` moved to 2026-10-26; its six YAML blocks parse and the index and records name the same 183 ids in the same order. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 24 were not rewritten, and validated this entry as number 25 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The clock of MBC3 and HuC3 cartridges, for Pokemon Gold, Silver and Crystal among others, is next as the user set out: gambatte keeps it as an 8-byte `baseTime` against `std::time(0)` (EMU-009), and on the A25 `time()` counts from reset, so the player needs the date from the BL616's RTC at launch and a place to keep `baseTime` beside the battery save. A state's send takes about four seconds for a 60 KB state and twelve for uCity's, and a lost send leaves the previous card file in place with only TinyTang's log saying so, as in TinyNES. The left stick, the Input and Video pages and the stale Oracle of Seasons boot figure in `tests/gb_bench_expected.txt` stand.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 26 COMMIT Unreleased 2026-10-27T00:39:58-07:00

#### Coming From:

Unreleased 08a3525

#### Purpose:

Keep the clock of MBC3 and HuC3 cartridges, Pokemon Gold, Silver and Crystal among them, so a game's clock moves on with real time between sessions as a cartridge's battery would keep it.

#### Outcome:

gambatte d9d6cd06 counts an MBC3 or HuC3 clock as `std::time(0)` less an 8-byte `baseTime` (`libgambatte/src/mem/rtc.cpp`, `huc3.cpp`), which libretro exposes as `RETRO_MEMORY_RTC` and RetroArch keeps as a `.rtc` file beside the save; on the A25 `time()` was the cycle counter from reset, so a kept `baseTime` would have meant nothing across sessions. `firmware/rt/syscalls.c` gained `tn_rt_set_date`, declared with C linkage in `firmware/include/tinygbc.h`, which sets an offset from 1970 to the counter's zero so `time()` runs on from the date it is given and stays the counter alone until then. In `firmware/apps/gb_bench/main.cpp` the settings blob grew from two bytes to sixteen, the volume and the mute, six reserved bytes and the host's clock as UTC seconds, with a static assertion on its size; a launcher that sends two bytes leaves the date 0. The battery save's transfer became a `CardFile` per channel, `receive_file` and `send_file` replacing `receive_save` and `send_save` with the same lines under the channel's name, and a second `CardFile` carries the clock as the `RTC` channel. With a date the player calls `tn_rt_set_date`, asks with `TinyGBC RTC WANT` for the `.rtc` straight into gambatte's `baseTime`, sets a cartridge with none to the present so its clock starts at zero, and at HOME and MINUS sends the clock after the battery save when it differs from what it held at launch; without one nothing is read or written, and `TinyGBC GB DATE=<utc> RTC=<bytes>` reports which. `<ctime>` is now included for both builds. The uCity default run still ends `VIDEO=aaddaa41 AUDIO=a36b5633` and its `STATETEST=900` still matches. TinyTang's half is its entry 48: an `RTC` channel to `/root/saves/gbc/<cartridge>.rtc`, the clock in the TinyGBC settings reply and a clock line on the Saves page, flashed as `b10ae68-dirty.886b72a`. A scan of the user's library found seven cartridges with a clock, types 0x10 and 0xfe: E.T. - Digital Companion, Harvest Moon GBC, Mary-Kate and Ashley - Pocket Planner, Pokemon Crystal, Gold and Silver, and Robopon - Sun Version. The player the user tested, `tinygbc-play.elf` of 3,240,816 bytes, SHA-256 `f23f385db2bc31be7d752afeb77f9ae04b9464fea13d8e762ce0e605cda70f51`, also carries entry 27's mono mix; the clock alone was first deployed as `b312287c697792ee6d8771653c80bf80191571f15d61699891a957b0c67bc44a`. One launch from the console with `ae350run`, recorded over the console, reported `clock not set` and `DATE=0`, then stopped relaying after 450 frames, froze at exit and needed a power cycle with its save lost; the user identified holding the console during a run as a known cause of a failed core unload, and that run is not counted. Launched from the TinyGBC window, the user set Pokemon Gold's clock, saved in the game and left with HOME and MINUS, and the Saves page then read the battery save written whole at 00:31 with 32,768 bytes and the clock written whole at 00:31 with 8 bytes, both stamped with the date; the user then relaunched after a wait, found the save and an in-game clock that had moved from 10:07 to 10:09, and passed the cycle. The card could not be listed afterwards because the console was running the desktop. README describes the clock. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 25 were not rewritten, and validated this entry as number 26 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The one recorded launch found the board's clock unset after a replug, so the player can be started without a date and then keeps no clock for that session, silently; the TinyGBC window could say so, or TinyTang could sync the RTC before answering. Frame pacing is next: the player paces itself at 59.7275 Hz against HDMI's 60 Hz, and a recording of Jayro's Test Cart showed bursts of repeated and skipped frames about every 3.5 s, the beat of the two rates, which the user also saw at Zelda's screen transitions; locking a frame to each vertical blank, as TinyNES does, with the resampler's step moved to 0.6741504 to match, is the proposed fix. The left stick, the Input and Video pages and the stale Oracle of Seasons boot figure in `tests/gb_bench_expected.txt` stand.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc.h
- firmware/rt/syscalls.c

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 27 COMMIT Unreleased 2026-10-27T00:40:34-07:00

#### Coming From:

Unreleased 42ee84b

#### Purpose:

Mix the Game Boy's stereo sound to mono, as the user chose, so a sound the game pans to one side is heard on both rather than only on that side.

#### Outcome:

The user reported the sound as only the left channel, on HDMI and, they believed, on the Pmod I2S2, and recalled that the port had kept one channel to stay simple. The path was traced and measured before anything changed: over 3,000 frames of Oracle of Seasons, uCity 1.3, Jayro's Test Cart v3.0.1 and RetroRegen GBC Test v0.1.6, a scratch harness linked against the PC build's gambatte objects found the two channels at equal RMS levels with a correlation of 1.0000, so gambatte produces stereo; the player packed left into bits 31:16 of `TN_AUDIO_DATA` and right into 15:0, `i2s_out.sv` sends 31:16 with LRCK low and 15:0 with it high, `hdmi_silent.sv` maps them to words 0 and 1, and both files are byte-identical to TinyNES's, whose player writes the same sample to both halves and is heard on both sides, so the right channel's path works and what reached one side only was sound the game panned there. The user chose a mono mix over stereo for now. In `firmware/apps/gb_bench/main.cpp` the resampler takes (L + R) / 2 from each of gambatte's pairs, keeps one previous and one current sample instead of two of each, and writes the volume-scaled result to both halves of every FIFO word, including the level top-up's repeated pair. The average cannot clip, and a sound panned hard to one side comes out at half its level. The core's own `AUDIO` checksum is taken before the mix, so the uCity default run still ends `VIDEO=aaddaa41 AUDIO=a36b5633`. The player was built clean and deployed as entry 26's `f23f385db2bc31be7d752afeb77f9ae04b9464fea13d8e762ce0e605cda70f51`, and the user played Pokemon Gold with it in entry 26's test, but did not report on the mix itself, so the user test is not claimed. README states the mix. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 26 were not rewritten, and validated this entry as number 27 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The user should confirm in RetroRegen GBC Test's Sound Test that the left, right and both tones all come out of both sides, on HDMI and the I2S2. Stereo would be a player change only, since the fabric already carries two channels. Entry 26's next steps stand, frame pacing first.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: NOT RUN

---

## 28 COMMIT Unreleased 2026-10-27T07:29:34-07:00

#### Coming From:

Unreleased f6a1c7e

#### Purpose:

Remove the stutter the user saw in Jayro's Test Cart's tearing test and at Zelda's screen transitions by pacing the player to the HDMI picture as TinyNES does, and record the user's pass of entry 27's mono mix.

#### Outcome:

The user tested entry 27's mono mix after it was committed and reported that left and right sound the same, and the sound slightly louder, which is expected: a sound the game panned to one side now reaches both sides at half its level; entry 27's user test, recorded as not run, is therefore passed here. The stutter was measured before it was changed: in the user's 27-second 60 fps HDMI capture of Jayro's Test Cart v3.0.1's tearing test, the per-frame scroll of the steady 2-pixel vertical pan ran clean for a few seconds and then repeated and skipped frames for about 70 frames, in bursts about 3.5 s apart, 64 of 375 frames off pace. The player paced itself at the Game Boy's 59.7275 Hz while the picture runs at exactly 60 Hz, 74.25 MHz over 1,650 by 750, and the two beat every 1 / (60 - 59.7275) = 3.67 s; near each alignment a frame finished just after a blank and the handover rule held it, so one frame showed twice and the next was lost. No other Game Boy emulator is installed on this PC for a comparison, so the reference used was gambatte itself through scratch harnesses built from the PC objects. In `firmware/apps/gb_bench/main.cpp` the AE350's `GB_PACE` now waits after each handover until `TN_VIDEO_STATUS` names the new buffer, which `video.sv` sets as vertical blank begins, and waits for the next blank alone when nothing was handed over, so one emulated frame runs per 60 Hz blank and each is shown once; the game runs 0.46 percent fast. `MISSED` counts blanks a frame was not ready for, the first frame excepted, and the PC keeps its timer pacing. With the picture's and the bus's clocks both from the 50 MHz crystal, the resampler's step became 0.6741504 exactly, 32,917.5 over 48,828.125, as Q32 `2895453921`. Locking the rates removed the slow drain the FIFO's top-up had relied on, and the first A25 run, uCity 1.3 for 1,800 frames, showed `MISSED=0`, `SHOWN=1800`, `VF=1800` and the PC's checksums but `DROPS=324`: the level after each push sat near 1,900 of 2,048 pairs and up to 3 ms of jitter in when a frame ends overflowed it. The level after each push is now held between 1,200 and 1,500 by repeating the newest pair or leaving pairs out of the next frame, a quarter of the distance outside the band each frame, between 2 and 64 pairs, the FIFO is primed with 900 pairs instead of 1,100, and the report gained `SKIP`. Intermediate settings gave 5 underruns with a 500-pair prime, and 136 drops with a fixed two-pair correction while a high start took 300 frames to come down; the final build gave `DROPS=0 UNDERRUNS=0 MISSED=0` in three successive 30 s runs and in a 3-minute run of 10,800 frames, with `SHOWN` equal to the frames each time and the PC's checksums, `VIDEO=aaddaa41 AUDIO=a36b5633` for 1,800 frames. `make CARD=1` built `tinygbc-play.elf`, 3,242,996 bytes, SHA-256 `de8157a01aed57d75c6abc61327e5aab78d567d15d39112532b1610009389021`, which the committed tree rebuilds byte for byte, and it was put at `/root/ae350/tinygbc-play.elf` with the desktop core restored. The user's second capture found the bursts gone and a single held frame about once a second in the 1-pixel pans, 65 frames apart at frames 204, 269, 432 and 497, each a step of 0 followed by normal steps, and a run of five in the 2-pixel pan. The same test driven on the PC (Start at frame 900, A for Screen, Down twice and A for Tearing/Ghosting, A to begin), measured on gambatte's own frames, holds a frame every 65 frames in the 1-pixel pan and five in a row in the 2-pixel pan, so those are the cartridge's own output, which states it was made in GB Studio, and the board now reproduces the emulator. Walking Link from the overworld state across a screen edge on the PC gave no dupe in 2,400 frames, and over 3,000 frames of four cartridges gambatte dupes rarely and irregularly. The user then passed everything: sound perfect and Zelda's transitions perfect. README describes the pacing, the step and the band, and the Makefile's `PACE` comment matches. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 27 were not rewritten, and validated this entry as number 28 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Nothing is outstanding from this cycle. The board's clock can still be unset at a launch after a replug, which leaves that session without a cartridge clock and says nothing, as entry 26 recorded. The left stick, the Input and Video pages and the stale Oracle of Seasons boot figure in `tests/gb_bench_expected.txt` stand.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/Makefile
- firmware/apps/gb_bench/main.cpp

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 29 COMMIT Unreleased 2026-10-27T08:01:15-07:00

#### Coming From:

Unreleased b71ccca

#### Purpose:

Establish whether `tests/gb_bench_expected.txt`'s Oracle of Seasons boot line was invalidated by a later change or was never true of the committed tree, so the file is not relied on for a figure it cannot support.

#### Outcome:

The line was never true of the committed tree, and the tree's other baselines are sound. Before anything was concluded the two baselines that cover the picture path were re-run and reproduced: the uCity default build ends `VIDEO=aaddaa41 AUDIO=a36b5633 SAMPLES=987948` over 1,800 frames, and the `DISPLAY=1` build ends `FB=d6e86438` with the core's own checksums unchanged, which is the number an AE350 run is held against for the frame buffer the video unit fetches. The Oracle of Seasons boot run does not reproduce: `FRAMES=24000` with `tests/zelda_input.txt` embedded gives `VIDEO=95513058 AUDIO=0630f3c5 SAMPLES=13167010` where the file holds `VIDEO=900c74b5`, with the sound checksum and the sample count equal to the file's. Entry 24 saw the mismatch and left the cause open; it is settled here by rebuilding `d21c72a`, the commit that recorded the figure, in a scratch worktree with the fetched core symlinked in, where the same command gives `95513058` twice, so the discrepancy dates from the recording and not from any later cycle. Everything that could have made that run different was checked against what the file states: the cartridge hashes `862a5136...`, the fetched gambatte is at the pinned `d9d6cd06...` and clean, and `tests/zelda_input.txt` matches entry 2's description at 647 presses from frame 600 to 19,980, alternating every 60 frames with 3-frame holds. The one explanation that would fit a picture-only difference, a core built without `-DVIDEO_RGB565`, was tested and fails loudly rather than quietly: the harness grants only RGB565, so such a build is refused at load with `TinyGBC GB LOAD FAILED ROM=1048576` instead of producing a different checksum. What remains is that `900c74b5` came from a working state during entry 2's cycle that was not the one committed, most likely an earlier form of the input script, which would move the game onto different screens while the menus are silent; that is the leading explanation and not a proven one. Nothing in the tree reads the file, `gb_bench_expected` appearing only in this log, so no test could catch the mismatch and it survived four cycles. The A25 side of that line is unanchored for the same reason, and the walk baseline is unverifiable by construction because it is pinned to savestate `49d4a697...`, which entry 9 recorded the play build as having replaced with `c0c54bce...` and which lives in the gitignored `build/`. The file therefore keeps two baselines that work and two that cannot do their job; by the user's instruction nothing in it was changed this cycle, and it should be re-recorded or proven when that path is next touched, because a picture checksum that moves with the sound unchanged is also the shape a real regression would take. As a side check the benches were run: `video_gb_tb` passed fifteen checks, `sdram_video_tb` six with 160 halfwords, `tests/wii_pads_tb.sv` passed, and `tools/test_cart_zip.sh` returned its five accepted and four refused archives. No board was attached, so nothing was deployed or tested on hardware. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 28 were not rewritten, and validated this entry as number 29 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and `None.` in Files Modified.

#### Next Steps:

The next cycle is input remapping, built the way TinyNES does it, so a controller's buttons can be rearranged without rebuilding the player. Standing work beside it is the left stick, whose bytes 0 and 1 the pad decode still does not map or deadzone, the silent session a launch without a date leaves on a clock cartridge, and the Input and Video pages, which are placeholders in TinyTang's `tinygbc_app.cpp` rather than files in this repository.

#### Files Modified:

None.

#### Status:

- Build: PASS
- Deployment: N/A
- User Test: N/A

---

## 30 COMMIT Unreleased 2026-10-27T09:00:29-07:00

#### Coming From:

Unreleased dc7880d

#### Purpose:

Let a controller's buttons be rearranged from the desk window, built the way TinyNES does it, so the Game Boy's buttons can be rebound without rebuilding the player.

#### Outcome:

The mapping is a table the host fills and the player applies, and the decode moved into a header so a test can reach it. `firmware/include/tinygbc_pad.h` holds the Classic Controller's fifteen measured buttons as one word in the order entry 13 measured them, the fourteen sources a Game Boy button may answer to in the order `/tinygbc.ini` lists them, and a pure `pad_decode(b4, b5, map)` that `pad_poll()` now calls in place of its fixed `if` chain; `main.cpp` asserts with `static_assert` that the joypad positions in the header still match the core's `RETRO_DEVICE_ID_JOYPAD_*`. HOME is not a source: a tap of it belongs to the desk layer, so a game button bound to it would raise the desktop on every press, and the machine's own combos (HOME with MINUS to quit, with L or R to save or load a state) still read the raw pad rather than the mapping. `HostSettings` grew from sixteen bytes to twenty-four with `pad_map[8]` appended last, one index per button in `pad_joypad[]` order, so a launcher built before the mapping sends the sixteen bytes it always did and leaves the mapping at its compiled default instead of zeroing it; the defaults reproduce the old decode exactly, which is why the two working baselines did not move, `VIDEO=aaddaa41 AUDIO=a36b5633 SAMPLES=987948` and `TinyGBC GB DISPLAY FB=d6e86438`. `tools/test_pad_map.sh` builds `tests/pad_map_tb.c` with the address and undefined-behaviour sanitizers and passes: the default map decodes all 65,536 combinations of the report's two bytes identically to the old fixed decode, all fourteen sources reach all eight buttons and nothing else, an index past the table yields no button, one source can answer to two buttons, the table has no duplicates, HOME is absent, and the three measured bit positions used as witnesses land where WII-003 put them. The check is known to bite: swapping A and B in the default map fails it on the first combination. TinyTang's half, in that repository's own log, is an `[input]` section of one `<button> = <controller>` key each in `/tinygbc.ini`, a `pad_map[]` in its settings struct, a `TinyGBC SETTINGS WANT` reply of twenty-four bytes still cut to what the player asked for, and a real Input page in place of the placeholder, modelled on TinyNES's: eight rows, a Change that steps the highlighted row to the next controller button, Restore that resets the mapping alone so the volume is untouched, and a Save that rewrites only `[input]` and `[audio]` and keeps every other byte of the file. The firmware went to the board as `0f896ca-dirty.ad50850` after one earlier build, and the player as 3,244,664 bytes with SHA-256 `40bde651750768c988aa66916cca15d99aa1fa75d293826ed66a8db5bd0a6551`; the player asked for `00000018` bytes, the reply was `ae350run: settings /tinygbc.ini, volume=20, clock not set, a=x` and the player reported `TinyGBC GB PADMAP=1,8,3,2,4,5,6,7`, so A answered to X. The user then rebound B to Y and A to B from the Input page and passed the cycle; read back with the shell's `cat`, `/tinygbc.ini` holds `b = y`, `a = b`, the four directions and `select`/`start` unchanged, `volume = 20` as it was, and the comment line above them, so Save rewrote its two sections and left the rest. Three faults were found and two were fixed. The first build's Input page drew no rows at all, because `td_list` draws nothing until `td_list_set_count()` is called and the page never called it, as TinyNES's does; the fix adds the count, and a second flash was needed. More seriously, one launch in five came up with the default mapping and the volume at 100 without saying so, and the console showed why, `TinTinyGBC SETTINGS WANT` and `TinyGBCDLINES`, the head of the request line lost on the AE350-to-BL616 wire. This project already documents that fault and has `save_pace()` for it, "the first bytes after a quiet spell are where it loses some" from TinyNES on 2026-10-14, but only the save-send path ever called it; every request the player makes now settles the line first, and the settings request is asked again if no answer lands, which is safe because a host that never saw the request is idle and a host that answered is never asked twice. Three consecutive launches then read back `PADMAP=9,1,3,2,4,5,6,7` and `VOLUME=20`, against roughly one failure in five before, and two further runs were abandoned when the board was needed elsewhere, so the fix is measured and not exhausted. The board then left the USB bus during the cleanup, with the player and the firmware deployed and the desktop core not yet reloaded, so the last thing the FPGA was given may still be the TinyGBC core. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 29 were not rewritten, and validated this entry as number 30 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

The left stick is still the one input not decoded, its bytes 0 and 1 moving in the pad trace with no axis mapped or deadzoned, and it would join the mapping as four more sources. The same loss that took the settings line can take any request: the battery save, the clock and the state file are settled before they ask but not retried, so a lost line there means a game that silently starts without its save, which is the next thing to close in that path. Entry 26's silent clock on a launch after a replug, entry 29's Oracle of Seasons baseline, and the Video page stand unchanged.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc_pad.h
- tests/pad_map_tb.c
- tools/test_pad_map.sh

#### Status:

- Build: PASS
- Deployment: PASS
- User Test: PASS

---

## 31 COMMIT Unreleased 2026-10-27T09:35:53-07:00

#### Coming From:

Unreleased 0179ec9

#### Purpose:

Give the window's Video page real settings -- the palette a monochrome cartridge is drawn in and the colour correction a colour one gets -- with the default leaving the picture this project has measured unchanged.

#### Outcome:

Aspect was dropped rather than built: `TN_VIDEO_ASPECT` exists at `0x001c` but for this machine it only chooses the picture's width, five columns for 800x720 with square pixels or six for 960x720, and `video.sv`'s own header says five by five is the picture's own aspect, so there is no honest choice to offer. The two settings that do something cannot be fabric settings at all: the video unit's palette and emphasis stores have been dead since the picture began reading the frame buffer, and libgambatte's palette setters sit behind the core's own object, so gambatte's core options are the only door. `firmware/include/tinygbc_video.h` holds six palettes -- Off, Auto, DMG, Pocket, Light and the Color's brown -- three colour corrections -- Off, Accurate and Fast -- and `gb_video_option()`, which answers exactly four keys, `gambatte_gb_colorization`, `gambatte_gb_internal_palette`, `gambatte_gbc_color_correction` and its `_mode`, and declines everything else so the core keeps its own defaults for the boot ROM, the rumble and the rest; `gb_environment` now answers `RETRO_ENVIRONMENT_GET_VARIABLE` from it. `HostSettings` stayed twenty-four bytes: the six reserved bytes became four, with the palette and the colour correction taking the two that were freed, so the clock and the mapping kept their offsets and a launcher that still sends zeros leaves the core on its own defaults. Two things had to be true for the settings to arrive at all. The core reads its options once, inside `retro_load_game`, through `check_variables(true)`, and never asks whether they changed because `GET_VARIABLE_UPDATE` is answered false, so `receive_settings()` moved ahead of `retro_set_environment` and the load, immediately after the cartridge; nothing in the blob depends on the cartridge, so the move costs nothing but the order of two statements. And the default had to be measured rather than assumed: with no option answered, libretro leaves its `colorCorrection` at zero and calls `setColorCorrection` false, so the picture every checksum in `tests/gb_bench_expected.txt` was recorded against is the **uncorrected** one, and the core's own option table saying "GBC only" never applied here because nothing was serving options at all. `GB_VIDEO=0,2` on the PC reproduced the uCity baseline `VIDEO=aaddaa41` exactly, which is what settled it, so Off is the default in both settings and Accurate and any palette are opt-ins. The PC build gained `GB_VIDEO=<palette>,<colour>` for measuring the path without the board. On uCity, a colour cartridge, no `GB_VIDEO` and `GB_VIDEO=0,0` both give `VIDEO=aaddaa41 AUDIO=a36b5633`, Accurate gives `e921f6ea`, Fast gives `8b494967`, and every palette from Auto to GBC leaves the checksum at `aaddaa41` -- inert, as the core returns before reading the option, which is what the greyed row says in the window. On Super Mario Land, a monochrome cartridge taken from the card's library, the six palettes give six different pictures, `d8835ec1`, `f03280aa`, `a0e30d10`, `1f8d9199`, `bc6f6f8b` and `f34a9d93`. `tools/test_video_options.sh` builds `tests/video_options_tb.c` with the sanitizers, checks that every index names a value and that unknown keys and out-of-range indices are refused, and then holds every key and every value the mapping can return against the core's own source, asking the mapping itself for the values so a mistyped one fails: changing `GBC - Brown` to `GBC - Brownish` fails the run. The player built as 3,247,464 bytes with SHA-256 `d198c49e855d1794a555c506d1494373a0f8d32b8b74cb2d9a67566d770d69ef`; by the user's instruction nothing was deployed, so the board still runs the entry 30 firmware and no part of this cycle has been seen on hardware. README's launcher section describes the page, the four keys and the ordering. The core-syntax audit re-read `.ai/core.md` and `.ai/core-syntax.md`, inspected the complete `.ai/` diff, confirmed that `.ai/core.md` was not changed and entries 1 to 30 were not rewritten, and validated this entry as number 31 of 100 with exactly six sections, prose in Purpose, Outcome and Next Steps, allowed Status values and only eligible paths in Files Modified.

#### Next Steps:

Nothing here has been on the board: the deployment is the next step when the user allows it, after which the six palettes and the three corrections are judged on a monitor rather than by checksum, and the palette row's greyed state can be seen for a colour cartridge. That state depends on the last launch having been the cartridge now selected, so a cartridge that has never run -- every `.zip`, which the deck cannot open to read the header -- shows the row enabled; the player's report is the only route to its type without unpacking it on the BL616. The left stick, entry 26's silent clock, entry 29's Oracle of Seasons baseline and the untried retry on the battery-save, clock and state requests all stand as before.

#### Files Modified:

- README.md
- firmware/apps/gb_bench/main.cpp
- firmware/include/tinygbc_video.h
- tests/video_options_tb.c
- tools/test_video_options.sh

#### Status:

- Build: PASS
- Deployment: NOT RUN
- User Test: NOT RUN

---
