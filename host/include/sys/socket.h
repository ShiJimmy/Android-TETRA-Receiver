/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Host-only shim: map <sys/socket.h> onto WinSock2 for MinGW builds.
 *
 * The vendored osmo-tetra decoder includes POSIX socket headers and calls
 * sendto() to emit telive monitoring messages.  On Android these resolve to
 * the real libc socket API; on the MinGW host build this shim provides the
 * declarations via WinSock2 so the same sources compile unchanged.
 */
#ifndef TETRA_HOST_SYS_SOCKET_H
#define TETRA_HOST_SYS_SOCKET_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>

#endif /* TETRA_HOST_SYS_SOCKET_H */
