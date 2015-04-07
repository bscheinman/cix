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

	/* XXX: Have separate id blocks for order and trade IDs */
	struct cix_id_block id_block;

	struct cix_trade_log_manager trade_log;
};

struct cix_message_order;
struct cix_session;

bool cix_book_init(struct cix_book *, cix_symbol_t *);
void cix_book_destroy(struct cix_book *);

bool cix_book_order(struct cix_book *, struct cix_message_order *,
    struct cix_session *);

#endif /* _CIX_BOOK_H */
