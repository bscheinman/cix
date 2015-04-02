#include <ck_pr.h>
#include <inttypes.h>
#include <stdbool.h>

#include "id_generator.h"

void
cix_id_generator_init(struct cix_id_generator *gen, uint64_t interval)
{

	gen->cursor = 0;
	gen->interval = interval;
	return;
}

void
cix_id_block_init(struct cix_id_block *block)
{

	block->cursor = 0;
	block->finish = 0;
	return;
}

/*
 * XXX: Check for overflow
 */
static bool
cix_id_block_next(struct cix_id_block *block,
    struct cix_id_generator *generator)
{

	block->cursor = ck_pr_faa_64(&generator->cursor, generator->interval);
	block->finish = block->cursor + generator->interval;
	return true;
}

bool
cix_id_next(struct cix_id_generator *generator,
    struct cix_id_block *block, uint64_t *id)
{

	if (block->cursor == block->finish && 
	    cix_id_block_next(block, generator) == false) {
		return false;
	}

	/*
	 * Each block should only be accessed by a single thread
	 */
	*id = block->cursor++;
	return true;
}
