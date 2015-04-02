#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "book.h"
#include "id_generator.h"
#include "messages.h"
#include "trade_data.h"
#include "trade_log.h"

#define CIX_BOOK_DEFAULT_HEAP_SIZE	(1 << 8)

#define CIX_BOOK_BUY_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((~((O)->recv_time)) & (((uint64_t)1 << 32) - 1)))
#define CIX_BOOK_SELL_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((O)->recv_time & (((uint64_t)1 << 32) - 1)))

static struct cix_id_generator cix_exec_id_gen =
    CIX_ID_GENERATOR_INITIALIZER(1 << 14);

bool
cix_book_init(struct cix_book *book, const char *symbol)
{
	/* XXX: Configurable */
	char log_path[PATH_MAX];
	struct cix_trade_log_config config = { .path = log_path };
	int r;

	strncpy(book->symbol, symbol, sizeof book->symbol);
	if (book->symbol[sizeof(book->symbol) - 1] != '\0') {
		fprintf(stderr, "symbol %s exceeds maximum length\n", symbol);
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

	r = snprintf(log_path, sizeof log_path,
	    "/home/brendon/source/cix/logs/%s", book->symbol);
	if (r == -1) {
		fprintf(stderr, "failed to create log path\n");
		return false;
	} else if ((size_t)r >= sizeof log_path) {
		fprintf(stderr, "log path exceeded maximum length\n");
		return false;
	}

	if (cix_trade_log_init(&book->trade_log, &config) == false) {
		fprintf(stderr, "failed to initialize trade log\n");
		return false;
	}

	cix_id_block_init(&book->id_block);
	return true;
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
	memcpy(execution.symbol, book->symbol, sizeof execution.symbol);
	execution.quantity = min(bid->remaining, offer->remaining);
	execution.price = price;

	/*
	 * If we can't persist trade data then it doesn't make sense
	 * to process any further orders.
	 * XXX: Offload logging to a separate thread to allow for timestamping
	 * and other more expensive operations.
	 */
	if (cix_trade_log_execution(&book->trade_log, &execution) == false) {
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
cix_book_order(struct cix_book *book, struct cix_order *order)
{

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
