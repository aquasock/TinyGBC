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

	const size_t rom_size = (size_t)(gb_rom_end - gb_rom);
	const uint32_t rom_crc = crc_update(0, gb_rom, rom_size);
	const unsigned cgb = rom_size > 0x143 ? (gb_rom[0x143] >> 7) & 1u : 0u;

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
	info.data = gb_rom;
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

	const unsigned frames = GB_FRAMES;
	/* 70224 dots a frame at 4194304 Hz, in microseconds. */
	const uint64_t period_us = 70224ull * 1000000ull / 4194304ull;

	uint32_t video_crc = 0, audio_crc = 0;
	uint64_t total_us = 0, min_us = ~0ull, max_us = 0;
	uint64_t total_samples = 0;
	unsigned over = 0;

	/* Frames since the last F= line. */
	uint64_t iv_us = 0, iv_max = 0;
	unsigned iv_frames = 0;

	for (unsigned f = 0; f < frames; f++) {
		frame_no = f;
		frame_ptr = nullptr;
		audio_copy_frames = 0;

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
		printf("TinyGBC GB F=%u AVG=%u MAX=%u VIDEO=%08x AUDIO=%08x\n", frames,
		       (unsigned)(iv_us / iv_frames), (unsigned)iv_max,
		       (unsigned)video_crc, (unsigned)audio_crc);

	uint64_t avg = total_us / frames;
	unsigned speed_x10 = (unsigned)(period_us * 1000 / avg);
	printf("TinyGBC GB FRAMES=%u AVG=%u MIN=%u MAX=%u OVER=%u SPEED=%u.%u%%"
	       " SAMPLES=%u VIDEO=%08x AUDIO=%08x\n",
	       frames, (unsigned)avg, (unsigned)min_us, (unsigned)max_us, over,
	       speed_x10 / 10, speed_x10 % 10, (unsigned)total_samples,
	       (unsigned)video_crc, (unsigned)audio_crc);

	return 0;
#endif
}
