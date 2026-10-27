/* SPDX-License-Identifier: MIT
 *
 * The picture settings, as the core options gambatte reads them. The player
 * cannot reach gambatte's palette machinery any other way: the fabric's
 * palette and emphasis stores are dead since the picture began reading the
 * frame buffer, and libgambatte's setters sit behind the libretro core's own
 * object, so the options a frontend would answer are the only door.
 *
 * These are the four keys the core asks for and the values it compares against
 * (third_party/gambatte-libretro/libgambatte/libretro/libretro.cpp,
 * check_variables). Anything not answered here leaves the core on its own
 * default, which is what this program did before the settings existed:
 * no colourisation for a monochrome cartridge and "GBC only", accurate, for a
 * colour one. tools/test_video_options.sh checks these strings against that
 * file, so a value the core stops recognising is a failing test rather than a
 * setting that silently does nothing. */

#ifndef TINYGBC_VIDEO_H
#define TINYGBC_VIDEO_H

#include <stdint.h>
#include <string.h>

/* The palette a monochrome cartridge is drawn in. It has no effect on a colour
 * cartridge, whose colours come from its own data; the core returns before it
 * reads this. The order is the order /tinygbc.ini and the Video page list
 * them, and 0 is what an older host's zero-filled blob means. */
enum {
	GB_PAL_OFF,      /* gambatte's own default: no colourisation */
	GB_PAL_AUTO,     /* the palette gambatte thinks suits the game */
	GB_PAL_DMG,      /* the original green */
	GB_PAL_POCKET,   /* the Game Boy Pocket's grey */
	GB_PAL_LIGHT,    /* the Game Boy Light's blue-green */
	GB_PAL_GBC_BROWN,/* one of the Color's built-in presets */
	GB_PAL_COUNT
};

/* How a colour cartridge's colours are corrected for the Color's LCD. Off is
 * what answering nothing has always meant here: libretro.cpp leaves
 * colorCorrection at zero when the option is declined, so the picture this
 * project has measured its checksums against is the raw, uncorrected one.
 * Accurate is therefore an opt-in, not the default, whatever the core's own
 * option table would have said. */
enum {
	GB_COLOUR_OFF,      /* the raw colours the cartridge asks for */
	GB_COLOUR_ACCURATE, /* the LCD's response, as real hardware shows it */
	GB_COLOUR_FAST,     /* darkens and desaturates, for a slow machine */
	GB_COLOUR_COUNT
};

static const char *const gb_palette_names[GB_PAL_COUNT] = {
	"Off", "Auto", "DMG", "Pocket", "Light", "GBC",
};

static const char *const gb_colour_names[GB_COLOUR_COUNT] = {
	"Off", "Accurate", "Fast",
};

/* What gambatte's `gambatte_gb_colorization` should say for this palette, and
 * the `gambatte_gb_internal_palette` that goes with it when the colourisation
 * is the internal kind. NULL from the second means the key need not be
 * answered at all. */
static inline const char *gb_colorization_value(unsigned palette)
{
	return palette == GB_PAL_OFF ? "disabled"
	     : palette == GB_PAL_AUTO ? "auto"
				      : "internal";
}

static inline const char *gb_internal_palette_value(unsigned palette)
{
	switch (palette) {
	case GB_PAL_DMG:
		return "GB - DMG";
	case GB_PAL_POCKET:
		return "GB - Pocket";
	case GB_PAL_LIGHT:
		return "GB - Light";
	case GB_PAL_GBC_BROWN:
		return "GBC - Brown";
	default:
		/* Off and Auto name no preset: Auto picks for itself. */
		return NULL;
	}
}

/* And the two the colour correction reads. */
static inline const char *gb_colour_correction_value(unsigned colour)
{
	return colour == GB_COLOUR_OFF ? "disabled" : "GBC only";
}

static inline const char *gb_colour_mode_value(unsigned colour)
{
	return colour == GB_COLOUR_FAST ? "fast" : "accurate";
}

/* One core option: the value to answer with, or NULL to leave the core on its
 * own default. `key` is the option's key as the core asks for it. */
static inline const char *gb_video_option(const char *key, unsigned palette,
					  unsigned colour)
{
	if (!key)
		return NULL;
	if (palette >= GB_PAL_COUNT) {
		palette = GB_PAL_OFF;
	}
	if (colour >= GB_COLOUR_COUNT) {
		colour = GB_COLOUR_OFF;
	}
	if (strcmp(key, "gambatte_gb_colorization") == 0) {
		return gb_colorization_value(palette);
	}
	if (strcmp(key, "gambatte_gb_internal_palette") == 0) {
		return gb_internal_palette_value(palette);
	}
	if (strcmp(key, "gambatte_gbc_color_correction") == 0) {
		return gb_colour_correction_value(colour);
	}
	if (strcmp(key, "gambatte_gbc_color_correction_mode") == 0) {
		return gb_colour_mode_value(colour);
	}
	return NULL;
}

#endif /* TINYGBC_VIDEO_H */
