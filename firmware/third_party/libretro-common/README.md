# libretro-common, in part

Three files from [libretro-common](https://github.com/libretro/libretro-common),
unmodified, MIT (RetroArch team; each file carries its own licence header):

- `encodings/encoding_deflate.c` — raw DEFLATE, `rinflate_*`
- `include/encodings/deflate.h`
- `include/retro_endianness.h`

They are here because a cartridge may arrive as a `.zip` and gambatte's own
copy of libretro-common is trimmed to the pieces the core draws on. TinyNES
uses the same decoder for the same job, from the copy that
`libretro/libretro-fceumm` carries; this one was taken from that copy at
FCEUmm commit `ac0d00a7fc01166f84c7bac01d1084ebfd2b865e`, which is what
TinyNES's `tools/fetch_fceumm.sh` pins.

`retro_inline.h` and `retro_common_api.h` are deliberately not duplicated:
the other two headers include them, and the copies already in gambatte's
fetched `libretro-common/include` are in the include path, with the macros
these files use and nothing they would collide with.

Nothing else from libretro-common is copied. `firmware/common/cart_zip.c`,
the zip reader built on this decoder, is this tree's own.
