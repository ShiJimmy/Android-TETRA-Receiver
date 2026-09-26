/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/bits.h> replacement.
 *
 * libosmocore generates bit16gen.h / bit32gen.h / bit64gen.h at build time,
 * so the upstream header can not be dropped in as-is.  Only the bit type
 * definitions and the two pack/unpack helpers used by osmo-tetra are needed.
 */
#ifndef TETRA_ANDROID_OSMO_BITS_H
#define TETRA_ANDROID_OSMO_BITS_H

#include <stdint.h>
#include <stddef.h>

/*! unpacked bit: one byte per bit, value 0 or 1 */
typedef uint8_t ubit_t;
/*! packed bits: 8 bits per byte, MSB first */
typedef uint8_t pbit_t;
/*! soft bit: -1..-127 = "1", 0 = erasure, 1..127 = "0" */
typedef int8_t sbit_t;

/*! convert unpacked bits (ubit_t) into packed bits (pbit_t).
 *  \returns number of packed bytes written */
int osmo_ubit2pbit(pbit_t *out, const ubit_t *in, unsigned int num_bits);

/*! convert packed bits (pbit_t) into unpacked bits (ubit_t).
 *  \returns number of unpacked bits written */
int osmo_pbit2ubit(ubit_t *out, const pbit_t *in, unsigned int num_bits);

#endif /* TETRA_ANDROID_OSMO_BITS_H */
