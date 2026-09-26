/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/talloc.h> replacement.
 *
 * osmo-tetra uses talloc only as a heap allocator (contexts are never walked
 * or freed hierarchically in the receive path), so the talloc API maps onto
 * malloc/calloc/free.  A small leak is acceptable here; the application
 * frees whole objects with talloc_free().
 */
#ifndef TETRA_ANDROID_OSMO_TALLOC_H
#define TETRA_ANDROID_OSMO_TALLOC_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef void TALLOC_CTX;

void *talloc_zero_size_(const void *ctx, size_t size);
void *talloc_realloc_size_(const void *ctx, void *ptr, size_t size);
void *talloc_named_const_(const void *ctx, size_t size, const char *name);

/*! allocate zero-initialised memory for one object of \a type */
#define talloc_zero(ctx, type) \
	((type *)talloc_zero_size_((ctx), sizeof(type)))
/*! allocate zero-initialised memory for \a count objects of \a type */
#define talloc_zero_array(ctx, type, count) \
	((type *)talloc_zero_size_((ctx), sizeof(type) * (count)))
/*! resize (or allocate) an array of \a count objects of \a type */
#define talloc_realloc(ctx, ptr, type, count) \
	((type *)talloc_realloc_size_((ctx), (ptr), sizeof(type) * (count)))
#define talloc_named_const(ctx, size, name) \
	(talloc_named_const_((ctx), (size), (name)))
#define talloc_new(ctx) talloc_named_const_((ctx), 1, "talloc_new")
#define talloc_size(ctx, size) talloc_named_const_((ctx), (size), __func__)
#define talloc_zero_size(ctx, size) talloc_zero_size_((ctx), (size))

/*! free memory previously obtained from talloc.
 *  \returns 0 (same as talloc) */
static inline int talloc_free(void *ptr)
{
	free(ptr);
	return 0;
}

static inline void *talloc_parent(const void *ctx)
{
	return NULL;
}

/* contexts are ignored by this implementation */
static inline void *talloc_ctx_init(void *ctx, const char *name)
{
	return ctx;
}

static inline void *talloc_ctx(const void *ctx)
{
	return (void *)ctx;
}

#endif /* TETRA_ANDROID_OSMO_TALLOC_H */
