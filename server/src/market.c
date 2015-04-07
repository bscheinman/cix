#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "book.h"
#include "market.h"
#include "messages.h"
#include "vector.h"
#include "worq.h"

/* XXX: Make this configurable */
#define CIX_MARKET_DEFAULT_BOOK_COUNT 64
#define CIX_MARKET_DEFAULT_WORQ_SIZE 256

struct cix_market_thread {
	struct cix_vector *books;
	struct cix_worq queue;
	pthread_t tid;
};

struct cix_market {
	struct cix_market_thread *threads;
	unsigned int n_thread;
};

/*
 * Copy order by value here because its lifetime is not guaranteed
 * by the network session.
 */
struct cix_market_order_context {
	struct cix_message_order order;
	struct cix_session *session;
};

static bool
cix_market_thread_init(struct cix_market_thread *thread)
{

	if (cix_worq_init(&thread->queue,
	    sizeof(struct cix_market_order_context),
	    CIX_MARKET_DEFAULT_WORQ_SIZE) == false) {
		fprintf(stderr, "failed to create market work queue\n");
		return false;
	}

	if (cix_vector_init(&thread->books, sizeof(struct cix_book),
	    CIX_MARKET_DEFAULT_BOOK_COUNT) == false) {
		fprintf(stderr, "failed to create orderbooks\n");
		return false;
	}

	return true;
}

static void
cix_market_thread_destroy(struct cix_market_thread *thread)
{
	struct cix_book *book;

	CIX_VECTOR_FOREACH(book, thread->books) {
		cix_book_destroy(book);
	}

	free(thread->books);
	cix_worq_destroy(&thread->queue);
	return;
}

/*
 * XXX: Determine which thread should handle a given symbol.
 */
static struct cix_market_thread *
cix_market_symbol_thread(struct cix_market *market, cix_symbol_t *symbol)
{

	(void)market;
	(void)symbol;

	return &market->threads[0];
}

/*
 * Allow configuration to specify any of the following options:
 * 1. Use an event loop to handle orders
 * 2. Busy wait for orders
 * 3. Spin waiting for orders but yield when queue is empty
 *
 * Right now for simplicity we will only use option 2
 */
static void *
cix_market_thread_run(void *p)
{
	struct cix_market_thread *thread = p;

	for (;;) {
		struct cix_market_order_context *context =
		    cix_worq_pop(&thread->queue, CIX_WORQ_WAIT_BLOCK);
		struct cix_book *book;

		/*
		 * XXX: Associate a number with each symbol and have clients
		 * send those instead to avoid this lookup.  In the short term
		 * we could also consider using a hash table here.
		 */
		CIX_VECTOR_FOREACH(book, thread->books) {
			if (strcmp(book->symbol.symbol,
			    context->order.symbol.symbol) != 0) {
				continue;
			}

			if (cix_book_order(book, &context->order,
			    context->session) == false) {
				fprintf(stderr, "failed to process order\n");
			}

			break;
		}
		
		cix_worq_complete(&thread->queue, context);
	}

	return NULL;
}

struct cix_market *
cix_market_init(struct cix_vector *symbols, unsigned int n_thread)
{
	struct cix_market *market = malloc(sizeof *market);
	unsigned int i;
	cix_symbol_t *symbol;

	/* XXX */
	if (n_thread != 1) {
		fprintf(stderr, "multiple market threads not yet supported\n");
		return NULL;
	}

	if (market == NULL) {
		fprintf(stderr, "failed to create market\n");
		return NULL;
	}

	market->n_thread = n_thread;
	market->threads = malloc(market->n_thread * sizeof(*market->threads));
	if (market->threads == NULL) {
		fprintf(stderr, "failed to create market threads\n");
		goto fail;
	}

	for (i = 0; i < market->n_thread; ++i) {
		struct cix_market_thread *thread = &market->threads[i];

		if (cix_market_thread_init(thread) == false) {
			fprintf(stderr, "failed to initialize market thread\n");
			goto fail;
		}
	}

	CIX_VECTOR_FOREACH(symbol, symbols) {
		struct cix_market_thread *thread =
		    cix_market_symbol_thread(market, symbol);
		struct cix_book *book = cix_vector_next(&thread->books);

		printf("symbol %s\n", symbol->symbol);

		if (book == NULL) {
			fprintf(stderr, "failed to create orderbook\n");
			goto fail;
		}

		if (cix_book_init(book, symbol) == false) {
			fprintf(stderr, "failed to initialize orderbook\n");
			goto fail;
		}
	}

	return market;

fail:
	for (i = 0; i < market->n_thread; ++i) {
		cix_market_thread_destroy(&market->threads[i]);
	}

	free(market->threads);
	free(market);
	return NULL;
}

bool
cix_market_run(struct cix_market *market)
{
	unsigned int i, j;

	for (i = 0; i < market->n_thread; ++i) {
		struct cix_market_thread *thread = &market->threads[i];
		int r;

		r = pthread_create(&thread->tid, NULL,
		    cix_market_thread_run, thread);
		if (r == 0) {
			continue;
		}

		fprintf(stderr, "failed to start market thread: %s\n",
		    strerror(r));
		break;
	}

	if (i == market->n_thread) {
		return true;
	}

	for (j = 0; j <= i; ++j) {
		/* XXX: Cancel any threads that have been started */
	}

	return false;
}

bool
cix_market_order(struct cix_market *market, struct cix_message_order *order,
    struct cix_session *session)
{
	struct cix_market_thread *thread =
	    cix_market_symbol_thread(market, &order->symbol);
	struct cix_market_order_context *context;

	context = cix_worq_claim(&thread->queue);
	if (context == NULL) {
		fprintf(stderr,
		    "failed to submit order: market queue is full\n");
		return false;
	}

	context->session = session;
	memcpy(&context->order, order, sizeof context->order);
	
	cix_worq_publish(&thread->queue, context);
	return true;
}
