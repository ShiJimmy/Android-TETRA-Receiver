/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/*
 * Minimal libosmocore compatibility layer for the TETRA Android receiver.
 *
 * The TETRA decoder is taken verbatim from osmo-tetra-sq5bpf-2, which links
 * against libosmocore.  libosmocore is not available for Android (it drags in
 * vty/logging/talloc infrastructure), so this directory provides the small
 * subset that osmo-tetra actually uses:
 *
 *   <osmocom/core/linuxlist.h>  vendored verbatim  (tools/vendor_thirdparty.ps1)
 *   <osmocom/core/conv.h>       vendored verbatim  (tools/vendor_thirdparty.ps1)
 *   <osmocom/core/conv.c>       vendored verbatim  (tools/vendor_thirdparty.ps1)
 *   <osmocom/core/bits.h>       unpacked/soft bit types
 *   <osmocom/core/utils.h>      OSMO_ASSERT, ARRAY_SIZE, hex/bit dumps
 *   <osmocom/core/msgb.h>       minimal message buffer (msgb)
 *   <osmocom/core/prim.h>       osmo_prim_hdr
 *   <osmocom/core/talloc.h>     talloc_* macros mapped onto malloc/free
 *   <osmocom/core/panic.h>      osmo_panic()
 *   <osmocom/core/logging.h>    LOGP* no-op stubs
 *
 * Everything the TETRA code needs is implemented in src/osmo_compat.c and
 * src/msgb.c.
 */
#ifndef TETRA_ANDROID_OSMO_COMPAT_H
#define TETRA_ANDROID_OSMO_COMPAT_H

#include <stdio.h>
#include <stdlib.h>

#endif /* TETRA_ANDROID_OSMO_COMPAT_H */
