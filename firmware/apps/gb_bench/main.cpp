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
// runs one emulated frame per Game Boy frame, which is how the picture is
// watched at the rate the game runs.
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

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef __riscv
#include "tinygbc.h"
#include "uart.h"
#ifdef GB_CART
#include "load.h" /* the boot ROM's loader, for the cartridge the launcher sends */
#endif
#else
#include <ctime>
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
 * output sample, which a Q32 phase step holds without drift, and each output
 * sample is interpolated linearly between the two input samples either side of
 * it. A write to a full FIFO is dropped and counted by the unit itself. */
const uint64_t AUDIO_STEP_Q32 = 2882303762ull; /* 0.67108864 * 2^32, rounded */
const uint64_t AUDIO_ONE_Q32 = 1ull << 32;

int16_t rs_prev_l, rs_prev_r, rs_cur_l, rs_cur_r;
uint64_t rs_pos;
bool rs_primed;
uint64_t audio_us;
unsigned audio_pairs;
/* The last pair emitted, for the level top-up below. */
int16_t audio_last_l, audio_last_r;
unsigned audio_fix;
/* One emulated frame carries about 818 pairs and the unit drains them over
 * the frame, so the reserve kept in the FIFO has to cover a whole frame plus
 * margin: 1,500 of 2,048 pairs. The emulator's pacing is a hair slower than
 * the DAC, so the level creeps down and is topped up two pairs at a time. */
const unsigned AUDIO_LOW = 1500;
const unsigned AUDIO_PRIME = 1100; /* room for one frame's push */
const unsigned AUDIO_TOPUP = 2;

int16_t audio_lerp(int16_t a, int16_t b, uint64_t pos)
{
	return (int16_t)((int32_t)a +
			 (int32_t)(((int64_t)((int32_t)b - (int32_t)a) *
				    (int64_t)pos) >> 32));
}

void audio_resample(const int16_t *data, size_t frames)
{
	for (size_t i = 0; i < frames; i++) {
		const int16_t l = data[2 * i], r = data[2 * i + 1];

		if (!rs_primed) {
			rs_prev_l = rs_cur_l = l;
			rs_prev_r = rs_cur_r = r;
			rs_pos = 0;
			rs_primed = true;
			continue;
		}
		rs_cur_l = l;
		rs_cur_r = r;
		while (rs_pos < AUDIO_ONE_Q32) {
			audio_last_l = audio_lerp(rs_prev_l, rs_cur_l, rs_pos);
			audio_last_r = audio_lerp(rs_prev_r, rs_cur_r, rs_pos);
			TN_AUDIO_DATA = ((uint32_t)(uint16_t)audio_last_l << 16) |
					(uint16_t)audio_last_r;
			audio_pairs++;
			rs_pos += AUDIO_STEP_Q32;
		}
		rs_pos -= AUDIO_ONE_Q32;
		rs_prev_l = rs_cur_l;
		rs_prev_r = rs_cur_r;
	}
}
#endif

#ifdef GB_CART
/* The launcher sends the cartridge after this program asks for it: the boot
 * ROM's loader, compiled in here, receives it into the heap as the TNR1 image
 * `ae350run` and tools/load_ae350.py --file send, and the core is given that
 * buffer instead of one built into the program. */
const size_t CART_MAX = 2u << 20;

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
	size = length;
	return buffer + 8;
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

#ifdef GB_CART
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
#endif
#endif

	/* Frames since the last F= line. */
	uint64_t iv_us = 0, iv_max = 0;
	unsigned iv_frames = 0;

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
		ran = f + 1;

		uint64_t a = ticks();
		retro_run();
		uint64_t d = to_us(ticks() - a);

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
			/* The emulator's pacing is a hair slower than the DAC's clock,
			 * so the FIFO drains over a long run; repeat the newest pair
			 * when the reserve is short rather than let the unit underrun. */
			if (audio_pairs) {
				unsigned level = TN_AUDIO_LEVEL(TN_AUDIO_STATUS);

				for (unsigned k = 0;
				     k < AUDIO_TOPUP && level < AUDIO_LOW; k++) {
					TN_AUDIO_DATA =
						((uint32_t)(uint16_t)audio_last_l
						 << 16) |
						(uint16_t)audio_last_r;
					level++;
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
		/* One emulated frame per Game Boy frame, so the picture can be
		 * watched at the rate the game runs. */
		while (to_us(ticks() - a) < period_us)
			;
#endif

		if (iv_frames == GB_CHECK) {
			printf("TinyGBC GB F=%u AVG=%u MAX=%u VIDEO=%08x AUDIO=%08x\n",
			       f + 1, (unsigned)(iv_us / iv_frames), (unsigned)iv_max,
			       (unsigned)video_crc, (unsigned)audio_crc);
			iv_us = 0;
			iv_max = 0;
			iv_frames = 0;
		}
	}

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

		printf("TinyGBC GB AUDIO SENT=%u PAIRS=%u FIX=%u DROPS=%u"
		       " UNDERRUNS=%u LEVEL=%u US=%u\n",
		       (unsigned)(TN_AUDIO_FRAMES - audio_frames0), audio_pairs,
		       audio_fix, (unsigned)(TN_AUDIO_DROPS - audio_drops0),
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
	printf(" SHOWN=%u SHOW=%u VF=%u LATE=%u PIXELS=%u BITS=%04x PIXELS0=%u",
	       shown, shown ? (unsigned)(show_us / shown) : 0u,
	       (unsigned)(TN_VIDEO_FRAMES - video_frames0),
	       (unsigned)(TN_VIDEO_LATE - video_late0),
	       (unsigned)TN_VIDEO_PIXELS, (unsigned)TN_VIDEO_BITS,
	       (unsigned)video_pixels0);
#endif
	printf("\n");
#endif

	return 0;
#endif
}
