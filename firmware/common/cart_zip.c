/* SPDX-License-Identifier: MIT
 *
 * A cartridge sent as a .zip: find its one .gb or .gbc entry through the
 * zip's central directory and inflate it into a buffer of its own, with
 * libretro-common's raw DEFLATE decoder (encodings/encoding_deflate.c, MIT,
 * vendored under firmware/third_party/libretro-common). The central directory
 * is read rather than the local header because a zip written with a data
 * descriptor leaves the local header's sizes and CRC at zero; the CRC-32
 * stored there is checked against what was inflated.
 *
 * This is TinyNES's cart_zip.c with its checks kept and its cartridge test
 * widened from .nes to the two extensions a Game Boy image uses. It has no
 * dependency on the core, so the PC test tools/test_cart_zip.sh builds it
 * unchanged.
 */
#include "cart_zip.h"

#include <stdlib.h>
#include <string.h>

#include <encodings/deflate.h>

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | le16(p + 2) << 16; }

static uint32_t crc32_of(const uint8_t *p, size_t n)
{
	uint32_t crc = 0xffffffffu;
	for (size_t i = 0; i < n; i++) {
		crc ^= p[i];
		for (int k = 0; k < 8; k++)
			crc = crc >> 1 ^ (0xedb88320u & (0u - (crc & 1)));
	}
	return ~crc;
}

static int ends_with(const uint8_t *name, size_t n, const char *ext)
{
	const size_t m = strlen(ext);

	if (n < m)
		return 0;
	for (size_t i = 0; i < m; i++)
		if ((name[n - m + i] | 0x20) != (uint8_t)ext[i])
			return 0;
	return 1;
}

/* A Game Boy image, by extension: .gbc or .gb, in either case. */
static int ends_gameboy(const uint8_t *name, size_t n)
{
	return ends_with(name, n, ".gbc") || ends_with(name, n, ".gb");
}

bool cart_is_zip(const uint8_t *data, size_t len)
{
	return len >= 4 && le32(data) == 0x04034b50u;
}

const char *cart_unzip(const uint8_t *zip, size_t len, size_t max,
		       uint8_t **out, size_t *out_len, char *name, size_t name_size)
{
	/* The end-of-central-directory record: 22 bytes, at most 64 KiB of
	 * comment after it. */
	size_t eocd = (size_t)-1;
	for (size_t i = len >= 22 ? len - 22 : 0; len >= 22; i--) {
		if (le32(zip + i) == 0x06054b50u) {
			eocd = i;
			break;
		}
		if (i == 0 || len - i > 22 + 65535)
			break;
	}
	if (eocd == (size_t)-1)
		return "no end of central directory";
	const uint32_t entries = le16(zip + eocd + 10);
	const uint32_t cd_size = le32(zip + eocd + 12);
	const uint32_t cd_at = le32(zip + eocd + 16);
	if ((uint64_t)cd_at + cd_size > eocd)
		return "central directory out of range";

	/* The one cartridge entry. */
	const uint8_t *found = NULL;
	size_t at = cd_at;
	for (uint32_t e = 0; e < entries; e++) {
		if (at + 46 > eocd || le32(zip + at) != 0x02014b50u)
			return "bad central directory entry";
		const size_t n = le16(zip + at + 28);
		const size_t skip = 46 + n + le16(zip + at + 30) + le16(zip + at + 32);
		if (at + skip > eocd)
			return "bad central directory entry";
		if (ends_gameboy(zip + at + 46, n)) {
			if (found)
				return "more than one cartridge inside";
			found = zip + at;
		}
		at += skip;
	}
	if (!found)
		return "no .gb or .gbc inside";

	const uint32_t flags = le16(found + 8), method = le16(found + 10);
	const uint32_t crc = le32(found + 16), packed = le32(found + 20);
	const uint32_t size = le32(found + 24), local = le32(found + 42);
	const size_t n = le16(found + 28);
	if (flags & 1)
		return "encrypted";
	if (method != 0 && method != 8)
		return "not stored or deflated";
	if (size == 0 || size > max)
		return "too large";
	if ((uint64_t)local + 30 > len || le32(zip + local) != 0x04034b50u)
		return "bad local header";
	const size_t data = local + 30 + le16(zip + local + 26) + le16(zip + local + 28);
	if ((uint64_t)data + packed > len)
		return "entry out of range";

	uint8_t *buf = (uint8_t *)malloc(size);
	if (!buf)
		return "out of memory";
	size_t got = 0;
	if (method == 0) {
		if (packed != size) {
			free(buf);
			return "stored sizes differ";
		}
		memcpy(buf, zip + data, size);
		got = size;
	} else {
		void *z = rinflate_new(-15);	/* raw DEFLATE, as a zip entry holds */
		if (!z) {
			free(buf);
			return "out of memory";
		}
		rinflate_set_in(z, zip + data, packed);
		rinflate_set_out(z, buf, size);
		/* It may stop to ask for more room or input before the end; with
		 * both given whole, it ends or stops making progress. */
		int rc;
		do {
			size_t read = 0, wrote = 0;
			rc = rinflate_process(z, &read, &wrote);
			got += wrote;
			if (!read && !wrote)
				break;
		} while (rc == RDEFLATE_PROCESS_NEXT);
		rinflate_free(z);
		if (rc != RDEFLATE_PROCESS_END) {
			free(buf);
			return "inflate failed";
		}
	}
	if (got != size || crc32_of(buf, size) != crc) {
		free(buf);
		return got != size ? "inflated size differs" : "CRC-32 differs";
	}
	if (name && name_size) {
		const size_t k = n < name_size - 1 ? n : name_size - 1;
		memcpy(name, found + 46, k);
		name[k] = 0;
	}
	*out = buf;
	*out_len = size;
	return NULL;
}
