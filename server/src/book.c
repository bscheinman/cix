#include <inttypes.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "book.h"
#include "id_generator.h"
#include "messages.h"
#include "session.h"
#include "trade_data.h"
#include "trade_log.h"

#define CIX_BOOK_DEFAULT_HEAP_SIZE	(1 << 8)

#define CIX_BOOK_BUY_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((~((O)->recv_time)) & (((uint64_t)1 << 32) - 1)))
#define CIX_BOOK_SELL_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((O)->recv_time & (((uint64_t)1 << 32) - 1)))

struct cix_order {
	struct cix_message_order data;
	cix_order_id_t id;

	/*
	 * Session that received this order.  Used for sending acks,
	 * execution notifications, etc.
	 */
	struct cix_session *session;
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

static struct cix_id_generator cix_exec_id_gen =
    CIX_ID_GENERATOR_INITIALIZER(1 << 14);

bool
cix_book_init(struct cix_book *book, cix_symbol_t *symbol,
    struct cix_trade_log_manager *trade_log)
{

	strncpy(book->symbol.symbol, symbol->symbol,
	    sizeof book->symbol.symbol);
	if (book->symbol.symbol[sizeof(book->symbol.symbol) - 1] != '\0') {
		fprintf(stderr, "symbol %s exceeds maximum length\n",
		    symbol->symbol);
		return false;
	}

	if (cix_heap_init(&book->bid, false,
	    CIX_BOOK_DEFAULT_HEAP_SIZE) == false) {
		return false;
	}

	if (cix_heap_init(&book->offer, true,
	    CIX_BOOK_DEFAULT_HEAP_SIZE) == false) {
		return false;
	}

	book->trade_log = trade_log;
	cix_id_block_init(&book->id_block);
	return true;
}

void
cix_book_destroy(struct cix_book *book)
{

	cix_heap_destroy(&book->bid);
	cix_heap_destroy(&book->offer);

	return;
}

static bool
cix_book_execution(struct cix_book *book, struct cix_order *bid,
    struct cix_order *offer, cix_price_t price)
{
	struct cix_execution execution;

	/*
	 * We can't afford to throw away executions, so we will still report it
	 * with an empty ID to be cleaned up later.
	 */
	execution.id = 0;
	if (cix_id_next(&cix_exec_id_gen, &book->id_block, &execution.id) ==
	    false) {
		fprintf(stderr, "failed to generate execution ID\n");
	}

	execution.buyer = bid->user;
	execution.seller = offer->user;
	memcpy(execution.symbol.symbol, book->symbol.symbol,
	    sizeof execution.symbol.symbol);
	execution.quantity = min(bid->remaining, offer->remaining);
	execution.price = price;

	/*
	 * If we can't persist trade data then it doesn't make sense
	 * to process any further orders.
	 * XXX: Offload logging to a separate thread to allow for timestamping
	 * and other more expensive operations.
	 */
	if (cix_trade_log_execution(book->trade_log, &execution) == false) {
		fprintf(stderr, "!!!failed to log execution!!!\n");
		exit(EXIT_FAILURE);
	}

	bid->remaining -= execution.quantity;
	offer->remaining -= execution.quantity;
	return true;
}

static bool
cix_book_buy(struct cix_book *book, struct cix_order *bid)
{
	struct cix_order *offer;
	
	offer = cix_heap_peek(&book->offer);
	while (offer != NULL && bid->data.price >= offer->data.price) {
		cix_book_execution(book, bid, offer, offer->data.price);
		if (offer->remaining > 0)
			break;

		(void)cix_heap_pop(&book->offer);
		offer = cix_heap_peek(&book->offer);
	}

	if (bid->remaining > 0 && cix_heap_push(&book->bid, bid,
	    CIX_BOOK_BUY_SCORE(bid)) == false) {
		return false;
	}
	
	return true;
}

static bool
cix_book_sell(struct cix_book *book, struct cix_order *offer)
{
	struct cix_order *bid;

	bid = cix_heap_peek(&book->bid);
	while (bid != NULL && offer->data.price <= bid->data.price) {
		cix_book_execution(book, bid, offer, bid->data.price);
		if (bid->remaining > 0)
			break;

		(void)cix_heap_pop(&book->bid);
		bid = cix_heap_peek(&book->bid);
	}

	if (offer->remaining > 0 && cix_heap_push(&book->offer, offer,
	    CIX_BOOK_SELL_SCORE(offer)) == false) {
		return false;
	}

	return true;
}

bool
cix_book_order(struct cix_book *book, struct cix_message_order *message,
    struct cix_session *session)
{
	/* XXX: slab allocation */
	struct cix_order *order = malloc(sizeof *order);

	if (order == NULL) {
		fprintf(stderr, "failed to allocate memory for order\n");
		return false;
	}

	memcpy(&order->data, message, sizeof order->data);

	if (cix_id_next(&cix_exec_id_gen, &book->id_block, &order->id) ==
	    false) {
		fprintf(stderr, "failed to generate order ID\n");
		free(order);
		return false;
	}

	order->session = session;
	order->user = cix_session_user_id(session);
	order->remaining = order->data.quantity;
	order->recv_time = book->recv_counter++;

	switch (order->data.side) {
	case CIX_TRADE_SIDE_BUY:
		return cix_book_buy(book, order);
		break;
	case CIX_TRADE_SIDE_SELL:
		return cix_book_sell(book, order);
		break;
	default:
		fprintf(stderr, "unknown trade side %u\n", order->data.side);
		abort();
		break;
	}

	return false;
}
