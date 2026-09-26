/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/prim.h> replacement: primitive SAP header used by the
 * TETRA upper/lower MAC interfaces. */
#ifndef TETRA_ANDROID_OSMO_PRIM_H
#define TETRA_ANDROID_OSMO_PRIM_H

#include <stdint.h>

#include <osmocom/core/msgb.h>

/*! primitive operation (from libosmocore) */
enum osmo_prim_operation {
	PRIM_OP_REQUEST,
	PRIM_OP_RESPONSE,
	PRIM_OP_INDICATION,
	PRIM_OP_CONFIRM,
};

/*! generic primitive header */
struct osmo_prim_hdr {
	unsigned int sap;
	unsigned int primitive;
	enum osmo_prim_operation operation;
	struct msgb *msg;
};

static inline void osmo_prim_init(struct osmo_prim_hdr *oph, unsigned int sap,
				  unsigned int primitive,
				  enum osmo_prim_operation operation,
				  struct msgb *msg)
{
	oph->sap = sap;
	oph->primitive = primitive;
	oph->operation = operation;
	oph->msg = msg;
}

#endif /* TETRA_ANDROID_OSMO_PRIM_H */
