#ifndef _CIX_SESSION_H
#define _CIX_SESSION_H

#include "messages.h"

struct cix_market;
struct cix_session;

/*
 * Initializes the given number of sessions and starts listening
 * for new connections.
 *
 * XXX: Provide configuration for port numbers, thread count, etc.
 */
void cix_session_listen(struct cix_market *, unsigned int);

cix_user_id_t cix_session_user_id(const struct cix_session *);

#endif /* _CIX_SESSION_H */
