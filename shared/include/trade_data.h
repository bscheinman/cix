#ifndef _CIX_TRADE_DATA_H
#define _CIX_TRADE_DATA_H

#include <inttypes.h>

#include "messages.h"

struct cix_execution {
	cix_execution_id_t id;
	cix_user_id_t buyer;
	cix_user_id_t seller;
	cix_symbol_t symbol;
	cix_quantity_t quantity;
	cix_price_t price;
};

struct cix_order {
	/*
	 * Use raw network type as header to enable reading
	 * directly into application struct.
	 */
	struct cix_message_order data;

	cix_order_id_t id;
	cix_user_id_t user;

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

#endif /* _CIX_TRADE_DATA_H */
