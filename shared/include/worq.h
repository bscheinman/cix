#ifndef _CIX_WORQ_H
#define _CIX_WORQ_H

#include <stdbool.h>
#include <stdlib.h>

enum cix_worq_wait {
	/* Return immediately upon any delay */
	CIX_WORQ_WAIT_NONBLOCK,

	/*
	 * Return immediately if the queue is empty, but block if waiting
	 * on an already-claimed slot to be published.
	 */
	CIX_WORQ_WAIT_BLOCK_SLOT,

	/* Block until data is ready */
	CIX_WORQ_WAIT_BLOCK
};

/*
 * Work queue based loosely on LMAX disruptor pattern.
 * Current implementation is intended for multi-producer, single-consumer
 * workloads.  Producers are lock-free, but consumers can be blocked by
 * producer threads between the time that the producer claims a slot and the
 * time that they finish writing to it.
 */

struct cix_worq_item;

struct cix_worq {
	struct cix_worq_item *items;
	size_t slot_size;
	unsigned int size;
	unsigned int mask;
	unsigned int consume_cursor;
	unsigned int produce_cursor;
};

bool cix_worq_init(struct cix_worq *, size_t, unsigned int);
void cix_worq_destroy(struct cix_worq *);

/*
 * Reserve slot for producer to write data.
 */
void *cix_worq_claim(struct cix_worq *);

/*
 * Mark a slot as written and ready for consumer.
 */
void cix_worq_publish(struct cix_worq *, void *);

/*
 * Consume the last element in the queue.
 */
void *cix_worq_pop(struct cix_worq *, enum cix_worq_wait);

/*
 * Mark consumed slot as processed and ready for reuse.
 */
void cix_worq_complete(struct cix_worq *, void *);

#endif /* _CIX_WORQ_H */
