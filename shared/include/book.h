#ifndef _CIX_BOOK_H
#define _CIX_BOOK_H

#include <inttypes.h>
#include <pthread.h>

#include "heap.h"
#include "messages.h"

struct cix_book {
	/* XXX: Re-evaluate lock choice here */
	/*
	 * XXX: We definitely want access to each orderbook to be
	 * single-threaded, as the matching logic is probably too complex to
	 * perform concurrently.  However, ideally we will avoid using a mutex.
	 * A better option in the future is to have session threads
	 * place orders into MPSC queues and having processing threads
	 * responsible for order books or groups thereof.  This should
	 * avoid any locking on the order processing fast path.
	 */

	/*
	 * XXX: As long as we're using locks, we need to switch to one that
	 * ensures fairness and preferably minimizes cache effects.
	 * ck_spinlock_clh seems like a good option.
	 */
	pthread_mutex_t mutex;
	uint64_t recv_counter;
	struct cix_heap bid;
	struct cix_heap offer;
	cix_symbol_t symbol;
};

struct cix_order;

bool cix_book_init(struct cix_book *, const char *);
bool cix_book_order(struct cix_book *, struct cix_order *);

#endif /* _CIX_BOOK_H */