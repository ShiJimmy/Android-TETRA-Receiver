/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host-only shim: provide <linux/limits.h> for MinGW builds. */
#ifndef TETRA_HOST_LINUX_LIMITS_H
#define TETRA_HOST_LINUX_LIMITS_H

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#endif /* TETRA_HOST_LINUX_LIMITS_H */
