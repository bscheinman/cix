#ifndef _CIX_SESSION_H
#define _CIX_SESSION_H

/*
 * Initializes the given number of sessions and starts listening
 * for new connections.
 *
 * XXX: Provide configuration for port numbers, thread count, etc.
 */
void cix_session_listen(unsigned int);

#endif /* _CIX_SESSION_H */
