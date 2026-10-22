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
