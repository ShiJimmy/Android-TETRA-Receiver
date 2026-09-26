/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/defs.h> replacement. */
#ifndef TETRA_ANDROID_OSMO_DEFS_H
#define TETRA_ANDROID_OSMO_DEFS_H

/* osmo-tetra uses the non-standard `uint' alias in a few places */
#ifndef uint
typedef unsigned int uint;
#endif

#ifndef __has_attribute
#  define __has_attribute(x) 0
#endif

#if defined(__GNUC__)
#  define OSMO_DEPRECATED(text) __attribute__((deprecated(text)))
#  define __visibility__(x) __attribute__((visibility(x)))
#  define OSMO_UNUSED __attribute__((unused))
#else
#  define OSMO_DEPRECATED(text)
#  define __visibility__(x)
#  define OSMO_UNUSED
#endif

#endif /* TETRA_ANDROID_OSMO_DEFS_H */
