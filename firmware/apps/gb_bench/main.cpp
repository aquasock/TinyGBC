// SPDX-License-Identifier: MIT
//
// gb_bench: runs the gambatte Game Boy Color emulator on a cartridge image
// built into the program, with a scripted controller, and times each emulated
// frame against the Game Boy frame period, so that gambatte's throughput on
// the A25 can be measured and checked against the same program built for the
// PC. It links gambatte's libretro driver and stands in for the frontend, so
// the core is exercised exactly as RetroArch would exercise it: the core
// reports its frame through video_refresh, its sound through
// audio_sample_batch, and reads the pad through input_state.
//
// This is a throughput harness, not a player. It does not show the picture:
// the video callback only records where the frame is, and the checksum is
// taken after retro_run has returned so that it is outside the timed call.
// The audio callback copies the frame's samples for the same reason.
//
// Report lines (identical in form on both builds):
//   TinyGBC GB CORE=gambatte ROM=<bytes> CRC=<crc32> CGB=<0|1> AV=160x144
//       FPS=<fps> HZ=<rate> START=<us>
//   TinyGBC GB F=<frame> AVG=<us> MAX=<us> VIDEO=<crc> AUDIO=<crc>
//   TinyGBC GB FRAMES=<n> AVG=<us> MIN=<us> MAX=<us> OVER=<n> SPEED=<x.y>%
//       SAMPLES=<n> VIDEO=<crc> AUDIO=<crc>
//   TinyGBC GB DISPLAY FB=<crc> PACE=<0|1> [SHOWN=<n> SHOW=<us> VF=<n>
//       LATE=<n> PIXELS=<n> BITS=<hex> PIXELS0=<n>]
//   TinyGBC GB AUDIO SENT=<n> PAIRS=<n> DROPS=<n> UNDERRUNS=<n> LEVEL=<n>
//       US=<us>
//
// The DISPLAY line appears only in a GB_DISPLAY build (make DISPLAY=1),
// which converts each frame out of the core's RGB565 into the video unit's
// 15-bit format and writes it into the frame buffer the unit is not showing,
// handing that buffer over at a vertical blank. FB is the checksum of the
// converted picture, so a PC run and an AE350 run can be held against each
// other although only the AE350 has a buffer to write; the fields after it
// exist only there, SHOWN being how many frames were handed over, SHOW the
// average cost of one handover, and LATE, PIXELS and BITS what the video
// unit's own registers report after the run. GB_PACE (make PACE=1) also
// paces the run for watching: on the AE350 one emulated frame for each
// vertical blank of the 60 Hz picture, as TinyNES's player does, and on the PC
// one for each Game Boy frame period. MISSED counts blanks that came and went
// while a frame was still being made, so a frame was shown twice.
//
// CGB is the cartridge header's CGB flag byte ($0143), which tells whether the
// image asks for a Color Game Boy; it is not a report of whether the game
// switched the CPU to double speed, which gambatte does not expose through
// libretro. AV and FPS are what the core's own av_info says. The F= lines
// cover the frames since the last one. AVG, MIN and MAX time retro_run alone.
// OVER counts frames slower than the frame period, SPEED is that period over
// the average frame time, and SAMPLES the sound frames the core produced.
// VIDEO folds in every CHECK-th frame's 160x144 pixels and AUDIO every
// frame's samples, so that the two builds can be compared.
//
// The core is GPL-2.0 (third_party/gambatte-libretro/COPYING); with a
// cartridge image built in, the program is for local use only.

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef __riscv
#include "tinygbc.h"
#include "uart.h"
#ifdef GB_CART
#include "load.h"    /* the boot ROM's loader, for the cartridge the launcher sends */
#include "cart_zip.h" /* and the zip reader, for one sent as an archive */
#endif
#endif

#include "libretro.h"

extern "C" const unsigned char gb_rom[], gb_rom_end[];
extern "C" const unsigned char gb_state[], gb_state_end[];

#ifndef GB_FRAMES
#define GB_FRAMES 1800
#endif
#ifndef GB_INPUT
#define GB_INPUT ""
#endif
#ifndef GB_CHECK
#define GB_CHECK 30
#endif

#if defined(GB_PACE) && !defined(GB_DISPLAY)
#error GB_PACE needs GB_DISPLAY
#endif

/* GB_AUDIO plays the core's sound on the AE350's I2S DAC and HDMI. It is a
 * hardware path only: a PC build keeps converting samples for the checksum. */
#if defined(GB_AUDIO) && defined(__riscv)
#define GB_AUDIO_OUT 1
#endif

/* GB_PAD prints what PMOD0's first Wii extension bus sees, which is what says
 * whether a controller is reaching the fabric at all and through which wiring.
 * The fabric polls the pad itself; this only reads its registers. */
#if defined(GB_PAD) && defined(__riscv)
#define GB_PAD_OUT 1
#endif

/* GB_LBTEST fills the frame buffer with a pattern that names its own pixel,
 * shows it, and reads the video unit's line buffer back, so the fetch's
 * column mapping can be measured rather than inferred. */
#if defined(GB_LBTEST) && defined(__riscv)
#define GB_LB_TEST 1
#endif
#if GB_FRAMES == 0 && !defined(GB_PAD_OUT)
#error GB_FRAMES=0 runs until HOME and MINUS, so it needs GB_PAD
#endif
/* GB_STATE_TEST=<n> saves a state before frame n, loads it when the run ends
 * and plays the frames after it again with the same script, so a state is
 * proved to round-trip exactly rather than assumed to. */
#if defined(GB_STATE_TEST) && (defined(GB_CART) || GB_FRAMES == 0)
#error GB_STATE_TEST replays a scripted run of a fixed number of frames
#endif

namespace {

#ifdef __riscv
const uint64_t tick_hz = TN_CORE_HZ;
uint64_t ticks() { return tn_cycles64(); }
#else
const uint64_t tick_hz = 1000000000u;
[[maybe_unused]] uint64_t ticks()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * tick_hz + (uint64_t)ts.tv_nsec;
}
#endif

[[maybe_unused]] uint64_t to_us(uint64_t t) { return t * 1000000u / tick_hz; }

uint32_t crc_table[256];

void crc_init()
{
	for (uint32_t i = 0; i < 256; i++) {
		uint32_t c = i;
		for (int k = 0; k < 8; k++)
			c = (c >> 1) ^ (c & 1 ? 0xedb88320u : 0);
		crc_table[i] = c;
	}
}

[[maybe_unused]] uint32_t crc_update(uint32_t crc, const void *data, size_t len)
{
	const uint8_t *p = static_cast<const uint8_t *>(data);
	crc = ~crc;
	while (len--)
		crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return ~crc;
}

// The scripted pad: <first frame>+<count>=<buttons>, buttons comma separated.
struct Press {
	unsigned first, count;
	uint16_t buttons;
};
Press presses[2048];
unsigned press_count;

struct Button {
	const char *name;
	unsigned id;
};
const Button buttons[] = {
	{"a", RETRO_DEVICE_ID_JOYPAD_A},
	{"b", RETRO_DEVICE_ID_JOYPAD_B},
	{"select", RETRO_DEVICE_ID_JOYPAD_SELECT},
	{"start", RETRO_DEVICE_ID_JOYPAD_START},
	{"up", RETRO_DEVICE_ID_JOYPAD_UP},
	{"down", RETRO_DEVICE_ID_JOYPAD_DOWN},
	{"left", RETRO_DEVICE_ID_JOYPAD_LEFT},
	{"right", RETRO_DEVICE_ID_JOYPAD_RIGHT},
};

[[maybe_unused]] bool parse_input(const char *s)
{
	while (*s) {
		while (*s == ' ')
			s++;
		if (!*s)
			break;
		if (press_count == sizeof presses / sizeof presses[0])
			return false;
		Press &p = presses[press_count++];
		char *end;
		p.first = (unsigned)strtoul(s, &end, 10);
		if (*end != '+')
			return false;
		p.count = (unsigned)strtoul(end + 1, &end, 10);
		if (*end != '=')
			return false;
		s = end + 1;
		p.buttons = 0;
		do {
			if (*s == ',')
				s++;
			size_t len = strcspn(s, ", ");
			bool found = false;
			for (const Button &b : buttons) {
				if (strlen(b.name) == len && !strncmp(b.name, s, len)) {
					p.buttons |= 1u << b.id;
					found = true;
				}
			}
			if (!found)
				return false;
			s += len;
		} while (*s == ',');
	}
	return true;
}

[[maybe_unused]] uint16_t pad_state(unsigned frame)
{
	uint16_t state = 0;
	for (unsigned i = 0; i < press_count; i++)
		if (frame >= presses[i].first && frame < presses[i].first + presses[i].count)
			state |= presses[i].buttons;
	return state;
}

#ifndef __riscv
/* On the PC the CHECK frames are also written as PPM, which is how the run
 * is checked to be showing what it should be; the AE350 has nowhere to put
 * them. */
const char *dump_dir;

[[maybe_unused]] void dump_frame(unsigned frame, const void *ptr, unsigned w, unsigned h, size_t pitch)
{
	if (!dump_dir)
		return;
	char path[512];
	snprintf(path, sizeof path, "%s/%06u.ppm", dump_dir, frame);
	FILE *f = fopen(path, "wb");
	if (!f)
		return;
	fprintf(f, "P6\n%u %u\n255\n", w, h);
	const uint8_t *p = static_cast<const uint8_t *>(ptr);
	for (unsigned y = 0; y < h; y++) {
		const uint16_t *row = reinterpret_cast<const uint16_t *>(p + y * pitch);
		for (unsigned x = 0; x < w; x++) {
			uint16_t v = row[x];
			uint8_t rgb[3] = {
				(uint8_t)(((v >> 11) & 0x1f) * 255 / 31),
				(uint8_t)(((v >> 5) & 0x3f) * 255 / 63),
				(uint8_t)((v & 0x1f) * 255 / 31),
			};
			fwrite(rgb, 1, 3, f);
		}
	}
	fclose(f);
}
#endif

#ifdef GB_DISPLAY
/* The picture the video unit shows is 160x144 of 15 bits per pixel with red
 * in bits 4:0, green 9:5 and blue 14:10 (fpga/ae350_soc/video.sv). gambatte
 * hands over RGB565, so each pixel loses its green low bit and its red and
 * green fields move; note that libretro's own 0RGB1555 puts red at 14:10 and
 * blue at 4:0, the other way round, so writing that format straight through
 * would show blue where red belongs. */
/* The core reports 160x144 (AV in the CORE line); the destination format is
 * the video unit's. */
enum { DISPLAY_MAX_W = 160 };

[[maybe_unused]] uint16_t display_pixel(uint16_t c)
{
	return (uint16_t)((c >> 11) | (((c >> 6) & 0x1fu) << 5) |
			  ((c & 0x1fu) << 10));
}

/* Fold the converted frame into crc, a row at a time. The checksum is taken
 * from the conversion itself rather than from a destination, so it is the
 * same on both builds no matter which frames reached a frame buffer. */
[[maybe_unused]] uint32_t display_crc(const void *ptr, unsigned w, unsigned h,
				      size_t pitch, uint32_t crc, bool want)
{
	uint16_t row_out[DISPLAY_MAX_W];
	const uint8_t *src = static_cast<const uint8_t *>(ptr);

	if (!want)
		return crc;
	if (w > DISPLAY_MAX_W)
		w = DISPLAY_MAX_W;
	for (unsigned y = 0; y < h; y++) {
		const uint16_t *row =
			reinterpret_cast<const uint16_t *>(src + y * pitch);

		for (unsigned x = 0; x < w; x++)
			row_out[x] = display_pixel(row[x]);
		crc = crc_update(crc, row_out, w * sizeof(uint16_t));
	}
	return crc;
}

/* Convert the frame into dst, whose pixels are stride apart. */
[[maybe_unused]] void display_store(const void *ptr, unsigned w, unsigned h,
				    size_t pitch, uint16_t *dst, size_t stride)
{
	const uint8_t *src = static_cast<const uint8_t *>(ptr);

	if (w > DISPLAY_MAX_W)
		w = DISPLAY_MAX_W;
	for (unsigned y = 0; y < h; y++) {
		const uint16_t *row =
			reinterpret_cast<const uint16_t *>(src + y * pitch);
		uint16_t *out = dst + y * stride;

		for (unsigned x = 0; x < w; x++)
			out[x] = display_pixel(row[x]);
	}
}
#endif

#ifdef GB_AUDIO_OUT
/* gambatte hands over 32,768 Hz sample pairs, while the I2S DAC and HDMI both
 * run at 75 MHz / 1536 = 48,828.125 Hz (AUD-001, DEV-020). Resampling here is
 * libretro's arrangement: the core reports its rate and the frontend converts
 * to the sink's. 32,768 / 48,828.125 is exactly 0.67108864 input samples per
 * output sample, or 0.6741504 when the run is paced to the 60 Hz picture
 * (AUDIO_STEP_Q32), which a Q32 phase step holds without drift, and each output
 * sample is interpolated linearly between the two input samples either side of
 * it. A write to a full FIFO is dropped and counted by the unit itself.
 *
 * The sound is mixed to mono first, (L + R) / 2, and the same sample goes to
 * both sides, as TinyNES's does: a sound the game pans to one side is heard on
 * both rather than on that side alone. The average cannot clip, and a sound
 * panned hard to one side comes out at half its level, the usual cost of a
 * downmix. Stereo is a later choice, and the I2S unit and HDMI already carry
 * two channels. */
#ifdef GB_PACE
/* Paced to the 60 Hz picture, the core makes 70,224 / 128 = 548.625 pairs a
 * frame at 60 frames a second, 32,917.5 Hz, and 32,917.5 / 48,828.125 is
 * exactly 0.6741504. The picture's 74.25 MHz and the bus's 75 MHz come from
 * the same 50 MHz crystal, so the two rates do not drift apart. */
const uint64_t AUDIO_STEP_Q32 = 2895453921ull; /* 0.6741504 * 2^32, rounded */
#else
const uint64_t AUDIO_STEP_Q32 = 2882303762ull; /* 0.67108864 * 2^32, rounded */
#endif
const uint64_t AUDIO_ONE_Q32 = 1ull << 32;

int16_t rs_prev, rs_cur;
uint64_t rs_pos;
bool rs_primed;
uint64_t audio_us;
unsigned audio_pairs;
/* The last pair emitted, for the level top-up below. */
int16_t audio_last;
/* The master volume's gain (tn_volume_gain), which every pair is scaled by
 * before the FIFO; unity until the host says otherwise. */
uint32_t audio_gain = TN_VOLUME_UNITY;
unsigned audio_fix, audio_skipped;
unsigned audio_skip;	/* pairs the resampler is still to leave out */
/* One emulated frame carries about 814 pairs and the unit drains them over
 * the frame, so the level just after a frame's push has to cover a whole
 * frame plus margin, and stay far enough below the FIFO's 2,048 pairs that a
 * frame made a few milliseconds late does not overflow it: the push lands
 * after the drain it follows, and the time between pushes moves by up to
 * about 3 ms, some 160 pairs. Paced to the picture, the core's rate and the
 * DAC's are locked, so nothing moves the level but that jitter and a missed
 * blank, and it is held between AUDIO_LOW and AUDIO_HIGH after each push by
 * repeating the newest pair or leaving pairs out: a quarter of the distance
 * outside the band each frame, at least AUDIO_TOPUP and at most AUDIO_STEP_MAX,
 * so a level the start leaves far out settles within a few frames instead of
 * overflowing for hundreds while two pairs a frame bring it back. */
const unsigned AUDIO_LOW = 1200;
const unsigned AUDIO_HIGH = 1500;
const unsigned AUDIO_PRIME = 900; /* covers the first frame; its push lands in the band */
const unsigned AUDIO_TOPUP = 2;
const unsigned AUDIO_STEP_MAX = 64;

unsigned audio_step(unsigned distance)
{
	const unsigned step = distance / 4;

	return step < AUDIO_TOPUP ? AUDIO_TOPUP :
	       step > AUDIO_STEP_MAX ? AUDIO_STEP_MAX : step;
}

int16_t audio_lerp(int16_t a, int16_t b, uint64_t pos)
{
	return (int16_t)((int32_t)a +
			 (int32_t)(((int64_t)((int32_t)b - (int32_t)a) *
				    (int64_t)pos) >> 32));
}

void audio_resample(const int16_t *data, size_t frames)
{
	for (size_t i = 0; i < frames; i++) {
		const int16_t m = (int16_t)(((int32_t)data[2 * i] + data[2 * i + 1]) / 2);

		if (!rs_primed) {
			rs_prev = rs_cur = m;
			rs_pos = 0;
			rs_primed = true;
			continue;
		}
		rs_cur = m;
		while (rs_pos < AUDIO_ONE_Q32) {
			audio_last = (int16_t)tn_volume_apply(
				audio_lerp(rs_prev, rs_cur, rs_pos), audio_gain);
			if (audio_skip) {
				audio_skip--;
				audio_skipped++;
			} else {
				TN_AUDIO_DATA = ((uint32_t)(uint16_t)audio_last << 16) |
						(uint16_t)audio_last;
				audio_pairs++;
			}
			rs_pos += AUDIO_STEP_Q32;
		}
		rs_pos -= AUDIO_ONE_Q32;
		rs_prev = rs_cur;
	}
}
#endif

#ifdef GB_CART
/* The launcher sends the cartridge after this program asks for it: the boot
 * ROM's loader, compiled in here, receives it into the heap as the TNR1 image
 * `ae350run` and tools/load_ae350.py --file send, and the core is given that
 * buffer instead of one built into the program. A cartridge sent as a .zip is
 * inflated here, into a buffer of its own, and the archive is freed: four
 * megabytes covers both the largest Game Boy Color image and the archive that
 * holds it, and the heap reaches the frame buffers at 0x41fc0000. */
const size_t CART_MAX = 4u << 20;

const uint8_t *receive_cart(size_t &size)
{
	uint8_t *buffer = static_cast<uint8_t *>(malloc(CART_MAX + 8));

	if (!buffer)
		return nullptr;
	printf("TinyGBC WANT %08lx %08lx\n",
	       (unsigned long)(uintptr_t)buffer,
	       (unsigned long)(CART_MAX + 8));
	fflush(stdout);
	tn_load_reset();
	uint32_t got = 0;

	for (int second = 0; second < 30 && !got; second++)
		got = tn_load_poll(TN_CORE_HZ);
	uint32_t magic = 0, length = 0;

	memcpy(&magic, buffer, 4);
	memcpy(&length, buffer + 4, 4);
	if (got != (uintptr_t)buffer || magic != 0x31524e54u /* "TNR1" */ ||
	    length == 0 || length > CART_MAX)
		return nullptr;

	if (cart_is_zip(buffer + 8, length)) {
		uint8_t *rom = nullptr;
		size_t rom_size = 0;
		char name[96];
		const char *why = cart_unzip(buffer + 8, length, CART_MAX, &rom,
					     &rom_size, name, sizeof name);

		free(buffer);
		if (why) {
			printf("TinyGBC ZIP FAILED %s\n", why);
			return nullptr;
		}
		printf("TinyGBC ZIP %s %u %u\n", name, (unsigned)length,
		       (unsigned)rom_size);
		size = rom_size;
		return rom;
	}

	size = length;
	return buffer + 8;
}

/* What the host fills, in one headerless blob: the master volume in percent
 * and the mute, from /tinygbc.ini, and the host's clock in seconds since 1970
 * UTC, 0 when it has none. These bytes are the ABI with the host (TinyTang
 * ports/bl616/tang_fpga_uart.c), so the two sides change together, and the
 * program asks for sizeof this. What is here is what it keeps when the host
 * says nothing or sends fewer bytes, as one built before the clock was added
 * does: full volume, not muted, and no date. */
struct HostSettings {
	uint8_t volume;
	uint8_t mute;
	uint8_t reserved[6];
	int64_t utc;
};
static_assert(sizeof(HostSettings) == 16, "the settings blob is 16 bytes");
HostSettings host_settings = { 100, 0, {}, 0 };

/* Ask the host for the settings, as TinyNES's player does: the answer lands
 * directly in host_settings in the loader's protocol, and nothing arriving
 * within a second leaves the defaults. */
void receive_settings()
{
	printf("TinyGBC SETTINGS WANT %08lx %08lx\n",
	       (unsigned long)(uintptr_t)&host_settings,
	       (unsigned long)sizeof host_settings);
	fflush(stdout);
	tn_load_reset();
	uint32_t got = 0;

	for (int step = 0; step < 10 && !got; step++)
		got = tn_load_poll(TN_CORE_HZ / 10);
}
#endif

/* A file the card keeps for the cartridge, held in one of gambatte's own
 * buffers: the battery save (RETRO_MEMORY_SAVE_RAM, the .sav) and the clock of
 * an MBC3 or HuC3 cartridge (RETRO_MEMORY_RTC, the .rtc), which is gambatte's
 * 8-byte baseTime, the second since 1970 at which the cartridge's clock read
 * zero, kept apart from the save as RetroArch keeps it. The card's copy is
 * taken into the buffer before the first frame and given back when the run
 * ends; start_crc is what the buffer held as the game began, so a run that
 * never changed it does not write the card. */
struct CardFile {
	const char *channel;	/* TinyTang's channel: NVRAM or RTC */
	uint8_t *buf = nullptr;	/* null when the cartridge has none */
	uint32_t len = 0;
	uint32_t start_crc = 0;

	/* A file nobody wrote is still what the core allocated: every byte the
	 * same. Worth saying, because a blank file and a real one look identical
	 * from the card. */
	bool blank() const
	{
		for (uint32_t i = 1; i < len; i++) {
			if (buf[i] != buf[0])
				return false;
		}
		return true;
	}
};
CardFile sram = { "NVRAM" };
[[maybe_unused]] CardFile rtc = { "RTC" };

#ifdef GB_CART
/* Ask the host for the cartridge's file and take it straight into the core's
 * buffer, in the boot ROM's load protocol, as TinyNES's player does and with
 * the lines TinyTang's ae350run answers for either product name (files under
 * /root/saves/gbc/). Nothing arriving within about a second means the game
 * has no such file yet, the usual case, and leaves the buffer alone. True
 * when the file arrived. */
bool receive_file(const CardFile &file)
{
	if (!file.buf || !file.len)
		return false;
	printf("TinyGBC %s WANT %08lx %08lx\n", file.channel,
	       (unsigned long)(uintptr_t)file.buf, (unsigned long)file.len);
	fflush(stdout);
	tn_load_reset();
	uint32_t got = 0;

	for (int step = 0; step < 10 && !got; step++)
		got = tn_load_poll(TN_CORE_HZ / 10);
	/* The load's own checksums judge the payload, and its entry says which
	 * buffer it was framed for: anything else is not this file. */
	if (got != (uintptr_t)file.buf)
		return false;
	printf("TinyGBC %s GOT %lu %08lx%s\n", file.channel, (unsigned long)file.len,
	       (unsigned long)crc_update(0, file.buf, file.len),
	       file.blank() ? " BLANK" : "");
	fflush(stdout);
	return true;
}

/* Let the link's line out and leave it quiet a moment: the first bytes after a
 * quiet spell are where it loses some (TinyNES, 2026-10-14), and the card's
 * writes at the far end hold off the UART while they run. */
void save_pace()
{
	tn_wait_tx_idle();
	const uint64_t until = ticks() + tick_hz / 250;

	while (ticks() < until)
		;
}

/* Give the file back as text, in TinyNES's form: link mode frames lines and
 * drops carriage returns, so the bytes go as hex, 32 to a line, between a SAVE
 * line carrying the length and CRC-32 and a DONE line, which ae350run checks
 * before it replaces the card's file. A file the run did not change is not
 * sent, so the card keeps its copy and the exit costs nothing. */
void send_file(const CardFile &file)
{
	if (!file.buf || !file.len)
		return;
	const uint32_t crc = crc_update(0, file.buf, file.len);

	if (crc == file.start_crc) {
		printf("TinyGBC %s SAME %08lx\n", file.channel, (unsigned long)crc);
		fflush(stdout);
		tn_wait_tx_idle();
		return;
	}
	for (int lead = 0; lead < 3; lead++) {
		printf("TinyGBC %s BEGIN\n", file.channel);
		save_pace();
	}
	printf("TinyGBC %s SAVE %lu %08lx%s\n", file.channel, (unsigned long)file.len,
	       (unsigned long)crc, file.blank() ? " BLANK" : "");
	save_pace();
	const char *const digits = "0123456789abcdef";
	char hex[65];
	unsigned n = 0;

	for (uint32_t i = 0; i < file.len; i++) {
		hex[n++] = digits[file.buf[i] >> 4];
		hex[n++] = digits[file.buf[i] & 15];
		if (n == 64) {
			hex[n] = 0;
			printf("%s\n", hex);
			save_pace();
			n = 0;
		}
	}
	if (n) {
		hex[n] = 0;
		printf("%s\n", hex);
		save_pace();
	}
	printf("TinyGBC %s DONE %08lx\n", file.channel, (unsigned long)crc);
	tn_wait_tx_idle();
	save_pace();
}
#endif

#if defined(GB_CART) || defined(GB_STATE_TEST)
/* Save states are gambatte's own (retro_serialize): the CPU, the work and
 * video RAM, the PPU, the APU, the timer and the cartridge's banking, RAM and
 * clock, one buffer whose length depends on the cartridge and not on when it
 * is taken. The cartridge's RAM is in it, so a loaded state's battery RAM is
 * what the exit writes back as the battery save: the state wins, as it does
 * in TinyNES (TinyNES core-log entry 48).
 *
 * A slot is a state file as the card keeps it: TinyNES's 32-byte header with
 * TinyGBC's own magic, then the state. The header names the cartridge, so a
 * state is never loaded into another game, and the core it came from, which
 * is reported but not enforced. gambatte is built unpatched, so the patch
 * field is 0. */
const char STATE_MAGIC[4] = { 'T', 'G', 'S', '1' };

struct StateHeader {
	char magic[4];		/* "TGS1" */
	uint16_t header_bytes;	/* sizeof(StateHeader) */
	uint16_t version;	/* 1 */
	uint32_t rom_crc;	/* CRC-32 of the cartridge the state was taken in */
	uint32_t gambatte;	/* the first 32 bits of gambatte's commit */
	uint32_t patches;	/* 0: gambatte is built as fetched */
	uint32_t frame;		/* the player's frame count when saved */
	uint32_t length;	/* bytes of state after the header */
	uint32_t crc;		/* CRC-32 of those bytes */
};
static_assert(sizeof(StateHeader) == 32, "the state header is 32 bytes");

struct StateSlot {
	uint8_t *buf = nullptr;	/* header, then state */
	size_t size = 0;	/* the state's own length */
	bool full = false;
	StateHeader *header() const { return reinterpret_cast<StateHeader *>(buf); }
	size_t file_bytes() const { return sizeof(StateHeader) + size; }
};

bool state_alloc(StateSlot &slot)
{
	const size_t size = retro_serialize_size();

	if (size == 0)
		return false;
	slot.buf = static_cast<uint8_t *>(malloc(sizeof(StateHeader) + size));
	slot.size = slot.buf ? size : 0;
	return slot.buf != nullptr;
}

bool state_save(StateSlot &slot, uint32_t rom_crc, uint32_t frame)
{
	uint8_t *const state = slot.buf + sizeof(StateHeader);

	slot.full = retro_serialize(state, slot.size);
	if (!slot.full)
		return false;
	StateHeader &h = *slot.header();
	memcpy(h.magic, STATE_MAGIC, sizeof h.magic);
	h.header_bytes = sizeof(StateHeader);
	h.version = 1;
	h.rom_crc = rom_crc;
	h.gambatte = GB_GAMBATTE_COMMIT;
	h.patches = 0;
	h.frame = frame;
	h.length = (uint32_t)slot.size;
	h.crc = crc_update(0, state, slot.size);
	return true;
}

/* Why a slot's file cannot be loaded into this game, or nullptr if it can. */
const char *state_refusal(const StateSlot &slot, uint32_t rom_crc)
{
	const StateHeader &h = *slot.header();

	if (memcmp(h.magic, STATE_MAGIC, sizeof h.magic) != 0 ||
	    h.header_bytes != sizeof(StateHeader) || h.version != 1)
		return "NOT A STATE";
	if (h.rom_crc != rom_crc)
		return "OTHER GAME";
	if (h.length != slot.size)
		return "WRONG LENGTH";
	if (h.crc != crc_update(0, slot.buf + sizeof(StateHeader), slot.size))
		return "BAD CRC";
	return nullptr;
}

bool state_load(const StateSlot &slot)
{
	return slot.full && retro_unserialize(slot.buf + sizeof(StateHeader), slot.size);
}
#endif

#ifdef GB_CART
/* Ask the host for the cartridge's save state, which the card keeps as its
 * .state, and take it into the slot without loading it: R loads it, as it
 * loads one saved since. Nothing arriving within about a second means the
 * game has none, the usual case. A file whose header names another game, or
 * whose state does not match its own checksum, leaves the slot empty. */
void receive_state(StateSlot &slot, uint32_t rom_crc)
{
	printf("TinyGBC STATE WANT %08lx %08lx\n", (unsigned long)(uintptr_t)slot.buf,
	       (unsigned long)slot.file_bytes());
	fflush(stdout);
	tn_load_reset();
	uint32_t got = 0;

	for (int step = 0; step < 10 && !got; step++)
		got = tn_load_poll(TN_CORE_HZ / 10);
	if (got != (uintptr_t)slot.buf)
		return;
	if (const char *why = state_refusal(slot, rom_crc)) {
		printf("TinyGBC GB STATE BAD %s\n", why);
		return;
	}
	const StateHeader &h = *slot.header();

	slot.full = true;
	printf("TinyGBC GB STATE READY F=%lu CORE=%s\n", (unsigned long)h.frame,
	       h.gambatte == GB_GAMBATTE_COMMIT ? "SAME" : "OTHER");
}

/* A saved state goes to the host, which keeps it on the card as the
 * cartridge's .state, while the game runs, in TinyNES's form: two lines to
 * wake the link, whose first frame after a quiet spell is where it loses bytes
 * (save_pace), the file's length and CRC-32, then the file itself, STATE_CHUNK
 * bytes a frame, as bytes rather than hex and through tn_put_char rather than
 * printf, whose newline gains a carriage return. The host counts the payload
 * by its length, so nothing else may be printed until it has gone: a line
 * that comes due meanwhile waits in note, and a newer save waits behind the
 * one going. */
const size_t STATE_CHUNK = 256;

struct StateSender {
	uint8_t *buf = nullptr;	/* a copy of the slot, which L may overwrite */
	size_t len = 0, sent = 0;
	uint32_t crc = 0;
	unsigned phase = 0;	/* 0 idle, 1-2 wake lines, 3 header, 4 payload */
	bool again = false;
	char note[112] = "";
	bool busy() const { return phase != 0; }
};

StateSlot quick;
StateSender state_tx;

void sender_start(StateSender &tx, const StateSlot &slot)
{
	memcpy(tx.buf, slot.buf, slot.file_bytes());
	tx.len = slot.file_bytes();
	tx.crc = crc_update(0, tx.buf, tx.len);
	tx.sent = 0;
	tx.phase = 1;
	tx.again = false;
}

/* A line now, or once the payload has gone. Only the newest waits. */
void state_say(StateSender &tx, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	if (tx.busy()) {
		vsnprintf(tx.note, sizeof tx.note, format, args);
	} else {
		vprintf(format, args);
		printf("\n");
		fflush(stdout);
	}
	va_end(args);
}

/* One frame's share of the send, then whatever waited for it. */
void sender_step(StateSender &tx, const StateSlot &slot)
{
	if (tx.phase == 1 || tx.phase == 2) {
		printf("TinyGBC STATE BEGIN\n");
		fflush(stdout);
		tx.phase++;
		return;
	}
	if (tx.phase == 3) {
		printf("TinyGBC STATE DATA %lu %08lx\n", (unsigned long)tx.len,
		       (unsigned long)tx.crc);
		fflush(stdout);
		tx.phase = 4;
		return;
	}
	if (tx.phase == 4) {
		const size_t n = tx.len - tx.sent < STATE_CHUNK ? tx.len - tx.sent : STATE_CHUNK;

		for (size_t i = 0; i < n; i++)
			tn_put_char((char)tx.buf[tx.sent + i]);
		tx.sent += n;
		if (tx.sent < tx.len)
			return;
		tx.phase = 0;
		printf("TinyGBC GB STATE SENT %lu %08lx\n", (unsigned long)tx.len,
		       (unsigned long)tx.crc);
	}
	if (tx.note[0]) {
		printf("%s\n", tx.note);
		tx.note[0] = 0;
	}
	if (tx.again)
		sender_start(tx, slot);
	fflush(stdout);
}
#endif

#ifdef GB_PAD_OUT
/* The pad the fabric polls on bus 0. Bytes 4 and 5 of its report carry
 * fifteen active-low buttons and the left stick is bytes 0 and 1; reading
 * DATA also takes bytes 4 and 5 for the DATA2 read that follows. */
uint32_t pad_status, pad_data, pad_data2;
uint16_t pad_bits; /* RETRO_DEVICE_ID_JOYPAD_* bits */
bool pad_ready;
#ifdef GB_PAD_TRACE
/* Print every change of the raw report, so a button can be identified by the
 * bit it sets rather than by a table. */
uint32_t pad_last_data, pad_last_data2;
bool pad_last_valid;
#endif

/* This controller's real layout, measured from its own reports rather than
 * taken from WiiBrew's table, which did not match it (core-log entry 13): the
 * trace of one press per button gave the bits below. Both bytes are active
 * low, byte 4 in DATA2 bits 7:0 and byte 5 in bits 15:8. */
enum {
	PAD_B4_R = 1u << 1,
	PAD_B4_PLUS = 1u << 2,
	PAD_B4_HOME = 1u << 3,
	PAD_B4_MINUS = 1u << 4,
	PAD_B4_L = 1u << 5,
	PAD_B4_DOWN = 1u << 6,
	PAD_B4_RIGHT = 1u << 7,
	PAD_B5_UP = 1u << 0,
	PAD_B5_LEFT = 1u << 1,
	PAD_B5_ZR = 1u << 2,
	PAD_B5_X = 1u << 3,
	PAD_B5_A = 1u << 4,
	PAD_B5_Y = 1u << 5,
	PAD_B5_B = 1u << 6,
	PAD_B5_ZL = 1u << 7,
};

void pad_poll(void)
{
	const unsigned present = TN_PAD_PRESENT | TN_PAD_CLASSIC;

	pad_status = TN_PAD_STATUS(0);
	pad_data = TN_PAD_DATA(0);
	pad_data2 = TN_PAD_DATA2(0);
#ifdef GB_PAD_TRACE
	if (!pad_last_valid || pad_data != pad_last_data ||
	    pad_data2 != pad_last_data2) {
		printf("TinyGBC GB PADTRACE DATA=%08x DATA2=%08x B4=%02x B5=%02x\n",
		       (unsigned)pad_data, (unsigned)pad_data2,
		       (unsigned)(~pad_data2 & 0xffu),
		       (unsigned)((~pad_data2 >> 8) & 0xffu));
		pad_last_data = pad_data;
		pad_last_data2 = pad_data2;
		pad_last_valid = true;
	}
#endif
	pad_bits = 0;
	pad_ready = (pad_status & present) == present;
	if (!pad_ready)
		return;

	const unsigned b4 = ~pad_data2 & 0xffu;
	const unsigned b5 = (~pad_data2 >> 8) & 0xffu;

	if (b5 & PAD_B5_A)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_A;
	if (b5 & PAD_B5_B)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_B;
	if (b4 & PAD_B4_PLUS)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_START;
	if (b4 & PAD_B4_MINUS)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
	if (b5 & PAD_B5_UP)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
	if (b4 & PAD_B4_DOWN)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
	if (b5 & PAD_B5_LEFT)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
	if (b4 & PAD_B4_RIGHT)
		pad_bits |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
#ifdef GB_CART
	/* The desktop is over the game: its pointer has the controller. */
	if (TN_LINK_STATUS & TN_LINK_SHOWN)
		pad_bits = 0;
#endif
}

/* HOME and MINUS together end a run built with FRAMES=0. */
bool pad_quit(void)
{
	const unsigned b4 = ~pad_data2 & 0xffu;

	return pad_ready && (b4 & PAD_B4_HOME) && (b4 & PAD_B4_MINUS);
}

#ifdef GB_CART
/* L or R held with HOME, as PAD_B4_L and PAD_B4_R: L saves the state and R
 * loads it. Neither is a tap of HOME (desk_link.sv), so TinyDesk is not
 * brought up by them. */
unsigned pad_combo(void)
{
	const unsigned b4 = ~pad_data2 & 0xffu;

	if (!pad_ready || !(b4 & PAD_B4_HOME))
		return 0;
	return b4 & (PAD_B4_L | PAD_B4_R);
}
#endif
#endif

// What the core last handed over, recorded in the callback and read after
// retro_run has returned.
const void *frame_ptr;
unsigned frame_w, frame_h;
size_t frame_pitch;

static const size_t AUDIO_MAX = 8192; /* sample frames */
int16_t audio_copy[2 * AUDIO_MAX];
size_t audio_copy_frames;

[[maybe_unused]] unsigned frame_no;

#ifdef GB_PLAY
/* The keyboard pad, as bits at RETRO_DEVICE_ID_JOYPAD_* positions. */
uint16_t play_keys;
#endif

extern "C" {

void gb_video(const void *data, unsigned width, unsigned height, size_t pitch)
{
	frame_ptr = data;
	frame_w = width;
	frame_h = height;
	frame_pitch = pitch;
}

size_t gb_audio(const int16_t *data, size_t frames)
{
	size_t n = frames > AUDIO_MAX ? AUDIO_MAX : frames;
	memcpy(audio_copy, data, n * 2 * sizeof(int16_t));
	audio_copy_frames += n;
	return frames;
}

void gb_input_poll(void) {}

int16_t gb_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
	if (port != 0 || device != RETRO_DEVICE_JOYPAD || index != 0)
		return 0;
#ifdef GB_PLAY
	return (play_keys >> id) & 1u;
#else
#ifdef GB_PAD_OUT
	if (pad_ready)
		return (pad_bits >> id) & 1u;
#endif
	return (pad_state(frame_no) >> id) & 1u;
#endif
}

bool gb_environment(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:
		*static_cast<bool *>(data) = true;
		return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
		return data && *static_cast<const retro_pixel_format *>(data) ==
				       RETRO_PIXEL_FORMAT_RGB565;
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
		/* Nothing on this board has a filesystem: the core probes for a
		 * boot ROM and a palette and finds neither, which is what is
		 * wanted, since no boot ROM can be shipped. */
		*static_cast<const char **>(data) = ".";
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
		*static_cast<bool *>(data) = false;
		return true;
	case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
	case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
	case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
	case RETRO_ENVIRONMENT_SET_MESSAGE:
	case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
	case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
	case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
	case RETRO_ENVIRONMENT_SET_VARIABLE:
		return true;
	default:
		/* Core options, logging, rumble, bitmasks, the VFS and the
		 * fast-forward override are all declined, so the core keeps its
		 * own defaults and reads the pad one button at a time. */
		return false;
	}
}

} // extern "C"

#ifdef GB_STATE_TEST
/* One frame into a pair of checksums: every frame's picture and sound, so a
 * replay that differs anywhere is caught. */
void fold_frame(uint32_t &video, uint32_t &audio)
{
	if (frame_ptr) {
		const uint8_t *p = static_cast<const uint8_t *>(frame_ptr);

		for (unsigned y = 0; y < frame_h; y++)
			video = crc_update(video, p + y * frame_pitch, frame_w * 2);
	}
	audio = crc_update(audio, audio_copy, audio_copy_frames * 2 * sizeof(int16_t));
}
#endif

} // namespace


#if defined(GB_PLAY) && !defined(__riscv)
/*
 * A window to play in, for taking savestates by hand. X11 because the host
 * has libX11 and no SDL2 headers. It writes a state on F5, on SIGUSR1, on
 * SIGINT and on quit, and a rolling autosave every ten seconds, so a state
 * is never more than a moment behind whatever is being played.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <csignal>
#include <unistd.h>

namespace {

volatile sig_atomic_t save_signal;
void on_save_signal(int) { save_signal = 1; }

const char *state_path = "gb_play.state";
unsigned play_frame;

bool write_state(const char *path)
{
	size_t size = retro_serialize_size();
	if (!size)
		return false;
	void *buf = malloc(size);
	if (!buf)
		return false;
	if (!retro_serialize(buf, size)) {
		free(buf);
		return false;
	}
	FILE *f = fopen(path, "wb");
	if (!f) {
		free(buf);
		return false;
	}
	size_t n = fwrite(buf, 1, size, f);
	fclose(f);
	free(buf);
	printf("TinyGBC GB STATE frame=%u bytes=%u path=%s\n", play_frame,
	       (unsigned)n, path);
	fflush(stdout);
	return n == size;
}

unsigned key_bit(KeySym k)
{
	switch (k) {
	case XK_Up:      return 1u << RETRO_DEVICE_ID_JOYPAD_UP;
	case XK_Down:    return 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
	case XK_Left:    return 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
	case XK_Right:   return 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
	case XK_z: case XK_Z:       return 1u << RETRO_DEVICE_ID_JOYPAD_B;
	case XK_x: case XK_X:       return 1u << RETRO_DEVICE_ID_JOYPAD_A;
	case XK_Return:  return 1u << RETRO_DEVICE_ID_JOYPAD_START;
	case XK_Shift_R: return 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
	default:         return 0;
	}
}

int play_main()
{
	crc_init();

	/* The core, set up as the benchmark sets it up. */
	retro_set_environment(gb_environment);
	retro_set_video_refresh(gb_video);
	retro_set_audio_sample_batch(gb_audio);
	retro_set_input_poll(gb_input_poll);
	retro_set_input_state(gb_input_state);
	retro_init();

	retro_game_info info;
	memset(&info, 0, sizeof info);
	info.path = "built-in";
	info.data = gb_rom;
	info.size = (size_t)(gb_rom_end - gb_rom);
	if (!retro_load_game(&info)) {
		printf("TinyGBC GB LOAD FAILED\n");
		return 1;
	}
	const size_t state_size = (size_t)(gb_state_end - gb_state);
	if (state_size && !retro_unserialize(gb_state, state_size)) {
		printf("TinyGBC GB STATE FAILED\n");
		return 1;
	}
	if (state_size)
		printf("TinyGBC GB STATE=%u LOADED\n", (unsigned)state_size);

	printf("TinyGBC GB PLAY ROM=%u CGB=%u\n", (unsigned)info.size,
	       info.size > 0x143 ? (gb_rom[0x143] >> 7) & 1u : 0u);
	printf("TinyGBC GB KEYS arrows=dpad z=B x=A enter=start rshift=select"
	       " f5=save esc=quit\n");
	fflush(stdout);

	Display *dpy = XOpenDisplay(NULL);
	if (!dpy) {
		printf("TinyGBC GB NO DISPLAY\n");
		return 1;
	}
	const int screen = DefaultScreen(dpy);
	const int scale = 4;
	const unsigned W = 160 * scale, H = 144 * scale;
	Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0, W, H,
					 0, BlackPixel(dpy, screen),
					 BlackPixel(dpy, screen));
	XStoreName(dpy, win, "TinyGBC - Oracle of Seasons");
	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete, 1);
	XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask | ExposureMask |
				StructureNotifyMask);
	/* Key repeat would deliver a KeyRelease per repeat and drop the key. */
	XAutoRepeatOff(dpy);

	GC gc = XCreateGC(dpy, win, 0, NULL);
	char *pixels = static_cast<char *>(calloc((size_t)W * H, 4));
	XImage *img = XCreateImage(dpy, DefaultVisual(dpy, screen),
				   (unsigned)DefaultDepth(dpy, screen), ZPixmap, 0,
				   pixels, W, H, 32, 0);
	XMapWindow(dpy, win);
	XFlush(dpy);

	struct sigaction sa;
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = on_save_signal;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	const long frame_ns = 16742700; /* 70224 / 4194304 s */
	timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	long long next = (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
	bool running = true;

	while (running) {
		while (XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (ev.type == ClientMessage &&
			    (Atom)ev.xclient.data.l[0] == wm_delete)
				running = false;
			else if (ev.type == KeyPress) {
				KeySym k = XLookupKeysym(&ev.xkey, 0);
				if (k == XK_Escape)
					running = false;
				else if (k == XK_F5)
					write_state(state_path);
				else
					play_keys |= key_bit(k);
			} else if (ev.type == KeyRelease) {
				play_keys &= (uint16_t)~key_bit(
					XLookupKeysym(&ev.xkey, 0));
			}
		}
		if (save_signal) {
			save_signal = 0;
			write_state(state_path);
		}

		retro_run();
		play_frame++;

		if (!(play_frame % 600))
			write_state("gb_play.state.auto");

		/* RGB565 to the window's 32-bit pixels, nearest neighbour. */
		if (frame_ptr) {
			const uint8_t *src = static_cast<const uint8_t *>(frame_ptr);
			for (unsigned y = 0; y < H; y++) {
				const uint16_t *row = reinterpret_cast<const uint16_t *>(
					src + (y / scale) * frame_pitch);
				uint32_t *out = reinterpret_cast<uint32_t *>(
					pixels + (size_t)y * img->bytes_per_line);
				for (unsigned x = 0; x < W; x++) {
					uint16_t v = row[x / scale];
					out[x] = ((uint32_t)((v >> 11) & 0x1f) << 19) |
						 ((uint32_t)((v >> 5) & 0x3f) << 10) |
						 ((uint32_t)(v & 0x1f) << 3);
				}
			}
			XPutImage(dpy, win, gc, img, 0, 0, 0, 0, W, H);
			XFlush(dpy);
		}

		next += frame_ns;
		clock_gettime(CLOCK_MONOTONIC, &now);
		long long t = (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
		if (next > t) {
			timespec d = {0, next - t};
			nanosleep(&d, NULL);
		} else if (t - next > 5 * frame_ns) {
			next = t;
		}
	}

	write_state(state_path);
	XAutoRepeatOn(dpy);
	XCloseDisplay(dpy);
	return 0;
}
} // namespace
#endif

int main()
{
#if defined(GB_PLAY) && !defined(__riscv)
	return play_main();
#else
	crc_init();
#ifndef __riscv
	dump_dir = getenv("GB_DUMP");
#endif
	if (!parse_input(GB_INPUT)) {
		printf("TinyGBC GB BAD INPUT\n");
		return 2;
	}

#ifdef GB_CART
	size_t rom_size = 0;
	const uint8_t *cart = receive_cart(rom_size);

	if (!cart) {
		printf("TinyGBC CART FAILED\n");
		return 1;
	}
	const uint32_t rom_crc = crc_update(0, cart, rom_size);
	const unsigned cgb = rom_size > 0x143 ? (cart[0x143] >> 7) & 1u : 0u;
#else
	const size_t rom_size = (size_t)(gb_rom_end - gb_rom);
	const uint32_t rom_crc = crc_update(0, gb_rom, rom_size);
	const unsigned cgb = rom_size > 0x143 ? (gb_rom[0x143] >> 7) & 1u : 0u;
	const uint8_t *cart = gb_rom;
#endif

	uint64_t t0 = ticks();
	retro_set_environment(gb_environment);
	retro_set_video_refresh(gb_video);
	retro_set_audio_sample_batch(gb_audio);
	retro_set_input_poll(gb_input_poll);
	retro_set_input_state(gb_input_state);
	retro_init();

	retro_system_av_info av;
	memset(&av, 0, sizeof av);
	retro_get_system_av_info(&av);

	retro_game_info info;
	memset(&info, 0, sizeof info);
	info.path = "built-in";
	info.data = cart;
	info.size = rom_size;

	if (!retro_load_game(&info)) {
		printf("TinyGBC GB LOAD FAILED ROM=%u\n", (unsigned)rom_size);
		return 1;
	}
	const size_t state_size = (size_t)(gb_state_end - gb_state);
	if (state_size && !retro_unserialize(gb_state, state_size)) {
		printf("TinyGBC GB STATE FAILED\n");
		return 1;
	}
	uint64_t start_us = to_us(ticks() - t0);

	printf("TinyGBC GB CORE=gambatte ROM=%u CRC=%08x CGB=%u"
	       " AV=%ux%u FPS=%.4f HZ=%.0f START=%u\n",
	       (unsigned)rom_size, (unsigned)rom_crc, cgb, av.geometry.base_width,
	       av.geometry.base_height, (double)av.timing.fps,
	       (double)av.timing.sample_rate, (unsigned)start_us);
	if (state_size)
		printf("TinyGBC GB STATE=%u LOADED\n", (unsigned)state_size);

	sram.buf = static_cast<uint8_t *>(retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
	sram.len = sram.buf ? (uint32_t)retro_get_memory_size(RETRO_MEMORY_SAVE_RAM) : 0;
#ifndef __riscv
	/* GB_SAVE=<file> stands in for the card on the PC: a file of the save's
	 * size is taken into the save RAM before the first frame, and the save RAM
	 * is written back to it at the end. */
	const char *save_path = getenv("GB_SAVE");
	if (save_path && sram.len) {
		if (FILE *in = fopen(save_path, "rb")) {
			const bool whole = fread(sram.buf, 1, sram.len, in) == sram.len &&
					   fgetc(in) == EOF;
			fclose(in);
			if (!whole) {
				printf("TinyGBC GB NVRAM BAD SIZE\n");
				return 1;
			}
			printf("TinyGBC GB NVRAM LOADED %u %08x\n", (unsigned)sram.len,
			       (unsigned)crc_update(0, sram.buf, sram.len));
		}
	}
#endif
#ifdef GB_CART
	(void)receive_file(sram);
	sram.start_crc = crc_update(0, sram.buf, sram.len);
	/* The one save state, in the SDRAM: L saves into it, R loads it. The
	 * card's copy of the cartridge's state is taken into it now, not loaded. */
	if (state_alloc(quick))
		state_tx.buf = static_cast<uint8_t *>(malloc(quick.file_bytes()));
	const bool quick_ok = quick.buf && state_tx.buf;
	unsigned combo_prev = 0;

	printf("TinyGBC GB STATE SLOT=%u\n", quick_ok ? (unsigned)quick.size : 0u);
	if (quick_ok)
		receive_state(quick, rom_crc);
	receive_settings();
#ifdef GB_AUDIO_OUT
	audio_gain = tn_volume_gain(host_settings.volume, host_settings.mute);
#endif
	printf("TinyGBC GB VOLUME=%u%s\n", (unsigned)host_settings.volume,
	       host_settings.mute ? " MUTED" : "");
	/* The date, when the host's clock has one: time() runs on from it, so an
	 * MBC3 or HuC3 cartridge's clock counts real time, the time the board was
	 * off included, and its .rtc means the same thing in every session.
	 * Without a date time() counts from reset, and no .rtc is read or written,
	 * so a clock that never knew the date is never kept. */
	if (host_settings.utc > 0) {
		tn_rt_set_date(host_settings.utc);
		rtc.buf = static_cast<uint8_t *>(retro_get_memory_data(RETRO_MEMORY_RTC));
		rtc.len = rtc.buf ? (uint32_t)retro_get_memory_size(RETRO_MEMORY_RTC) : 0;
		rtc.start_crc = crc_update(0, rtc.buf, rtc.len);
		/* No .rtc yet is a cartridge never played here: its clock starts at
		 * zero now, as a new cartridge's would, rather than at gambatte's
		 * baseTime of 0, which reads as some 20,000 days. The buffer then
		 * differs from start_crc, so the exit writes the .rtc. */
		if (!receive_file(rtc) && rtc.len == sizeof(uint64_t)) {
			const uint64_t now = (uint64_t)time(nullptr);

			memcpy(rtc.buf, &now, sizeof now);
		}
	}
	printf("TinyGBC GB DATE=%lld RTC=%u\n", (long long)host_settings.utc,
	       (unsigned)rtc.len);
	/* Link mode last: from here this program's text goes out as TinyDesk
	 * frames, the desk layer can sit over the picture, and the launcher that
	 * started us knows the game is up. */
	printf("TinyGBC LINK\n");
	fflush(stdout);
	TN_UART_CTRL = TN_UART_LINK_MODE;
#endif

	/* FRAMES=0 means run until HOME and MINUS are held together. */
#ifdef GB_LB_TEST
	{
		uint16_t *fb = reinterpret_cast<uint16_t *>(TN_VIDEO_FB(0));
		const size_t stride = TN_VIDEO_FB_STRIDE / TN_VIDEO_PIXEL_BYTES;
		unsigned first_bad = 160;

		for (unsigned y = 0; y < TN_VIDEO_HEIGHT; y++)
			for (unsigned x = 0; x < TN_VIDEO_WIDTH; x++)
				fb[stride * y + x] = (uint16_t)x;
		tn_dcache_wbinval();
		TN_VIDEO_CTRL = TN_VIDEO_ENABLE;
		{
			uint32_t f0 = TN_VIDEO_FRAMES;

			while (TN_VIDEO_FRAMES - f0 < 3)
				;
		}
		for (unsigned i = 0; i < TN_VIDEO_WIDTH; i++)
			if ((TN_VIDEO_LB(i) & 0xffffu) != i) {
				first_bad = i;
				break;
			}
		printf("TinyGBC GB LBDEBUG=%08x LBDEBUG2=%08x\n",
		       (unsigned)TN_VIDEO_DEBUG, (unsigned)TN_VIDEO_DEBUG2);
		printf("TinyGBC GB LBTEST first_bad=%u LB0=%u LB31=%u LB32=%u"
		       " LB127=%u LB128=%u LB159=%u\n", first_bad,
		       (unsigned)(TN_VIDEO_LB(0) & 0xffffu),
		       (unsigned)(TN_VIDEO_LB(31) & 0xffffu),
		       (unsigned)(TN_VIDEO_LB(32) & 0xffffu),
		       (unsigned)(TN_VIDEO_LB(127) & 0xffffu),
		       (unsigned)(TN_VIDEO_LB(128) & 0xffffu),
		       (unsigned)(TN_VIDEO_LB(159) & 0xffffu));
		return 0;
	}
#endif
	const unsigned frames = GB_FRAMES;
	/* 70224 dots a frame at 4194304 Hz, in microseconds. */
	const uint64_t period_us = 70224ull * 1000000ull / 4194304ull;

	uint32_t video_crc = 0, audio_crc = 0;
	uint64_t total_us = 0, min_us = ~0ull, max_us = 0;
	uint64_t total_samples = 0;
	unsigned over = 0;
#ifdef GB_PAD_OUT
	/* The register map's buses are zero based: bus 0 is PMOD0's Digilent
	 * pins 1 and 7, the pair a Classic Controller was read on before. */
	printf("TinyGBC GB PADLINES=%08x\n", (unsigned)TN_PAD_LINES);
	for (unsigned pad = 0; pad < 4; pad++)
		printf("TinyGBC GB PAD%u STATUS=%08x IDENT=%08x\n", pad + 1,
		       (unsigned)TN_PAD_STATUS(pad),
		       (unsigned)TN_PAD_IDENT(pad));
#endif
#ifdef GB_AUDIO_OUT
	/* Playback starts here: the FIFO is emptied and the unit enabled, then
	 * the resampler feeds it one pair per output sample. The unit's counters
	 * wrap and run from reset, so what is reported is their movement. */
	TN_AUDIO_CTRL = TN_AUDIO_ENABLE | TN_AUDIO_FLUSH;
	/* Fill the FIFO with silence first, so the unit never starts empty. */
	for (unsigned i = 0; i < AUDIO_PRIME; i++)
		TN_AUDIO_DATA = 0;
	const uint32_t audio_frames0 = TN_AUDIO_FRAMES;
	const uint32_t audio_drops0 = TN_AUDIO_DROPS;
	const uint32_t audio_under0 = TN_AUDIO_STATUS >> 16;
#endif
#ifdef GB_DISPLAY
	uint32_t fb_crc = 0;
#if defined(__riscv)
	uint64_t show_us = 0;
	unsigned shown = 0;
	/* The buffer handed over but not yet named by STATUS, 2 for none: a CTRL
	 * write takes effect at the next vertical blank, so until STATUS shows
	 * it, that buffer may be the one the unit is about to fetch. */
	unsigned pending = 2;
	/* LATE and FRAMES count from FPGA configuration, not from this program, so
	 * what is reported is their movement across the run. */
	const uint32_t video_frames0 = TN_VIDEO_FRAMES;
	const uint32_t video_late0 = TN_VIDEO_LATE;
	const uint32_t video_pixels0 = TN_VIDEO_PIXELS;
#ifdef GB_PACE
	uint32_t pace_vbl = TN_VIDEO_FRAMES;
	unsigned missed = 0;
	unsigned paced_shown = 0;	/* shown as of the last pace */
#endif
#endif
#endif

	/* Frames since the last F= line. */
	uint64_t iv_us = 0, iv_max = 0;
	unsigned iv_frames = 0;
#ifdef GB_STATE_TEST
	StateSlot test_slot;
	uint64_t test_save_us = 0;
	uint32_t test_video = 0, test_audio = 0;
#endif

	unsigned ran = 0;

	for (unsigned f = 0; frames == 0 || f < frames; f++) {
		frame_no = f;
		frame_ptr = nullptr;
		audio_copy_frames = 0;
#ifdef GB_PAD_OUT
		pad_poll();
		if (frames == 0 && pad_quit())
			break;
#endif
#ifdef GB_CART
		/* HOME with L saves the state and HOME with R loads it, once a
		 * press, while the game has the controller. A save goes to the card
		 * as well, in the background. */
		{
			const unsigned combo = pad_combo();
			const unsigned pressed = combo & ~combo_prev;

			combo_prev = combo;
			if (pressed && quick_ok && !(TN_LINK_STATUS & TN_LINK_SHOWN)) {
				const uint64_t t0 = ticks();

				if (pressed & PAD_B4_L) {
					const bool ok = state_save(quick, rom_crc, f);

					state_say(state_tx, "TinyGBC GB STATE SAVE %s F=%u US=%llu",
						  ok ? "OK" : "FAILED", f,
						  (unsigned long long)to_us(ticks() - t0));
					if (ok) {
						if (state_tx.busy())
							state_tx.again = true;
						else
							sender_start(state_tx, quick);
					}
				} else if (!quick.full) {
					state_say(state_tx, "TinyGBC GB STATE LOAD EMPTY F=%u", f);
				} else if (const char *why = state_refusal(quick, rom_crc)) {
					state_say(state_tx, "TinyGBC GB STATE LOAD REFUSED %s", why);
				} else {
					const bool ok = state_load(quick);

					state_say(state_tx, "TinyGBC GB STATE LOAD %s F=%u FROM=%lu US=%llu",
						  ok ? "OK" : "FAILED", f,
						  (unsigned long)quick.header()->frame,
						  (unsigned long long)to_us(ticks() - t0));
				}
			}
		}
#endif
#ifdef GB_STATE_TEST
		if (f == GB_STATE_TEST) {
			const uint64_t t0 = ticks();

			if (!state_alloc(test_slot) || !state_save(test_slot, rom_crc, f)) {
				printf("TinyGBC GB STATE SAVE FAILED\n");
				return 1;
			}
			test_save_us = to_us(ticks() - t0);
			/* gambatte's libretro driver resets its audio resampler and
			 * output buffer on every load (retro_unserialize), so the
			 * sound after a load is framed differently from the sound of
			 * a run that never loaded, while the machine is the same. The
			 * run therefore loads its own state here, and the replay
			 * after it starts from the same reset: what is compared is
			 * whether the state holds everything the frames after it
			 * depend on. */
			if (!state_load(test_slot)) {
				printf("TinyGBC GB STATE LOAD FAILED\n");
				return 1;
			}
		}
#endif
		ran = f + 1;

		uint64_t a = ticks();
		retro_run();
		uint64_t d = to_us(ticks() - a);
#ifdef GB_STATE_TEST
		if (f >= GB_STATE_TEST)
			fold_frame(test_video, test_audio);
#endif

		if (frame_ptr && f % GB_CHECK == 0) {
			const uint8_t *p = static_cast<const uint8_t *>(frame_ptr);
			const size_t row = frame_w * 2; /* RGB565 */
			for (unsigned y = 0; y < frame_h; y++)
				video_crc = crc_update(video_crc, p + y * frame_pitch, row);
#ifndef __riscv
			dump_frame(f, frame_ptr, frame_w, frame_h, frame_pitch);
#endif
		}
#ifdef GB_AUDIO_OUT
		if (audio_copy_frames) {
			uint64_t a0 = ticks();

			audio_resample(audio_copy, audio_copy_frames);
			/* Repeat the newest pair when the reserve is short, rather
			 * than let the unit underrun, and leave pairs out of the next
			 * frame when it is long, rather than let it overflow. */
			if (audio_pairs) {
				unsigned level = TN_AUDIO_LEVEL(TN_AUDIO_STATUS);

				if (level > AUDIO_HIGH)
					audio_skip = audio_step(level - AUDIO_HIGH);

				const unsigned add = level < AUDIO_LOW ?
					audio_step(AUDIO_LOW - level) : 0;

				for (unsigned k = 0; k < add; k++) {
					TN_AUDIO_DATA =
						((uint32_t)(uint16_t)audio_last
						 << 16) |
						(uint16_t)audio_last;
					audio_fix++;
				}
			}
			audio_us += to_us(ticks() - a0);
		}
#endif
#ifdef GB_DISPLAY
		if (frame_ptr)
			fb_crc = display_crc(frame_ptr, frame_w, frame_h, frame_pitch,
					     fb_crc, f % GB_CHECK == 0);
#if defined(__riscv)
		if (frame_ptr) {
			const unsigned shown_buf =
				(TN_VIDEO_STATUS & TN_VIDEO_BUFFER) ? 1u : 0u;
			/* Write and hand over only once the previous handover has been
			 * taken at a blank: writing a buffer whose CTRL write is still
			 * pending tears the frame the unit begins fetching from it.
			 * This also bounds handovers to one per vertical blank, which
			 * suits a harness running at about four times the Game Boy's
			 * rate. The picture settles two frames after the write (entry
			 * 5), so a handover is not expected to be visible at once. */
			if (pending == 2 || shown_buf == pending) {
				const unsigned target = shown_buf ? 0u : 1u;
				uint16_t *dst = reinterpret_cast<uint16_t *>(
					TN_VIDEO_FB(target));
				uint64_t s0 = ticks();

				display_store(frame_ptr, frame_w, frame_h,
					      frame_pitch, dst,
					      TN_VIDEO_FB_STRIDE /
						      TN_VIDEO_PIXEL_BYTES);
				tn_dcache_wbinval();
				TN_VIDEO_CTRL = TN_VIDEO_ENABLE |
						(target ? TN_VIDEO_BUFFER : 0u);
				pending = target;
				shown++;
				show_us += to_us(ticks() - s0);
			}
		}
#endif
#endif
		if (audio_copy_frames)
			audio_crc = crc_update(audio_crc, audio_copy,
					       audio_copy_frames * 2 * sizeof(int16_t));
		total_samples += audio_copy_frames;

		total_us += d;
		if (d < min_us)
			min_us = d;
		if (d > max_us)
			max_us = d;
		if (d > period_us)
			over++;
		iv_us += d;
		if (d > iv_max)
			iv_max = d;
		iv_frames++;

#ifdef GB_PACE
#if defined(__riscv)
		/* One emulated frame for each vertical blank of the 60 Hz picture,
		 * as TinyNES's player does: a frame handed over is waited for until
		 * STATUS names it, which is the blank that shows it, and the next
		 * frame is made after that, so every frame is shown once. Pacing by
		 * the Game Boy's own 59.7275 Hz instead beat against the picture's
		 * 60 Hz every 3.67 s, repeating one frame and skipping the next
		 * through each beat (core-log entry 28). The game runs 60 / 59.7275,
		 * 0.46 percent, fast, and the resampler's step is set to match. A
		 * frame that was not handed over waits for the next blank alone. */
		if (shown != paced_shown) {
			const uint32_t want = TN_VIDEO_SHOWING |
					      (pending ? TN_VIDEO_BUFFER : 0u);

			while ((TN_VIDEO_STATUS & (TN_VIDEO_SHOWING | TN_VIDEO_BUFFER)) != want)
				;
		} else {
			const uint32_t vbl = TN_VIDEO_FRAMES;

			while (TN_VIDEO_FRAMES == vbl)
				;
		}
		{
			const uint32_t vbl = TN_VIDEO_FRAMES;

			/* The first frame is counted from before the loop. */
			if (f && vbl - pace_vbl > 1)
				missed++;
			pace_vbl = vbl;
			paced_shown = shown;
		}
#else
		/* One emulated frame per Game Boy frame, so the picture can be
		 * watched at the rate the game runs. */
		while (to_us(ticks() - a) < period_us)
			;
#endif
#endif

		/* A report that comes due while a state is going out waits for it,
		 * or the host would count it as payload. */
#ifdef GB_CART
		const bool report_held = state_tx.busy();
#else
		const bool report_held = false;
#endif
		if (iv_frames >= GB_CHECK && !report_held) {
			printf("TinyGBC GB F=%u AVG=%u MAX=%u VIDEO=%08x AUDIO=%08x\n",
			       f + 1, (unsigned)(iv_us / iv_frames), (unsigned)iv_max,
			       (unsigned)video_crc, (unsigned)audio_crc);
			iv_us = 0;
			iv_max = 0;
			iv_frames = 0;
		}
#ifdef GB_CART
		sender_step(state_tx, quick);
#endif
	}
#ifdef GB_CART
	/* A state still going out is finished before anything else is printed,
	 * paced as the battery save is, or the host would count the summary as
	 * its payload. */
	while (state_tx.busy() || state_tx.again) {
		sender_step(state_tx, quick);
		save_pace();
	}
#endif

	if (iv_frames)
		printf("TinyGBC GB F=%u AVG=%u MAX=%u VIDEO=%08x AUDIO=%08x\n", ran,
		       (unsigned)(iv_us / iv_frames), (unsigned)iv_max,
		       (unsigned)video_crc, (unsigned)audio_crc);

	uint64_t avg = total_us / (ran ? ran : 1);
	unsigned speed_x10 = (unsigned)(period_us * 1000 / avg);
	printf("TinyGBC GB FRAMES=%u AVG=%u MIN=%u MAX=%u OVER=%u SPEED=%u.%u%%"
	       " SAMPLES=%u VIDEO=%08x AUDIO=%08x\n",
	       ran, (unsigned)avg, (unsigned)min_us, (unsigned)max_us, over,
	       speed_x10 / 10, speed_x10 % 10, (unsigned)total_samples,
	       (unsigned)video_crc, (unsigned)audio_crc);

#ifdef GB_STATE_TEST
	{
		/* The refusals first: another cartridge's state, and the state with
		 * one byte changed, must not load. */
		StateHeader &h = *test_slot.header();
		uint8_t *const byte = test_slot.buf + sizeof(StateHeader) + test_slot.size / 2;

		h.rom_crc ^= 1u;
		const char *const other = state_refusal(test_slot, rom_crc);
		h.rom_crc ^= 1u;
		*byte ^= 0x40u;
		const char *const changed = state_refusal(test_slot, rom_crc);
		*byte ^= 0x40u;
		const bool refuses = other && !strcmp(other, "OTHER GAME") && changed &&
				     !strcmp(changed, "BAD CRC");
		const char *const why = state_refusal(test_slot, rom_crc);

		/* Then the frames after the save, again, from the state. */
		uint32_t replay_video = 0, replay_audio = 0;
		const uint64_t t0 = ticks();
		const bool loaded = !why && state_load(test_slot);
		const uint64_t load_us = to_us(ticks() - t0);

		for (unsigned f = GB_STATE_TEST; loaded && f < ran; f++) {
			frame_no = f;
			frame_ptr = nullptr;
			audio_copy_frames = 0;
			retro_run();
			fold_frame(replay_video, replay_audio);
		}
		const bool match = loaded && replay_video == test_video &&
				   replay_audio == test_audio;

		printf("TinyGBC GB STATE AT=%u BYTES=%u SAVE=%llu LOAD=%llu"
		       " VIDEO=%08x/%08x AUDIO=%08x/%08x %s\n", (unsigned)GB_STATE_TEST,
		       (unsigned)test_slot.size, (unsigned long long)test_save_us,
		       (unsigned long long)load_us, (unsigned)test_video,
		       (unsigned)replay_video, (unsigned)test_audio,
		       (unsigned)replay_audio,
		       !refuses ? "REFUSALS FAILED" : why ? why :
		       !loaded ? "LOAD FAILED" : match ? "MATCH" : "DIFFER");
		if (!refuses || !match)
			return 1;
	}
#endif

#ifdef GB_PAD_OUT
	/* The register map's buses are zero based: bus 0 is PMOD0's Digilent
	 * pins 1 and 7, the pair a Classic Controller was read on before. */
	printf("TinyGBC GB PADLINES=%08x\n", (unsigned)TN_PAD_LINES);
	for (unsigned pad = 0; pad < 4; pad++)
		printf("TinyGBC GB PAD%u STATUS=%08x IDENT=%08x\n", pad + 1,
		       (unsigned)TN_PAD_STATUS(pad),
		       (unsigned)TN_PAD_IDENT(pad));
#endif
#ifdef GB_AUDIO_OUT
	{
		const uint32_t st = TN_AUDIO_STATUS;

		printf("TinyGBC GB AUDIO SENT=%u PAIRS=%u FIX=%u SKIP=%u DROPS=%u"
		       " UNDERRUNS=%u LEVEL=%u US=%u\n",
		       (unsigned)(TN_AUDIO_FRAMES - audio_frames0), audio_pairs,
		       audio_fix, audio_skipped,
		       (unsigned)(TN_AUDIO_DROPS - audio_drops0),
		       (unsigned)((st >> 16) - audio_under0),
		       (unsigned)TN_AUDIO_LEVEL(st),
		       (unsigned)(audio_us / (ran ? ran : 1)));
	}
#endif
#ifdef GB_DISPLAY
	printf("TinyGBC GB DISPLAY FB=%08x", (unsigned)fb_crc);
#ifdef GB_PACE
	printf(" PACE=1");
#else
	printf(" PACE=0");
#endif
#if defined(__riscv)
#ifdef GB_PACE
	printf(" MISSED=%u", missed);
#endif
	printf(" SHOWN=%u SHOW=%u VF=%u LATE=%u PIXELS=%u BITS=%04x PIXELS0=%u",
	       shown, shown ? (unsigned)(show_us / shown) : 0u,
	       (unsigned)(TN_VIDEO_FRAMES - video_frames0),
	       (unsigned)(TN_VIDEO_LATE - video_late0),
	       (unsigned)TN_VIDEO_PIXELS, (unsigned)TN_VIDEO_BITS,
	       (unsigned)video_pixels0);
#endif
	printf("\n");
#endif

#ifdef GB_CART
	send_file(sram);
	send_file(rtc);
#endif
#ifndef __riscv
	if (save_path && sram.len) {
		FILE *out = fopen(save_path, "wb");

		if (!out || fwrite(sram.buf, 1, sram.len, out) != sram.len) {
			printf("TinyGBC GB NVRAM WRITE FAILED\n");
			return 1;
		}
		fclose(out);
		printf("TinyGBC GB NVRAM SAVED %u %08x%s\n", (unsigned)sram.len,
		       (unsigned)crc_update(0, sram.buf, sram.len),
		       sram.blank() ? " BLANK" : "");
	}
#endif

	return 0;
#endif
}
