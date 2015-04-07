#include <assert.h>
#include <ck_pr.h>
#include <stdlib.h>
#include <stdio.h>

#include "misc.h"
#include "worq.h"

/* XXX: Ensure we have the correct size here */
#define CIX_WORQ_CACHE_LINE 64
#define CIX_WORQ_CACHE_LINE_MASK (CIX_WORQ_CACHE_LINE - 1)

struct cix_worq_item {
	unsigned int ready;
	unsigned char data[];
};

bool
cix_worq_init(struct cix_worq *worq, size_t item_size, unsigned int length)
{
	size_t overage;

	assert((length & (length - 1)) == 0);

	worq->slot_size = item_size + sizeof(unsigned int);
	overage = worq->slot_size & CIX_WORQ_CACHE_LINE_MASK;
	worq->slot_size += CIX_WORQ_CACHE_LINE - overage;

	worq->items = calloc(length, worq->slot_size);
	if (worq->items == NULL) {
		fprintf(stderr, "Failed to allocate queue buffer\n");
		return false;
	}

	worq->size = length;
	worq->mask = length - 1;
	worq->consume_cursor = 0;
	worq->produce_cursor = 0;

	return true;
}

void
cix_worq_destroy(struct cix_worq *worq)
{

	free(worq->items);
	return;
}

void *
cix_worq_claim(struct cix_worq *worq)
{
	struct cix_worq_item *slot;
	unsigned int index;

	for (;;) {
		unsigned int end = worq->consume_cursor;

		index = ck_pr_load_uint(&worq->produce_cursor);

		/* Queue is full */
		if (index - end >= worq->size) {
			return NULL;
		}

		if (ck_pr_cas_uint_value(&worq->produce_cursor, index,
		    index + 1, &index) == true) {
			break;
		}
	}
	
	slot = &worq->items[index & worq->mask];

	return slot->data;
}

void
cix_worq_publish(struct cix_worq *worq, void *data)
{
	struct cix_worq_item *slot =
	    container_of(data, struct cix_worq_item, data);

	(void)worq;

	/* Make sure that all producer data was written before publishing. */
	ck_pr_fence_release();
	slot->ready = 1;

	return;
}

/*
 * This is not currently thread-safe because the queue is only intended
 * for single-consumer workloads.
 */
void *
cix_worq_pop(struct cix_worq *worq, enum cix_worq_wait wait)
{
	unsigned int index = worq->consume_cursor;
	struct cix_worq_item *slot;

	/* Wait for a producer to add a new item to the queue. */
	for (;;) {
		unsigned int last = ck_pr_load_uint(&worq->produce_cursor);

		/* XXX: Deal with overflow */
		if (index < last) {
			break;
		}

		if (wait != CIX_WORQ_WAIT_BLOCK) {
			return NULL;
		}
	}

	/* Wait for producer to publish new item */
	slot = &worq->items[index & worq->mask];

	for (;;) {
		unsigned int ready = ck_pr_load_uint(&slot->ready);

		if (ready != 0) {
			ck_pr_fence_acquire();
			return slot->data;
		}

		if (wait == CIX_WORQ_WAIT_NONBLOCK) {
			return NULL;
		}
	}

	return NULL;
}

void
cix_worq_complete(struct cix_worq *worq, void *data)
{
	struct cix_worq_item *slot =
	    container_of(data, struct cix_worq_item, data);

	slot->ready = 0;

	/*
	 * If we change to support multiple consumer threads then we will
	 * need a store fence here.
	 */

	++worq->consume_cursor;
	return;
}
