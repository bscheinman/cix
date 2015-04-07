#ifndef _CIX_MARKET_H
#define _CIX_MARKET_H

#include <stdbool.h>

struct cix_market;
struct cix_message_order;
struct cix_session;
struct cix_vector;

struct cix_market *cix_market_init(struct cix_vector *, unsigned int);
bool cix_market_run(struct cix_market *);
bool cix_market_order(struct cix_market *, struct cix_message_order *,
    struct cix_session *);

#endif /* _CIX_MARKET_H */
