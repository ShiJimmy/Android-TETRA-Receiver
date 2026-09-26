/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/*
 * Release-only: compile out the diagnostic printf()/fprintf() calls that are
 * scattered through the vendored osmo-tetra decoder and the DSP engine.
 *
 * This header is force-included (-include) at the very top of every
 * translation unit for the TETRA targets when TETRA_DEBUG=0.  It MUST pull in
 * <stdio.h> first so the real declaration is processed before the macros are
 * defined (a later, guarded #include <stdio.h> then becomes a no-op).
 *
 * Only the pure diagnostics are disabled.  Buffered formatters such as
 * snprintf()/sprintf(), and perror(), are deliberately left intact.
 */
#ifndef TETRA_SILENCE_DEBUG_H
#define TETRA_SILENCE_DEBUG_H

#include <stdio.h>

#if !defined(TETRA_DEBUG) || !TETRA_DEBUG

#define printf(...)    ((void)0)
#define fprintf(...)   ((void)0)
#define vprintf(...)   ((void)0)
#define vfprintf(...)  ((void)0)
#define puts(...)      ((void)0)
#define fputs(...)     ((void)0)
#define putchar(...)   ((void)0)
#define fputc(...)     ((void)0)
#define fflush(...)    ((void)0)

#endif /* !TETRA_DEBUG */

#endif /* TETRA_SILENCE_DEBUG_H */
