/* SPDX-License-Identifier: MIT
 *
 * The cartridge image named by ROM= at build time, copied to GB_ROM_FILE.
 * GNU as on both the AE350 and the PC build.
 */
	.section .rodata.gb_rom, "a"
	.balign 16
	.globl gb_rom
	.globl gb_rom_end
gb_rom:
	.incbin GB_ROM_FILE
gb_rom_end:
	.section .note.GNU-stack, "", @progbits
