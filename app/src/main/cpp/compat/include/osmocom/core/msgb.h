/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/msgb.h> replacement.
 *
 * Implements the subset of the libosmocore message buffer API that the TETRA
 * decoder uses.  The struct layout and the semantics of the length accessors
 * follow libosmocore (msgb_lXlen() = tail - lXh), which is what the PPP/MAC
 * parsing code in osmo-tetra depends on.
 */
#ifndef TETRA_ANDROID_OSMO_MSGB_H
#define TETRA_ANDROID_OSMO_MSGB_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/defs.h>
#include <osmocom/core/panic.h>

struct msgb {
	struct llist_head list;

	unsigned char *l1h;	/*!< layer 1 header */
	unsigned char *l2h;	/*!< layer 2 header */
	unsigned char *l3h;	/*!< layer 3 header */
	unsigned char *l4h;	/*!< layer 4 header */

	unsigned long cb[5];	/*!< control buffer */

	uint16_t data_len;	/*!< size of the data area */
	uint16_t len;		/*!< number of bytes used */

	unsigned char *head;	/*!< start of the allocated data area */
	unsigned char *tail;	/*!< end of the message inside the data area */
	unsigned char *data;	/*!< start of the message */

	unsigned char _data[];	/*!< data area (flexible array member) */
};

#define msgb_l1(m) ((void *)((m)->l1h))
#define msgb_l2(m) ((void *)((m)->l2h))
#define msgb_l3(m) ((void *)((m)->l3h))
#define msgb_l4(m) ((void *)((m)->l4h))

#define msgb_length(m) ((m)->len)
#define msgb_data(m)   ((m)->data)
#define msgb_li(msgb, i) ...

static inline unsigned int msgb_l1len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l1h);
	return msgb->tail - (uint8_t *)msgb_l1(msgb);
}

static inline unsigned int msgb_l2len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l2h);
	return msgb->tail - (uint8_t *)msgb_l2(msgb);
}

static inline unsigned int msgb_l3len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l3h);
	return msgb->tail - (uint8_t *)msgb_l3(msgb);
}

static inline unsigned int msgb_l4len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l4h);
	return msgb->tail - (uint8_t *)msgb_l4(msgb);
}

static inline int msgb_tailroom(const struct msgb *msgb)
{
	return (msgb->head + msgb->data_len) - msgb->tail;
}

static inline int msgb_headroom(const struct msgb *msgb)
{
	return (msgb->data - msgb->head);
}

#define MSGB_ABORT(msg, fmt, args ...) do {				\
		fprintf(stderr, "msgb_abort(%s:%d): ", __FILE__, __LINE__);	\
		fprintf(stderr, fmt, ##args);				\
		osmo_panic("msgb_abort");				\
	} while (0)

static inline unsigned char *msgb_put(struct msgb *msgb, unsigned int len)
{
	unsigned char *tmp = msgb->tail;
	if (OSMO_UNLIKELY(msgb_tailroom(msgb) < (int)len))
		MSGB_ABORT(msgb, "Not enough tailroom msgb_put (len %u, want %u)\n",
			   msgb->len, len);
	msgb->tail += len;
	msgb->len += len;
	return tmp;
}

static inline unsigned char *msgb_get(struct msgb *msgb, unsigned int len)
{
	OSMO_ASSERT(msgb->len >= len);
	msgb->tail -= len;
	msgb->len -= len;
	return msgb->tail;
}

static inline unsigned char *msgb_push(struct msgb *msgb, unsigned int len)
{
	OSMO_ASSERT(msgb_headroom(msgb) >= (int)len);
	msgb->data -= len;
	msgb->len += len;
	return msgb->data;
}

static inline unsigned char *msgb_pull(struct msgb *msgb, unsigned int len)
{
	OSMO_ASSERT(msgb->len >= len);
	msgb->len -= len;
	return msgb->data += len;
}

static inline unsigned char *msgb_pull_to_l3(struct msgb *msg)
{
	return msgb_pull(msg, msg->l3h - msg->data);
}

static inline unsigned char *msgb_pull_to_l2(struct msgb *msg)
{
	return msgb_pull(msg, msg->l2h - msg->data);
}

static inline void msgb_reserve(struct msgb *msg, int len)
{
	msg->data += len;
	msg->tail += len;
}

static inline int msgb_trim(struct msgb *msg, int len)
{
	OSMO_ASSERT(len >= 0);
	OSMO_ASSERT(len <= msg->data_len);
	msg->len = len;
	msg->tail = msg->data + len;
	return len;
}

static inline int msgb_l3trim(struct msgb *msg, int l3len)
{
	return msgb_trim(msg, (msg->l3h - msg->data) + l3len);
}

static inline void msgb_put_u8(struct msgb *msgb, uint8_t word)
{
	uint8_t *space = msgb_put(msgb, 1);
	space[0] = word & 0xFF;
}

struct msgb *msgb_alloc_c(const void *ctx, uint16_t size, const char *name);
struct msgb *msgb_alloc(uint16_t size, const char *name);
static inline void msgb_free(struct msgb *m) { talloc_free(m); }

static inline struct msgb *msgb_alloc_headroom(uint16_t size, uint16_t headroom,
					       const char *name)
{
	struct msgb *msg = msgb_alloc_c(NULL, size + headroom, name);
	if (OSMO_LIKELY(msg))
		msgb_reserve(msg, headroom);
	return msg;
}

#endif /* TETRA_ANDROID_OSMO_MSGB_H */
