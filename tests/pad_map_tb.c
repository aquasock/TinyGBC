/* SPDX-License-Identifier: MIT
 *
 * The pad mapping, driven on the PC: tools/test_pad_map.sh builds this against
 * firmware/include/tinygbc_pad.h and fails on any check. What it pins is that
 * the default mapping is the fixed decode the player had before the mapping
 * existed, that every Game Boy button can be reached from every offered
 * source, and that the places a mapping may not go are refused. */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tinygbc_pad.h"

/* The joypad positions, as the core has them. The player's static_asserts hold
 * these against libretro.h; here they are spelled out so this test needs no
 * core headers. */
enum {
	JB = 0, JA = 8, JSELECT = 2, JSTART = 3,
	JUP = 4, JDOWN = 5, JLEFT = 6, JRIGHT = 7,
};

/* The decode as it was before the mapping, byte by byte, from core-log entry
 * 13's measured layout. This is the behaviour the default must reproduce. */
static uint16_t old_decode(unsigned b4, unsigned b5)
{
	uint16_t bits = 0;

	if (b5 & (1u << 4)) bits |= 1u << JA;      /* Classic A */
	if (b5 & (1u << 6)) bits |= 1u << JB;      /* Classic B */
	if (b4 & (1u << 2)) bits |= 1u << JSTART;  /* Plus */
	if (b4 & (1u << 4)) bits |= 1u << JSELECT; /* Minus */
	if (b5 & (1u << 0)) bits |= 1u << JUP;
	if (b4 & (1u << 6)) bits |= 1u << JDOWN;
	if (b5 & (1u << 1)) bits |= 1u << JLEFT;
	if (b4 & (1u << 7)) bits |= 1u << JRIGHT;
	return bits;
}

/* One controller button pressed, as the two report bytes pad_decode() reads:
 * those are already made active high by the player, so every button but the
 * pressed one is clear. */
static void press(uint16_t button, unsigned *b4, unsigned *b5)
{
	*b4 = (((unsigned)button & 0x7fu) << 1) | 0x01u;
	*b5 = ((unsigned)button >> 7) & 0xffu;
}

static unsigned failures;

static void check(bool ok, const char *what)
{
	if (!ok) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

int main(void)
{
	const uint8_t standard[8] = PAD_DEFAULT_MAP;
	unsigned b4, b5;

	/* 1. The default mapping is the fixed decode, over every combination of
	 *    the two report bytes: 65,536 of them, not a sample. */
	for (unsigned lo = 0; lo < 256; lo++) {
		for (unsigned hi = 0; hi < 256; hi++) {
			const uint16_t got = pad_decode(lo, hi, standard);
			const uint16_t want = old_decode(lo, hi);

			if (got != want) {
				printf("FAIL default decode b4=%02x b5=%02x got=%04x want=%04x\n",
				       lo, hi, got, want);
				failures++;
				lo = 256;
				break;
			}
		}
	}
	printf("default decode: 65536 report combinations\n");

	/* 2. Every Game Boy button can be reached from every offered source, and
	 *    from nothing else: one source, one button, one bit. */
	for (unsigned src = 0; src < PAD_SRC_COUNT; src++) {
		for (unsigned btn = 0; btn < 8; btn++) {
			uint8_t map[8] = { PAD_SRC_COUNT, PAD_SRC_COUNT, PAD_SRC_COUNT,
					   PAD_SRC_COUNT, PAD_SRC_COUNT, PAD_SRC_COUNT,
					   PAD_SRC_COUNT, PAD_SRC_COUNT };
			map[btn] = (uint8_t)src;
			press(pad_source[src], &b4, &b5);
			const uint16_t got = pad_decode(b4, b5, map);

			if (got != (uint16_t)(1u << pad_joypad[btn])) {
				printf("FAIL source %u to button %u: got %04x\n", src, btn, got);
				failures++;
			}
		}
	}
	printf("reached: %u sources x 8 buttons\n", (unsigned)PAD_SRC_COUNT);

	/* 3. An index past the table names no button, which is how a settings
	 *    file leaves one unbound. */
	{
		uint8_t unmapped[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

		b4 = 0;
		b5 = 0; /* every button held */
		check(pad_decode(b4, b5, unmapped) == 0,
		      "an out-of-range index yields no button");
		b4 = ~0u & 0xffu;
		b5 = 0xff;
		check(pad_decode(b4, b5, unmapped) == 0,
		      "an out-of-range index yields no button, buttons released");
	}

	/* 4. Two buttons on one source: the whole point of a remap. */
	{
		uint8_t both[8] = PAD_DEFAULT_MAP;

		both[0] = PAD_SRC_X; /* B */
		both[1] = PAD_SRC_X; /* A */
		press(PAD_X, &b4, &b5);
		check(pad_decode(b4, b5, both) ==
			      (uint16_t)((1u << JB) | (1u << JA)),
		      "one source can answer to two buttons");
	}

	/* 5. HOME is not offered: a tap of it belongs to the desk layer. */
	{
		bool home = false;

		for (unsigned i = 0; i < PAD_SRC_COUNT; i++)
			if (pad_source[i] == PAD_HOME)
				home = true;
		check(!home, "HOME is not a mappable source");
	}

	/* 6. The table has no duplicates and nothing outside the measured word. */
	for (unsigned i = 0; i < PAD_SRC_COUNT; i++) {
		check(pad_source[i] != 0, "every source is a button");
		for (unsigned j = i + 1; j < PAD_SRC_COUNT; j++)
			check(pad_source[i] != pad_source[j], "sources are distinct");
	}

	/* 7. The report's two bytes land where WII-003 measured them: byte 4 bit 1
	 *    is R, byte 5 bit 7 is ZL. */
	press(PAD_R, &b4, &b5);
	check(pad_held(b4, b5) == PAD_R, "byte 4 bit 1 is R");
	press(PAD_ZL, &b4, &b5);
	check(pad_held(b4, b5) == PAD_ZL, "byte 5 bit 7 is ZL");
	press(PAD_HOME, &b4, &b5);
	check(pad_held(b4, b5) == PAD_HOME, "byte 4 bit 3 is HOME");

	if (failures) {
		printf("TinyGBC PADMAP FAILED %u\n", failures);
		return 1;
	}
	printf("TinyGBC PADMAP PASS\n");
	return 0;
}
