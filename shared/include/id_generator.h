#ifndef _CIX_ORDER_H
#define _CIX_ORDER_H

#include <inttypes.h>
#include <stdbool.h>

#include "messages.h"

struct cix_id_generator {
	uint64_t cursor;
	uint64_t interval;
};

struct cix_id_block {
	uint64_t cursor;
	uint64_t finish;
};

#define CIX_ID_BLOCK_INITIALIZER { .cursor = 0, .finish = 0 }
#define CIX_ID_GENERATOR_INITIALIZER(I) { .cursor = 0, .interval = (I) }

void cix_id_generator_init(struct cix_id_generator *, uint64_t);
void cix_id_block_init(struct cix_id_block *);
bool cix_id_next(struct cix_id_generator *, struct cix_id_block *, uint64_t *);

#endif /* _CIX_ORDER_H */
