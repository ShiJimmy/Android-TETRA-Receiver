/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Implementation of the minimal <osmocom/core/msgb.h> message buffer. */

#include <stdint.h>
#include <string.h>

#include <osmocom/core/msgb.h>

struct msgb *msgb_alloc_c(const void *ctx, uint16_t size, const char *name)
{
	struct msgb *msg = talloc_named_const(ctx, sizeof(*msg) + size, name);

	if (!msg)
		return NULL;

	/* libosmocore uses talloc_zero()/talloc_zero_size() here, so all
	 * header pointers (l1h/l2h/l3h/l4h) and the data area start out as
	 * NULL/zero.  Our compat talloc is plain malloc, so zero explicitly. */
	memset(msg, 0, sizeof(*msg) + size);

	msg->data_len = size;
	msg->len = 0;
	msg->head = msg->_data;
	msg->tail = msg->_data;
	msg->data = msg->_data;

	return msg;
}

struct msgb *msgb_alloc(uint16_t size, const char *name)
{
	return msgb_alloc_c(NULL, size, name);
}
