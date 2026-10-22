/* SPDX-License-Identifier: MIT
 *
 * The boot ROM's side of the UART load protocol in tinygbc.h.
 */
#ifndef TINYGBC_LOAD_H
#define TINYGBC_LOAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forget any partial header and clear the receive error flags. */
void tn_load_reset(void);

/* Watch the UART for up to cycles core cycles. If a header starts, receive
 * the program it describes into the SDRAM, then print "TinyGBC RUN <entry>"
 * and return the entry once the line is sent; the caller must fence.i and
 * jump. Returns 0 if nothing was loaded. */
uint32_t tn_load_poll(uint32_t cycles);

#ifdef __cplusplus
}
#endif

#endif
