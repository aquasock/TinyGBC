/* SPDX-License-Identifier: MIT
 *
 * The picture settings on the PC: that every palette and colour correction
 * names a value, that the four keys are answered and nothing else is, and that
 * the strings match what gambatte compares against. The first two are checked
 * here; the third is checked by tools/test_video_options.sh against the core's
 * own source, because a value the core stops recognising is a setting that
 * silently does nothing. */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "tinygbc_video.h"

static unsigned failures;

static void check(bool ok, const char *what)
{
	if (!ok) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

/* Every distinct value the mapping can answer with, one per line, so that
 * tools/test_video_options.sh can hold each against the core's own source
 * instead of a list of its own that could drift from this one. */
static int print_values(void)
{
	static const char *seen[32];
	unsigned count = 0;

	for (unsigned p = 0; p < GB_PAL_COUNT; p++) {
		for (unsigned c = 0; c < GB_COLOUR_COUNT; c++) {
			const char *v[4] = {
				gb_video_option("gambatte_gb_colorization", p, c),
				gb_video_option("gambatte_gb_internal_palette", p, c),
				gb_video_option("gambatte_gbc_color_correction", p, c),
				gb_video_option("gambatte_gbc_color_correction_mode", p, c),
			};

			for (unsigned i = 0; i < 4; i++) {
				bool dup = false;

				if (!v[i])
					continue;
				for (unsigned j = 0; j < count; j++)
					if (strcmp(seen[j], v[i]) == 0)
						dup = true;
				if (!dup && count < sizeof seen / sizeof seen[0]) {
					seen[count++] = v[i];
					printf("%s\n", v[i]);
				}
			}
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	if (argc > 1 && strcmp(argv[1], "--values") == 0) {
		return print_values();
	}

	/* 1. Every palette answers the colourisation key, and the four presets
	 *    also name an internal palette. Off and Auto name none, because
	 *    neither selects one. */
	for (unsigned p = 0; p < GB_PAL_COUNT; p++) {
		const char *colourisation =
			gb_video_option("gambatte_gb_colorization", p, GB_COLOUR_OFF);
		const char *internal =
			gb_video_option("gambatte_gb_internal_palette", p, GB_COLOUR_OFF);
		const bool preset = p >= GB_PAL_DMG;

		check(colourisation != NULL, "every palette answers the colourisation");
		check(preset == (internal != NULL),
		      "a preset names an internal palette and Off or Auto does not");
		if (preset) {
			check(strncmp(internal, "GB - ", 5) == 0 ||
				      strncmp(internal, "GBC - ", 6) == 0,
			      "an internal palette is one of the core's own names");
		}
	}

	/* 2. Off is the no-colourisation value and a preset is the internal kind;
	 *    Auto is Auto. These three readings are what the page promises. */
	check(strcmp(gb_video_option("gambatte_gb_colorization", GB_PAL_OFF,
				     GB_COLOUR_OFF), "disabled") == 0,
	      "Off means no colourisation");
	check(strcmp(gb_video_option("gambatte_gb_colorization", GB_PAL_AUTO,
				     GB_COLOUR_OFF), "auto") == 0,
	      "Auto means the core picks");
	check(strcmp(gb_video_option("gambatte_gb_colorization", GB_PAL_DMG,
				     GB_COLOUR_OFF), "internal") == 0,
	      "a preset means the internal palette");

	/* 3. Every colour correction answers both keys, and Off is the value that
	 *    leaves today's picture alone. */
	for (unsigned c = 0; c < GB_COLOUR_COUNT; c++) {
		const char *correction =
			gb_video_option("gambatte_gbc_color_correction", 0, c);
		const char *mode = gb_video_option("gambatte_gbc_color_correction_mode", 0, c);

		check(correction != NULL && mode != NULL,
		      "every colour setting answers both keys");
		if (c == GB_COLOUR_OFF) {
			check(strcmp(correction, "disabled") == 0,
			      "Off disables the correction");
		} else {
			check(strcmp(correction, "GBC only") == 0,
			      "a correction applies to colour cartridges");
		}
	}
	check(strcmp(gb_video_option("gambatte_gbc_color_correction_mode", 0,
				     GB_COLOUR_FAST), "fast") == 0,
	      "Fast selects the fast mode");
	check(strcmp(gb_video_option("gambatte_gbc_color_correction_mode", 0,
				     GB_COLOUR_ACCURATE), "accurate") == 0,
	      "Accurate selects the accurate mode");

	/* 4. The palettes really are inert on a colour cartridge, and the
	 *    correction really is inert on a monochrome one: the core returns
	 *    before reading one or the other, so the page greys the palette.
	 *    Here that is only the reading of the code; the checksums show it in
	 *    tools/test_video_options.sh, where a colour cartridge gives one
	 *    value for every palette. */

	/* 5. Anything else is declined, so the core keeps its own default. */
	check(gb_video_option("gambatte_gb_bootloader", 0, 0) == NULL,
	      "an unknown key is declined");
	check(gb_video_option(NULL, 0, 0) == NULL, "a null key is declined");

	/* 6. An index past the table is treated as the default, not read out of
	 *    bounds: the settings blob comes from the host. */
	check(strcmp(gb_video_option("gambatte_gb_colorization", 200, 0),
		     "disabled") == 0,
	      "a palette past the table reads as Off");
	check(strcmp(gb_video_option("gambatte_gbc_color_correction", 0, 200),
		     "disabled") == 0,
	      "a colour past the table reads as Off");

	/* 7. The names the page shows exist for every index. */
	for (unsigned i = 0; i < GB_PAL_COUNT; i++) {
		check(gb_palette_names[i] != NULL, "every palette has a name");
	}
	for (unsigned i = 0; i < GB_COLOUR_COUNT; i++) {
		check(gb_colour_names[i] != NULL, "every colour setting has a name");
	}

	if (failures) {
		printf("TinyGBC VIDEO FAILED %u\n", failures);
		return 1;
	}
	printf("TinyGBC VIDEO PASS\n");
	return 0;
}
