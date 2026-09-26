/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (C) 2026 Shi Jimmy
 */
/* Minimal <osmocom/core/linuxlist.h> replacement.
 *
 * Provides the tiny subset of the Linux kernel doubly-linked list that the
 * TETRA decoder uses: struct llist_head, INIT_LLIST_HEAD, LLIST_HEAD_INIT,
 * llist_add, llist_del and llist_for_each_entry.  Semantics match the kernel
 * list (libosmocore vendors linuxlist.h verbatim).
 */
#ifndef TETRA_ANDROID_OSMO_LINUXLIST_H
#define TETRA_ANDROID_OSMO_LINUXLIST_H

#include <stddef.h>
#include <stdbool.h>

struct llist_head {
	struct llist_head *next, *prev;
};

#define LLIST_HEAD_INIT(name) { &(name), &(name) }
#define LLIST_HEAD(name) \
	struct llist_head name = LLIST_HEAD_INIT(name)

#define INIT_LLIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

#define llist_entry(ptr, type, member) container_of(ptr, type, member)

static inline void __llist_add(struct llist_head *new,
			       struct llist_head *prev,
			       struct llist_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void llist_add(struct llist_head *new, struct llist_head *head)
{
	__llist_add(new, head, head->next);
}

static inline void llist_del(struct llist_head *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = entry;
	entry->prev = entry;
}

#define llist_for_each_entry(pos, head, member)				\
	for (pos = llist_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = llist_entry(pos->member.next, typeof(*pos), member))

#endif /* TETRA_ANDROID_OSMO_LINUXLIST_H */
