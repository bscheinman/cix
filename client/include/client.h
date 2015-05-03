#ifndef _CIX_CLIENT_SESSION_H
#define _CIX_CLIENT_SESSION_H

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "event.h"
#include "messages.h"

/*
 * Create separate client structs to decouple consumers from
 * network protocol implementation.
 */

enum cix_client_ack_status {
	CIX_CLIENT_ACK_STATUS_OK,
	CIX_CLIENT_ACK_STATUS_ERROR
};

struct cix_client_ack {
	const char *client_id;
	uint64_t server_id;
	enum cix_client_ack_status status;
};

struct cix_client_execution {
	uint64_t order_id;
	unsigned int quantity;
	unsigned int price;
};

typedef void cix_client_ack_cb_t(struct cix_client_ack *, void *);
typedef void cix_client_exec_cb_t(struct cix_client_execution *, void *);

struct cix_client_callbacks {
	cix_client_ack_cb_t *ack;
	cix_client_exec_cb_t *exec;
};

struct cix_client {
	int fd;
	struct cix_event event;

	struct cix_buffer *read_buf;
	struct cix_buffer *write_buf;
	bool in_batch;

	struct cix_client_callbacks callbacks;
	void *closure;
};

bool cix_client_init(struct cix_client *, const char *, uint16_t,
    struct cix_client_callbacks *, void *);

static inline struct cix_event *
cix_client_event(struct cix_client *client)
{

	return &client->event;
}

/*
 * This interface allows users to send multiple messages in a single batch
 * without requiring multiple system calls.  However, there is currently
 * no guarantee of all-or-nothing transmission.  In the future we can add
 * server support for these types of batches.
 */
void cix_client_batch_start(struct cix_client *);
bool cix_client_batch_end(struct cix_client *);

struct cix_message_order;

/* XXX: Use separate struct to abstract away network representation */
bool cix_client_send_order(struct cix_client *,
    const struct cix_message_order *);


#endif /* _CIX_CLIENT_SESSION_H */
