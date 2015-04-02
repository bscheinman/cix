#ifndef _CIX_BOOK_H
#define _CIX_BOOK_H

#include <inttypes.h>

#include "heap.h"
#include "id_generator.h"
#include "messages.h"
#include "trade_log.h"

struct cix_book {
	uint32_t recv_counter;
	struct cix_heap bid;
	struct cix_heap offer;
	cix_symbol_t symbol;
	struct cix_id_block id_block;

	struct cix_trade_log_manager trade_log;
};

struct cix_order;

bool cix_book_init(struct cix_book *, const char *);
bool cix_book_order(struct cix_book *, struct cix_order *);

#endif /* _CIX_BOOK_H */
