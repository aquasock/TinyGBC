/* SPDX-License-Identifier: MIT
 *
 * The pad's decode, apart from the registers that carry it: which controller
 * button answers to each Game Boy button. The player fills the two report
 * bytes from the fabric's pad registers and hands them here with the mapping
 * the host sent; tools/test_pad_map.sh drives this same code on the PC, so a
 * mapping fault is a test failure rather than a play session.
 *
 * Kept free of the register map and of libretro so that a host test can
 * include it alone. */

#ifndef TINYGBC_PAD_H
#define TINYGBC_PAD_H

#include <stdint.h>

/* The Classic Controller's fifteen measured buttons as one word, in the order
 * core-log entry 13 measured them: report byte 4's bits 1 to 7, then byte 5's
 * bits 0 to 7, active low in the report (reference record WII-003). The bit
 * numbers here are this header's own; the report's are what pad_held() reads. */
enum {
	PAD_R     = 1u << 0,
	PAD_PLUS  = 1u << 1,
	PAD_HOME  = 1u << 2,
	PAD_MINUS = 1u << 3,
	PAD_L     = 1u << 4,
	PAD_DOWN  = 1u << 5,
	PAD_RIGHT = 1u << 6,
	PAD_UP    = 1u << 7,
	PAD_LEFT  = 1u << 8,
	PAD_ZR    = 1u << 9,
	PAD_X     = 1u << 10,
	PAD_A     = 1u << 11,
	PAD_Y     = 1u << 12,
	PAD_B     = 1u << 13,
	PAD_ZL    = 1u << 14,
};

/* What a mapping index names, in the order a settings file lists them. HOME is
 * deliberately absent: a tap of it belongs to the desk layer, so a game button
 * bound to it would bring the desktop up on every press. */
enum {
	PAD_SRC_A,
	PAD_SRC_B,
	PAD_SRC_START,
	PAD_SRC_SELECT,
	PAD_SRC_UP,
	PAD_SRC_DOWN,
	PAD_SRC_LEFT,
	PAD_SRC_RIGHT,
	PAD_SRC_X,
	PAD_SRC_Y,
	PAD_SRC_L,
	PAD_SRC_R,
	PAD_SRC_ZL,
	PAD_SRC_ZR,
	PAD_SRC_COUNT
};

static const uint16_t pad_source[PAD_SRC_COUNT] = {
	PAD_A, PAD_B, PAD_PLUS, PAD_MINUS,
	PAD_UP, PAD_DOWN, PAD_LEFT, PAD_RIGHT,
	PAD_X, PAD_Y, PAD_L, PAD_R, PAD_ZL, PAD_ZR,
};

/* The eight Game Boy buttons a mapping covers, at their
 * RETRO_DEVICE_ID_JOYPAD_* positions. Spelled out so this header stands alone;
 * main.cpp asserts that they still match the core's. */
#define PAD_JOY_B      0
#define PAD_JOY_A      8
#define PAD_JOY_SELECT 2
#define PAD_JOY_START  3
#define PAD_JOY_UP     4
#define PAD_JOY_DOWN   5
#define PAD_JOY_LEFT   6
#define PAD_JOY_RIGHT  7

static const uint8_t pad_joypad[8] = {
	PAD_JOY_B, PAD_JOY_A, PAD_JOY_SELECT, PAD_JOY_START,
	PAD_JOY_UP, PAD_JOY_DOWN, PAD_JOY_LEFT, PAD_JOY_RIGHT,
};

/* The mapping the player has always done, in pad_joypad[] order: B as the
 * Classic's B, A as its A, Select and Start as Minus and Plus, and the D-pad
 * as itself. */
#define PAD_DEFAULT_MAP { \
	PAD_SRC_B, PAD_SRC_A, PAD_SRC_SELECT, PAD_SRC_START, \
	PAD_SRC_UP, PAD_SRC_DOWN, PAD_SRC_LEFT, PAD_SRC_RIGHT \
}

/* The two report bytes as the word above: byte 4's bit 1 becomes PAD_R, and
 * byte 4's bit 0, which the controller leaves alone, is dropped. */
static inline unsigned pad_held(unsigned b4, unsigned b5)
{
	return ((b4 >> 1) & 0x7fu) | ((b5 & 0xffu) << 7);
}

/* One report and one mapping as the buttons now held: a 16-bit word whose set
 * bits are RETRO_DEVICE_ID_JOYPAD_* positions. An index at or past the table
 * names no button, which is how a file leaves one unbound. */
static inline uint16_t pad_decode(unsigned b4, unsigned b5, const uint8_t *map)
{
	const unsigned held = pad_held(b4, b5);
	uint16_t bits = 0;

	for (unsigned i = 0; i < 8; i++) {
		const unsigned src = map[i];

		if (src < PAD_SRC_COUNT && (held & pad_source[src]))
			bits |= (uint16_t)(1u << pad_joypad[i]);
	}
	return bits;
}

#endif /* TINYGBC_PAD_H */
