# TinyGBC

A Game Boy Color port in the form TinyNES took: gambatte on the AE350 inside
the Tang Console 138K.

**Status: the emulator is measured and the video unit is retargeted, but
nothing has been shown on a screen yet.** `gb_bench` answers whether the A25
can run gambatte fast enough; `fpga/ae350_soc` is TinyNES's AE350 SoC brought
into this tree and changed to draw a 160x144 Game Boy frame instead of the
NES's 256x240. The A25 does not yet write frames into the frame buffer, so the
picture path is built and timed but not exercised.

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
TinyGBC GB DISPLAY FB=<crc> PACE=<0|1> [MISSED=<n>] [SHOWN=<n> SHOW=<us> VF=<n> LATE=<n> PIXELS=<n> BITS=<hex> PIXELS0=<n>]
TinyGBC GB AUDIO SENT=<n> PAIRS=<n> FIX=<n> SKIP=<n> DROPS=<n> UNDERRUNS=<n> LEVEL=<n> US=<us>
```

The core is built with `VIDEO_RGB565`, its default integer (blipper) audio
resampler at 32,768 Hz, no boot ROM, and no core options, so a PC run and an
AE350 run of the same program and cartridge produce the same `VIDEO` and
`AUDIO` checksums. On the PC, `GB_DUMP=<dir>` also writes the CHECK frames as
PPM.

`make DISPLAY=1` builds the same benchmark with a picture: each frame is
converted from the core's RGB565 into the video unit's 15-bit format, red in
bits 4:0, green 9:5 and blue 14:10, and written into the SDRAM frame buffer
the unit is not showing, which is handed over at a vertical blank. The last
line then carries `FB`, the checksum of the converted picture, so the PC and
the AE350 can be compared although only the AE350 has a buffer to write; on
the AE350 it also carries `SHOWN`, the frames handed over, `SHOW`, the average
cost of one handover, and what the video unit's registers report across the
run: `VF` display frames, `LATE` lines re-shown because their fetch was late,
`PIXELS` lit screen pixels of the last whole frame, `BITS` the pixel bits ever
seen set, and `PIXELS0` the lit count before the run started. Handover happens at most once per vertical blank, because
the harness runs at about four times the Game Boy's rate, and the picture
settles two frames after the write. `PACE=1` paces the run for watching: on
the AE350 one emulated frame for each vertical blank of the 60 Hz picture, as
TinyNES does, waiting after each handover until the video unit shows it, so
every frame is shown exactly once and the game runs 60 / 59.7275, 0.46
percent, fast; on the PC one per Game Boy frame (16,742.7 us). Pacing the
AE350 by the Game Boy's own 59.7275 Hz beat against the picture's 60 Hz every
3.67 s, repeating a frame and skipping the next through each beat. `MISSED`
counts blanks a frame was not ready for.

`make AUDIO=1` plays the sound: gambatte hands over 32,768 Hz sample pairs and
the Pmod I2S2's DAC and HDMI both run at 75 MHz / 1536 = 48,828.125 Hz, so the
harness resamples, which is libretro's arrangement: the core reports its rate
and the frontend converts to the sink's. Paced to the picture the core makes
70,224 / 128 = 548.625 pairs a frame at 60 frames a second, and 32,917.5 /
48,828.125 is exactly 0.6741504 input samples per output sample (0.67108864
unpaced), held as a Q32 phase step, and each output sample is interpolated
linearly between the two input samples either side of it. The picture's
74.25 MHz and the bus's 75 MHz come from the same 50 MHz crystal, so the two
rates are locked. The FIFO is primed with 900 pairs of silence, and the level
just after each frame's push is held between 1,200 and 1,500 of its 2,048
pairs by repeating the newest pair or leaving pairs out of the next frame, a
quarter of the distance outside the band a frame, between 2 and 64 pairs;
`FIX` and `SKIP` count those pairs, `DROPS` and `UNDERRUNS` are the unit's own
counters, and `US` is what resampling costs per emulated
frame. The core's `AUDIO` checksum is untouched, so the PC and AE350 still
compare. An unpaced run produces sound three times too fast for the DAC and
will drop by design. The sound is mixed to mono before resampling, (L + R) / 2
to both sides, as TinyNES's is: a sound the game pans to one side is heard on
both rather than on that side alone, at half its level when it is panned hard.
The I2S unit and HDMI still carry two channels, so stereo remains a player
change.

`make PAD=1` reads a Wii Classic Controller and plays with it. The fabric
polls the pad and the harness answers gambatte's input from it, falling back to
the scripted table when no controller answers. **Bus 0 is on USB-A port 2's
data pair** — D+ is SDA (ball H13) and D- is SCL (G13) — and buses 1 to 3 stay
on PMOD0's other pairs, so a controller on PMOD0's first pair is no longer
read. The controller's own buttons, measured from its reports rather than taken
from a table: byte 4 carries R, Plus, Home, Minus, L, Down and Right in bits 1
to 7, byte 5 carries Up, Left, ZR, X, A, Y, B and ZL in bits 0 to 7, all active
low. A is the Game Boy's A, B its B, Plus Start, Minus Select, and the d-pad the
d-pad. That mapping is only the default: the window's Input page, or the
`[input]` section of `/tinygbc.ini`, can point any Game Boy button at any of
the controller's fourteen measured buttons, and `tools/test_pad_map.sh` checks
the decode on the PC (`firmware/include/tinygbc_pad.h`). HOME is deliberately
not one of them: a tap of it belongs to the desk layer, so a game button bound
to it would bring the desktop up with every press, and the combos below read
the pad directly rather than through the mapping.

Each bus selects data format 3 (`0x03` to `0xfe`) after waking a controller,
reads eight bytes and packs them into format 1's six, so the registers and the
byte numbers above keep format 1's layout. That is what makes 8BitDo's Retro
Receiver for the NES/SNES Classic work as well as a Classic Controller in the
adapter: the receiver, with a Wii Remote and Classic Controller or another
Bluetooth pad paired to it, sends only format 3, and a Classic Controller
switches when told. `tests/wii_pads_tb.sv` models both, and a clock-divider
write lets a transfer in flight finish before the buses restart:

```sh
mkdir -p build/wii_pads_tb
iverilog -g2012 -s wii_pads_tb -o build/wii_pads_tb/tb \
  fpga/ae350_soc/wii_pads.sv tests/wii_pads_tb.sv
vvp build/wii_pads_tb/tb
```

`FRAMES=0` runs until **HOME and Minus** are held together, which is how a play
session ends; `PADTRACE=1` prints the raw report whenever it changes, which is
how the button bits above were measured. `TN_PAD_LINES` (`0xe8040008`, SDA in
bits 7:4 and SCL in 3:0) reads the four buses' pin levels directly: the PMOD
pairs idle high, and the USB pair idles low until a powered adapter's pull-ups
reach it, which is what tells a wiring fault from a protocol one.

`make LBTEST=1` measures the video path instead of playing: it fills the frame
buffer with a pattern that names its own pixel, shows it, and reads the video
unit's line buffer back through `TN_VIDEO_LB`, printing the first entry that
does not match. `TN_VIDEO_DEBUG` (`0xe8010020`) and `TN_VIDEO_DEBUG2`
(`0xe8010024`) report the halfword count and the first and last
`(index, data)` pair the SDRAM fetch delivered, which is how a fetch fault is
told from a display fault.

## Building and running

```
tools/fetch_gambatte.sh                 # gambatte at a pinned commit
tools/fetch_ucity.sh                    # uCity 1.3, the benchmark cartridge
cd firmware/apps/gb_bench
make ROM=../../../third_party/roms/ucity.gbc host   # build/firmware/apps/gb_bench/host/gb_bench
make ROM=../../../third_party/roms/ucity.gbc        # the AE350 program
```

`FRAMES=<n>`, `INPUT="<first>+<count>=<buttons> ..."` and `CHECK=<n>` are
build-time options, and `STATE=<file>` embeds a savestate and loads it after
the cartridge, so a run can start in the game rather than at boot.
`DISPLAY=1`, `PACE=1`, `AUDIO=1`, `PAD=1`, `PADTRACE=1` and `FRAMES=0` are described above.

To play on the PC and take a savestate by hand:

```
make ROM=<file.gbc> PLAY=1 host     # build/firmware/apps/gb_play/host/gb_bench
```

It opens an X11 window at four times scale, runs at 59.7275 Hz, and takes
arrows as the d-pad, `Z` as B, `X` as A, `Enter` as Start, `Right Shift` as
Select, `F5` to save and `Esc` to quit. It writes `gb_play.state` on a save,
on `SIGUSR1`, on a signal and on exit, and a rolling `gb_play.state.auto`
every 600 frames. The A25 program needs the AE350 SoC image loaded on the
board; its boot ROM is this tree's own (`firmware/boot`), and
`tools/deploy_tinygbc.sh` puts the image on the card and has TinyTang's
`tangload` program the FPGA with it. A program then goes over the UART with
`tools/load_ae350.py --tinytang`.

## The desk launcher

`make CARD=1` builds the player TinyTang's TinyGBC window runs: no cartridge
is built in, the launcher sends one after the player prints `TinyGBC WANT`, and
it is paced, has sound and reads the pad, so it runs until HOME and MINUS are
held together. The artifact is `build/firmware/apps/gb_card/tinygbc-play.elf`,
and `tools/deploy_tinygbc.sh` puts it on the card at
`/root/ae350/tinygbc-play.elf` beside the core image, which the window loads
from `/root/cores/console138k/tinygbc.bin`.

The window is TinyTang's (`ports/bl616/tinygbc_app.cpp` there), not this
tree's: the Library page lists the card's `.gbc`, `.gb` and `.zip` files from
`/root/media/gb`, and launching one loads the core, sends the cartridge as the
file the player asks for, and puts the desktop core back when the game exits.
Its Audio page sets one master volume and a mute for HDMI and the I2S2
together, and its Input page sets which controller button answers to each Game
Boy button; both are kept in TinyTang's `/tinygbc.ini`, under `[audio]` and
`[input]`, and both reach the player the same way: after the cartridge it
prints `TinyGBC SETTINGS WANT <address> <bytes>`, takes the volume in percent
and the mute straight into its `HostSettings`, and scales every resampled pair
by `tn_volume_gain` (`firmware/include/tinygbc.h`), a gain out of 256 that is
the square of the percentage's fraction and exactly 256 at 100 percent. The
mapping is the blob's last eight bytes, one index into `pad_source[]` per Game
Boy button in `pad_joypad[]` order, and because it is last a host that sends
fewer bytes leaves the player on its default mapping rather than zeroing it: a
launcher built before the mapping sends sixteen and a player built before the
clock asks for two, and neither disturbs the other. It prints
`TinyGBC GB VOLUME=<percent>` and `TinyGBC GB PADMAP=<eight indices>`. The Saves
page is still a placeholder until the player has something to set.

Its Video page sets the picture's palette and its colour correction, kept under
`[video]` of the same file. Neither is a fabric setting: the video unit's
palette and emphasis stores have been dead since the picture began reading the
frame buffer, and libgambatte's setters sit behind the core's own object, so
the only door into them is the core options a frontend answers. The player
answers four of those from `tinygbc_video.h` -- `gambatte_gb_colorization`,
`gambatte_gb_internal_palette`, `gambatte_gbc_color_correction` and its `_mode`
-- and declines the rest, so the core keeps its own defaults for the boot ROM,
the rumble and everything else. The settings have to arrive **before** the core
loads, because it reads its options once, in `retro_load_game`, and never asks
whether they changed; the player therefore asks for them straight after the
cartridge, ahead of `retro_set_environment`. The palette is a monochrome
game's, `Off`, `Auto`, `DMG`, `Pocket`, `Light` or `GBC`, and it does nothing on
a colour cartridge, which draws its own colours and for which the core returns
before reading the option at all -- so the window greys that row when the
cartridge it launched was a colour one. The colour correction is `Off`,
`Accurate` or `Fast`, at the Color's LCD. `Off` is the default and it is also
what an unanswered option has always meant here: libretro leaves its
correction at zero when none is given, so every checksum this project recorded
before the setting existed is the uncorrected picture, and `Accurate` is an
opt-in that changes it. `tools/test_video_options.sh` holds every key and value
the player can answer against the core's own source, and the PC build takes
`GB_VIDEO=<palette>,<colour>` so both paths can be measured without the board.

A cartridge with a battery keeps its save on the card, in TinyNES's protocol,
which TinyTang's `ae350run` answers for either product name. Once the game has
loaded the player prints `TinyGBC NVRAM WANT <address> <bytes>` for gambatte's
save RAM, and the launcher sends `/root/saves/gbc/<cartridge>.sav` straight into
it; nothing arriving within about a second means the game has no save yet. When
HOME and MINUS end the run, a save RAM that changed goes back as hex, 32 bytes a
line, between `TinyGBC NVRAM SAVE <bytes> <crc32>` and `TinyGBC NVRAM DONE`,
which the launcher checks before it replaces the file; one that did not change
prints `TinyGBC NVRAM SAME` and leaves the card's copy alone. The lines are
paced at about 4 ms each, so an 8 KiB save takes about a second to go back and
the largest, 128 KiB, about 18 seconds. Only HOME and MINUS save: a game left by
any other way keeps the save it started with.

The clock of an MBC3 or HuC3 cartridge, such as Pokemon Gold, Silver and
Crystal or Harvest Moon GBC, is kept beside the save as
`/root/saves/gbc/<cartridge>.rtc`, the 8-byte `baseTime` gambatte exposes as
`RETRO_MEMORY_RTC`, the format RetroArch keeps. gambatte counts the clock as
`time()` less `baseTime`, so `time()` has to give the date: the launcher's
settings reply carries the board's clock in UTC seconds after the volume and
the mute, sixteen bytes in all, and the runtime's `time()` runs on from it
(`tn_rt_set_date`, `firmware/rt/syscalls.c`). With a date the player asks for
the `.rtc` with `TinyGBC RTC WANT`, starts a cartridge that has none at zero,
and at HOME and MINUS sends a changed one back in the battery save's form, so
the game's clock moves on by the time the board was off, as a cartridge's
battery would keep it. Without a date, when the board's clock has never been
set or a launcher sends only two bytes, `time()` counts from reset as before
and no `.rtc` is read or written.

Save states work as TinyNES's do (TinyNES core-log entries 48 and 49): one
state per cartridge, HOME with L to save and HOME with R to load, once a press
and never while the desktop is up, and neither combination is a tap of HOME,
so the desktop does not come up. The state is gambatte's own
(`retro_serialize`), after a 32-byte header in TinyNES's layout with the magic
`TGS1`: the cartridge's CRC-32, the first 32 bits of gambatte's commit, the
frame, the length and the state's CRC-32. R refuses a state whose header names
another cartridge or whose CRC does not match. Each save also goes to the card
in the background as `/root/states/gbc/<cartridge>.state`, as two
`TinyGBC STATE BEGIN` lines, `TinyGBC STATE DATA <bytes> <crc32>` and the file
as bytes, 256 a frame, with nothing else printed until it has gone: about four
seconds for Oracle of Seasons' 59,629-byte state and twelve for uCity's
182,509, which holds its 128 KiB of cartridge RAM. At launch the player asks
for it with `TinyGBC STATE WANT` and takes it into the slot without loading it.
The cartridge's RAM is in the state, so after R the battery save written at
exit is the state's: the state wins. `make STATETEST=<n>` checks a state on
either build: it saves before frame `n` and loads it straight back, plays on,
loads it again after the run and plays the same frames over, and fails unless
picture and sound repeat exactly and both a state from another cartridge and
one with a byte changed are refused. The load in the run is deliberate:
gambatte's libretro driver resets its audio resampler and output buffer on
every load, so the sound after a load is split into frames differently from a
run that never loaded, while the machine and the picture are unchanged.

On the PC, `GB_SAVE=<file>` stands in for the card: a file of the
save's size is loaded before the first frame and the save RAM is written back
to it at the end.

A `.zip` reaches the player unchanged; the player inflates it itself, reading
the archive's central directory and checking the stored CRC-32 against what
came out, into a buffer of its own (`firmware/common/cart_zip.c`). The archive
may hold one `.gb` or `.gbc` in any case and any folder, and anything else in
it is ignored. `tools/test_cart_zip.sh` builds the reader for the PC with the
sanitizers on and runs it over archives it makes itself: stored and deflated,
both extensions, an ignored extra entry, and the four refusals -- two
cartridges, no cartridge, a wrong CRC, and an archive cut short.

## fpga/ae350_soc

The SoC is TinyNES's, imported whole: the AE350, its SDRAM and block RAM, the
UART, the desk layer, the Wii pad buses, the I2S output and the hdl-util HDMI
transmitter. The video unit is the part that is now TinyGBC's own: two 160x144
frame buffers of 15-bit RGB555 in the top 256 KiB of the SDRAM, a line at
`FB(b) + 512 * y` and a pixel at `2 * x`, each Game Boy pixel drawn five rows
tall and five columns wide for the authentic 800x720 picture, or six columns
wide for 960 at 4:3. The palette and emphasis stores remain in the register
map but the picture no longer reads them.

```
tools/build_ae350_soc.sh        # synthesise, place, route and pack
tools/build_ae350_soc.sh        # writes build/ae350_soc/ae350_soc.{fs,bin}
python3 tools/sweep.py ae350_soc --seeds 1-8
tools/deploy_tinygbc.sh         # card /root/cores/console138k/tinygbc.bin, then tangload
```

The build needs the open flow: `~/oss-cad-suite`, nextpnr-himbaechel and a
local Apicula. Placement decides whether the AHB bus reaches 75 MHz; seed 6 is
the default because, with the Wii pad unit reading data format 3, it measured
84.38 MHz on the bus and 89.17 on the HDMI pixel clock, the best bus margin of
the seven clean seeds of 1 to 8 (core-log entry 22), with the pixel clock on
SPINE16+SPINE25, the pair this board is known to run. That image is deployed
and the user passed it with both controller paths (entry 23).

## Licensing

This project's code is MIT. gambatte is GPL-2.0 and is fetched, never vendored:
`tools/fetch_gambatte.sh` pins the commit and `third_party/` is git-ignored, so
only patches to the core would be carried here. Programs built from it,
including `gb_bench`, are GPL-2.0 and stay in `build/`. The benchmark
cartridge, uCity, is GPL-3.0 and is fetched the same way.

The one third-party file carried in this tree is libretro-common's raw DEFLATE
decoder (MIT, RetroArch team): three files under
`firmware/third_party/libretro-common/` whose own licence headers are intact
and whose provenance that folder's README gives. It is here because gambatte's
copy of libretro-common is trimmed to what the core draws on and has no
inflate, and a cartridge may arrive as a `.zip`.
