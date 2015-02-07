#include "order.h"

/*
 * XXX: Check for overflow
 */
static bool
cix_order_id_block_next(struct cix_order_id_block *block,
    struct cix_order_id_generator *generator)
{

	block->cursor = ck_pr_faa_64(&generator->cursor, generator->interval);
	block->finish = block->cursor + generator->interval;
	return true;
}

bool
cix_order_id_next(uint64_t *id, struct cix_order_id_generator *generator,
    struct cix_order_id_block *block)
{

	if (block->cursor == block->finish && 
	    cix_order_id_block_next(block, generator) == false) {
		return false;
	}

	/*
	 * Each block should only be accessed by a single thread
	 */
	*id = block->cursor++;
	return true;
}
