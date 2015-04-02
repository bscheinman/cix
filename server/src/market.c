#include "book.h"
#include "market.h"
#include "vector.h"

/*
 * XXX: Create a separate book for each symbol
 */
static struct cix_book cix_the_book;

bool
cix_market_init(struct cix_vector *symbols)
{
	cix_vector_element_t *item;
	unsigned int i;

	CIX_VECTOR_FOREACH(symbols, item, i) {
		/* XXX: Create orderbook for symbol */
	}

	return cix_book_init(&cix_the_book, "GOOG");
}

struct cix_book *
cix_market_book_get(cix_symbol_t symbol)
{

	(void)symbol;
	return &cix_the_book;
}
