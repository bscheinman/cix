#ifndef _CIX_MARKET_H
#define _CIX_MARKET_H

#include <stdbool.h>

#include "messages.h"

struct cix_book;
struct cix_vector;

struct cix_book *cix_market_book_get(cix_symbol_t);
bool cix_market_init(struct cix_vector *);

#endif /* _CIX_MARKET_H */
