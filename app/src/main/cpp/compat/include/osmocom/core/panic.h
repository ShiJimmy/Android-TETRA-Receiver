/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/panic.h> replacement. */
#ifndef TETRA_ANDROID_OSMO_PANIC_H
#define TETRA_ANDROID_OSMO_PANIC_H

#include <stdio.h>
#include <stdlib.h>

/*! Called on unrecoverable errors. Implemented in compat/src/osmo_compat.c:
 *  it reports through the application log hook and aborts. */
void osmo_panic(const char *fmt, ...)
	__attribute__((format(printf, 1, 2), noreturn));

#define OSMO_PANIC(fmt, args...) osmo_panic(fmt, ##args)

#endif /* TETRA_ANDROID_OSMO_PANIC_H */
