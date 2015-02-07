#ifndef _CIX_ORDER_H
#define _CIX_ORDER_H

#include "messages.h"

struct cix_order_id_generator {
	uint64_t cursor;
	uint64_t interval;
};
typedef struct cix_order_id_generator cix_order_id_generator_t;

struct cix_order_id_block {
	uint64_t cursor;
	uint64_t finish;
};
typedef struct cix_order_id_block cix_order_id_block_t;

#define CIX_ORDER_ID_BLOCK_INITIALIZER { .cursor = 0, .finish = 0 }

bool cix_order_id_next(cix_order_id_t *, cix_order_id_generator_t *,
    cix_order_id_block_t *);

struct cix_order {
	/*
	 * Use raw network type as header to enable reading
	 * directly into application struct.
	 */
	struct cix_message_order data;

	/*
	 * This is a monotonically increasing value assigned by the order
	 * book.  It does not represent any specific time value, but it is
	 * guaranteed that all orders for a given symbol will be properly
	 * ordered by this value.
	 */
	uint64_t recv_time;
	
	/*
	 * Shares remaining to be executed.  Once this reaches 0, the
	 * entire order has been traded and should be removed from
	 * the order book.
	 */
	cix_quantity_t remaining;
};

#endif /* _CIX_ORDER_H */
