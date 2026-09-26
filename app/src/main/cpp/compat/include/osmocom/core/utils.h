/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/utils.h> replacement. */
#ifndef TETRA_ANDROID_OSMO_UTILS_H
#define TETRA_ANDROID_OSMO_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include <osmocom/core/panic.h>
#include <osmocom/core/defs.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/talloc.h>

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#if defined(__GNUC__)
#  define OSMO_LIKELY(exp)   __builtin_expect(!!(exp), 1)
#  define OSMO_UNLIKELY(exp) __builtin_expect(!!(exp), 0)
#else
#  define OSMO_LIKELY(exp)   (exp)
#  define OSMO_UNLIKELY(exp) (exp)
#endif

/*! assert that only fires in debug builds; here it always fires, which is what
 *  the decoder relies on (it is compiled without NDEBUG) */
#define OSMO_ASSERT(exp) do {						\
		if (OSMO_UNLIKELY(!(exp)))				\
			osmo_panic("Assert failed %s %s:%d\n",		\
				   #exp, __FILE__, __LINE__);		\
	} while (0)

/*! dump an array of unpacked bits as an ASCII string ('0'/'1').
 *  Uses a small ring of static buffers like libosmocore. */
char *osmo_ubit_dump(const uint8_t *bits, unsigned int len);

/*! hexdump with one space between octets */
char *osmo_hexdump(const unsigned char *buf, int len);
/*! hexdump without separators */
char *osmo_hexdump_nospc(const unsigned char *buf, int len);

/* big endian load/store helpers (used by the msgb helpers) */
uint16_t osmo_load16be(const uint8_t *p);
uint32_t osmo_load32be(const uint8_t *p);
void osmo_store16be(uint16_t v, uint8_t *p);
void osmo_store32be(uint32_t v, uint8_t *p);

/*! value / string pair used for pretty-printing enumerated values */
struct value_string {
	unsigned int value;
	const char *str;
};

/*! look up the string for \a val in a NULL-terminated value_string array */
const char *get_value_string(const struct value_string *vs, uint32_t val);

#endif /* TETRA_ANDROID_OSMO_UTILS_H */
