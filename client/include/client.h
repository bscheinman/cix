#ifndef _CIX_CLIENT_SESSION_H
#define _CIX_CLIENT_SESSION_H

#include <inttypes.h>
#include <stdbool.h>

struct cix_client {
	int fd;
};

bool cix_client_init(struct cix_client *, const char *, uint16_t);

struct cix_message_order;

/* XXX: Use separate struct to abstract away network representation? */
bool cix_client_send_order(struct cix_client *,
    const struct cix_message_order *);


#endif /* _CIX_CLIENT_SESSION_H */
