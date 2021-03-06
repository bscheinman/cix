#include <assert.h>
#include <ck_pr.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

#include "event.h"
#include "misc.h"
#include "worq.h"

#define CIX_WORQ_CACHE_LINE_MASK (CK_MD_CACHELINE - 1)

struct cix_worq_item {
	unsigned int ready;
	unsigned char data[];
};

bool
cix_worq_init(struct cix_worq *worq, size_t item_size, unsigned int length)
{
	size_t overage;

	assert((length & (length - 1)) == 0);

	worq->slot_size = item_size + sizeof(struct cix_worq_item);
	overage = worq->slot_size & CIX_WORQ_CACHE_LINE_MASK;
	if (overage > 0) {
		worq->slot_size += CK_MD_CACHELINE - overage;
	}

	worq->items = calloc(length, worq->slot_size);
	if (worq->items == NULL) {
		fprintf(stderr, "Failed to allocate queue buffer\n");
		return false;
	}

	worq->size = length;
	worq->mask = worq->size - 1;
	worq->consume_cursor = 0;
	worq->produce_cursor = 0;
	worq->event = NULL;

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
	uint64_t index;

	for (;;) {
		uint64_t end = ck_pr_load_64(&worq->consume_cursor);

		index = ck_pr_load_64(&worq->produce_cursor);

		/* Queue is full */
		if (index - end >= worq->size) {
			return NULL;
		}

		if (ck_pr_cas_64_value(&worq->produce_cursor, index,
		    index + 1, &index) == true) {
			break;
		}
	}

	index &= worq->mask;
	slot = (struct cix_worq_item *)(worq->items + index * worq->slot_size);

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

	if (worq->event != NULL && cix_event_managed_trigger(worq->event) ==
	    false) {
		fprintf(stderr, "failed to notify worq consumer\n");
	}

	return;
}

/*
 * This is not currently thread-safe because the queue is only intended
 * for single-consumer workloads.
 */
void *
cix_worq_pop(struct cix_worq *worq, enum cix_worq_wait wait)
{
	uint64_t index = worq->consume_cursor;
	struct cix_worq_item *slot;

	/* Wait for a producer to add a new item to the queue. */
	for (;;) {
		uint64_t last = ck_pr_load_64(&worq->produce_cursor);

		/* XXX: Deal with overflow */
		if (index < last) {
			break;
		}

		if (wait != CIX_WORQ_WAIT_BLOCK) {
			return NULL;
		}
	}

	/* Wait for producer to publish new item */
	index &= worq->mask;
	slot = (struct cix_worq_item *)(worq->items + index * worq->slot_size);

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

bool
cix_worq_event_subscribe(struct cix_worq *worq, struct cix_event *event)
{

	if (worq->event != NULL) {
		fprintf(stderr, "worq already has a subscriber\n");
		return false;
	}

	worq->event = event;
	return true;
}
