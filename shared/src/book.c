#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "book.h"
#include "messages.h"
#include "order.h"

#define CIX_BOOK_DEFAULT_HEAP_SIZE	(1 << 8)

#define CIX_BOOK_BUY_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((~((O)->recv_time)) & (((uint64_t)1 << 32) - 1)))
#define CIX_BOOK_SELL_SCORE(O) (((((uint64_t)(O)->data.price)) << 32) | \
	((O)->recv_time & (((uint64_t)1 << 32) - 1)))

bool
cix_book_init(struct cix_book *book, const char *symbol)
{

	strncpy(book->symbol, symbol, sizeof book->symbol);

	if (cix_heap_init(&book->bid, false,
	    CIX_BOOK_DEFAULT_HEAP_SIZE) == false) {
		return false;
	}

	if (cix_heap_init(&book->offer, true,
	    CIX_BOOK_DEFAULT_HEAP_SIZE) == false) {
		return false;
	}

	if (pthread_mutex_init(&book->mutex, NULL) == false) {
		return false;
	}

	return true;
}

static bool
cix_book_execution(struct cix_book *book, struct cix_order *bid,
    struct cix_order *offer, cix_price_t price)
{
	cix_quantity_t quantity = min(bid->remaining, offer->remaining);

	bid->remaining -= quantity;
	offer->remaining -= quantity;

	printf("executed %" CIX_PR_Q " shares of %s at %" CIX_PR_P "\n",
	    quantity, book->symbol, price);
	return true;
}

bool
cix_book_buy(struct cix_book *book, struct cix_order *bid)
{
	struct cix_order *offer;
	
	offer = cix_heap_peek(&book->offer);
	while (offer != NULL && bid->data.price >= offer->data.price) {
		cix_book_execution(book, bid, offer, offer->data.price);
		if (offer->remaining == 0) {
			(void)cix_heap_pop(&book->offer);
			offer = cix_heap_peek(&book->offer);
		} else {
			break;
		}
	}

	if (bid->remaining > 0 && cix_heap_push(&book->bid, bid,
	    CIX_BOOK_BUY_SCORE(bid)) == false) {
		return false;
	}
	
	return true;
}

bool
cix_book_sell(struct cix_book *book, struct cix_order *offer)
{
	struct cix_order *bid;

	bid = cix_heap_peek(&book->bid);
	while (bid != NULL && offer->data.price <= bid->data.price) {
		cix_book_execution(book, bid, offer, bid->data.price);
		if (bid->remaining == 0) {
			(void)cix_heap_pop(&book->bid);
			bid = cix_heap_peek(&book->bid);
		} else {
			break;
		}
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
	bool result;

	pthread_mutex_lock(&book->mutex);

	order->remaining = order->data.quantity;
	order->recv_time = book->recv_counter++;

	switch (order->data.side) {
	case CIX_TRADE_SIDE_BUY:
		result = cix_book_buy(book, order);
		break;
	case CIX_TRADE_SIDE_SELL:
		result = cix_book_sell(book, order);
		break;
	default:
		fprintf(stderr, "unknown trade side %u\n", order->data.side);
		abort();
		break;
	}

	pthread_mutex_unlock(&book->mutex);
	return result;
}
