/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Implementation of the minimal libosmocore compatibility layer.
 *
 * See the headers under compat/include/osmocom/core for the interfaces.
 * This file provides the small set of runtime helpers that the vendored
 * osmo-tetra decoder relies on, implemented with zero external dependencies
 * (plain libc only) so that it builds unchanged on Android and on the host.
 */

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <osmocom/core/bits.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/panic.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/talloc.h>

/* --------------------------------------------------------------------- */
/* packed / unpacked bit conversion                                      */
/* --------------------------------------------------------------------- */

int osmo_ubit2pbit(pbit_t *out, const ubit_t *in, unsigned int num_bits)
{
	unsigned int i;
	uint8_t curbyte = 0;
	pbit_t *outptr = out;

	for (i = 0; i < num_bits; i++) {
		curbyte |= (uint8_t)((in[i] & 1) << (7 - (i % 8)));
		if ((i % 8) == 7) {
			*outptr++ = curbyte;
			curbyte = 0;
		}
	}
	/* a non-modulo-8 bit count still produces a final partial byte */
	if (num_bits % 8)
		*outptr++ = curbyte;

	return (int)(outptr - out);
}

int osmo_pbit2ubit(ubit_t *out, const pbit_t *in, unsigned int num_bits)
{
	unsigned int i;

	for (i = 0; i < num_bits; i++)
		out[i] = (in[i / 8] >> (7 - (i % 8))) & 1;

	return (int)num_bits;
}

/* --------------------------------------------------------------------- */
/* big-endian load/store helpers                                         */
/* --------------------------------------------------------------------- */

uint16_t osmo_load16be(const uint8_t *p)
{
	return (uint16_t)((uint16_t)p[0] << 8) | p[1];
}

uint32_t osmo_load32be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

void osmo_store16be(uint16_t v, uint8_t *p)
{
	p[0] = (uint8_t)(v >> 8);
	p[1] = (uint8_t)(v & 0xff);
}

void osmo_store32be(uint32_t v, uint8_t *p)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)(v & 0xff);
}

/* --------------------------------------------------------------------- */
/* dumps                                                                 */
/* --------------------------------------------------------------------- */

#define DUMP_BUF_COUNT 16
#define DUMP_BUF_SIZE  4096

char *osmo_ubit_dump(const uint8_t *bits, unsigned int len)
{
	static char bufs[DUMP_BUF_COUNT][DUMP_BUF_SIZE];
	static unsigned int idx;
	char *buf;
	unsigned int i;

	if (len > DUMP_BUF_SIZE - 1)
		len = DUMP_BUF_SIZE - 1;

	idx = (idx + 1) % DUMP_BUF_COUNT;
	buf = bufs[idx];

	for (i = 0; i < len; i++)
		buf[i] = bits[i] ? '1' : '0';
	buf[len] = '\0';

	return buf;
}

static char *hexdump_common(const unsigned char *buf, int len, int with_space)
{
	static char bufs[DUMP_BUF_COUNT][DUMP_BUF_SIZE];
	static unsigned int idx;
	char *out;
	int i, o = 0;
	static const char hex[] = "0123456789abcdef";

	/* each byte: 2 hex chars + 1 space */
	if (len < 0)
		len = 0;
	if ((size_t)len > (DUMP_BUF_SIZE - 1) / 3)
		len = (DUMP_BUF_SIZE - 1) / 3;

	idx = (idx + 1) % DUMP_BUF_COUNT;
	out = bufs[idx];

	for (i = 0; i < len; i++) {
		if (with_space && i)
			out[o++] = ' ';
		out[o++] = hex[buf[i] >> 4];
		out[o++] = hex[buf[i] & 0xf];
	}
	out[o] = '\0';

	return out;
}

char *osmo_hexdump(const unsigned char *buf, int len)
{
	return hexdump_common(buf, len, 1);
}

char *osmo_hexdump_nospc(const unsigned char *buf, int len)
{
	return hexdump_common(buf, len, 0);
}

/* --------------------------------------------------------------------- */
/* value_string                                                          */
/* --------------------------------------------------------------------- */

const char *get_value_string(const struct value_string *vs, uint32_t val)
{
	for (; vs->str; vs++) {
		if (vs->value == val)
			return vs->str;
	}
	return NULL;
}

/* --------------------------------------------------------------------- */
/* panic / logging                                                       */
/* --------------------------------------------------------------------- */

void osmo_panic(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "osmo_panic: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);

	abort();
}

osmo_log_fn osmo_log_hook;

void osmo_logp(int level, const char *file, int line, const char *fmt, ...)
{
	(void)level;
	(void)file;
	(void)line;
	if (osmo_log_hook) {
		va_list ap;
		va_start(ap, fmt);
		osmo_log_hook(level, fmt, ap);
		va_end(ap);
	}
}

/* --------------------------------------------------------------------- */
/* talloc bridge (maps onto malloc/calloc/realloc/free)                  */
/* --------------------------------------------------------------------- */

void *talloc_zero_size_(const void *ctx, size_t size)
{
	(void)ctx;
	return calloc(1, size ? size : 1);
}

void *talloc_realloc_size_(const void *ctx, void *ptr, size_t size)
{
	(void)ctx;
	return realloc(ptr, size ? size : 1);
}

void *talloc_named_const_(const void *ctx, size_t size, const char *name)
{
	(void)ctx;
	(void)name;
	return malloc(size ? size : 1);
}
