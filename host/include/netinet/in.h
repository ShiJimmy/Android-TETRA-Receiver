/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host-only shim: map <netinet/in.h> onto WinSock2 for MinGW builds. */
#ifndef TETRA_HOST_NETINET_IN_H
#define TETRA_HOST_NETINET_IN_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#endif /* TETRA_HOST_NETINET_IN_H */
