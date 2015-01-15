#ifndef _CIX_BOOK_H
#define _CIX_BOOK_H

#include "heap.h"

struct cix_trade;
typedef void (*cix_trade_handler_t)(struct cix_trade *);

struct cix_book {
	cix_heap_t bid;
	cix_heap_t offer;
};
typedef struct cix_book cix_book_t;

struct cix_order;

void cix_book_init(cix_book_t *);
void cix_book_order(cix_book_t *, struct cix_order *, struct cix_order *);



#endif /* _CIX_BOOK_H */
