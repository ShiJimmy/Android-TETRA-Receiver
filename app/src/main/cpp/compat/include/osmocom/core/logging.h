/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/logging.h> replacement: the TETRA decoder does not use
 * libosmocore logging (it uses printf/DEBUGP), but include it for safety. */
#ifndef TETRA_ANDROID_OSMO_LOGGING_H
#define TETRA_ANDROID_OSMO_LOGGING_H

#include <stdarg.h>

enum log_level {
	LOGL_FATAL = 0,
	LOGL_ERROR,
	LOGL_NOTICE,
	LOGL_INFO,
	LOGL_DEBUG,
};

/*! application log hook, installed by the engine (may be NULL) */
typedef void (*osmo_log_fn)(int level, const char *fmt, va_list ap);
extern osmo_log_fn osmo_log_hook;

void osmo_logp(int level, const char *file, int line, const char *fmt, ...);

#define LOGP(ss, level, fmt, args...) \
	osmo_logp(level, __FILE__, __LINE__, fmt, ##args)
#define LOGPC(ss, level, fmt, args...) LOGP(ss, level, fmt, ##args)
#define DEBUGP(ss, fmt, args...) do { } while (0)

#endif /* TETRA_ANDROID_OSMO_LOGGING_H */
