/* SPDX-License-Identifier: MIT
 *
 * The savestate named by STATE= at build time, copied to GB_STATE_FILE. It is
 * empty when STATE is not set, and gb_bench then skips loading it. GNU as on
 * both the AE350 and the PC build.
 */
	.section .rodata.gb_state, "a"
	.balign 16
	.globl gb_state
	.globl gb_state_end
gb_state:
	.incbin GB_STATE_FILE
gb_state_end:
	.section .note.GNU-stack, "", @progbits
