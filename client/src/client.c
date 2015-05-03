#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "buffer.h"
#include "client.h"
#include "messages.h"

#define CIX_CLIENT_BUFFER_SIZE (1 << 16)

static void
cix_client_receive_ack(const struct cix_client *client,
    const struct cix_message_ack *message)
{
	struct cix_client_ack ack;

	if (client->callbacks.ack == NULL) {
		return;
	}

	ack.client_id = message->external_id;
	ack.server_id = message->internal_id;
	switch (message->status) {
	case CIX_ORDER_STATUS_OK:
		ack.status = CIX_CLIENT_ACK_STATUS_OK;
		break;
	case CIX_ORDER_STATUS_ERROR:
		ack.status = CIX_CLIENT_ACK_STATUS_ERROR;
		break;
	default:
		fprintf(stderr, "Invalid ack status %u\n",
		    (unsigned)message->status);
		return;
	}

	client->callbacks.ack(&ack, client->closure);
	return;
}

static void
cix_client_receive_exec(const struct cix_client *client,
    const struct cix_message_execution *message)
{
	struct cix_client_execution exec;

	(void)client;
	(void)message;
	(void)exec;

	if (client->callbacks.exec == NULL) {
		return;
	}

	exec.order_id = message->order_id;
	exec.quantity = message->quantity;
	exec.price = message->price;
	client->callbacks.exec(&exec, client->closure);

	return;
}

static void
cix_client_receive(struct cix_client *client)
{
	struct cix_buffer_result result;
	size_t processed, length, target;

	cix_buffer_fd_read(&client->read_buf, client->fd, 0, 0, &result);

	if (result.code == CIX_BUFFER_ERROR) {
		fprintf(stderr, "Failed to read incoming data\n");
		return;
	}

	processed = 0;
	length = cix_buffer_length(client->read_buf);
	while (processed < length) {
		struct cix_message *message;

		message = (struct cix_message *)
		    (cix_buffer_data(client->read_buf) + processed);
		target = cix_message_length(message->type);

		if (processed + target + 1 > length) {
			break;
		}

		switch (message->type) {
		case CIX_MESSAGE_ACK:
			cix_client_receive_ack(client, &message->payload.ack);
			break;
		case CIX_MESSAGE_EXECUTION:
			cix_client_receive_exec(client,
			    &message->payload.execution);
			break;
		default:
			fprintf(stderr, "Unrecognized message type %u\n",
			    (unsigned)message->type);
			break;
		}

		processed += target + 1;
	}

	cix_buffer_drain(client->read_buf, processed);
	return;
}

static bool
cix_client_send(struct cix_client *client)
{
	struct cix_buffer_result result;
	
	if (client->in_batch == true ||
	    cix_buffer_length(client->write_buf) == 0) {
		return true;
	}

	cix_buffer_fd_write(client->write_buf, client->fd, 0, 0, &result);

	switch (result.code) {
	case CIX_BUFFER_OK:
		break;
	case CIX_BUFFER_PARTIAL:
		fprintf(stderr, "Partial network write\n");
		break;
	case CIX_BUFFER_BLOCKED:
		break;
	case CIX_BUFFER_ERROR:
		fprintf(stderr, "Failed to send data\n");
		return false;
	default:
		break;
	}

	return true;
}

static void
cix_client_event_handler(struct cix_event *event, cix_event_flags_t flags,
    void *p)
{
	struct cix_client *client = container_of(event, struct cix_client,
	    event);

	(void)p;

	if (cix_event_flags_read(flags) == true) {
		cix_client_receive(client);
	}

	if (cix_event_flags_write(flags) == true) {
		cix_client_send(client);
	}

	return;
}

bool
cix_client_init(struct cix_client *client, const char *address, uint16_t port,
    struct cix_client_callbacks *callbacks, void *closure)
{
	struct addrinfo hints;
	struct addrinfo *result;
	int r;
	char port_buf[8];

	client->callbacks = *callbacks;
	client->closure = closure;

	if (cix_buffer_init(&client->read_buf, CIX_CLIENT_BUFFER_SIZE) ==
	    false || cix_buffer_init(&client->write_buf,
	    CIX_CLIENT_BUFFER_SIZE) == false) {
		fprintf(stderr, "Failed to initialize client buffers\n");
		return false;
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	sprintf(port_buf, "%" PRIu16, port);
	r = getaddrinfo(address, port_buf, &hints, &result);
	if (r != 0) {
		fprintf(stderr, "failed to get address: %s\n",
		    gai_strerror(r));
		return false;
	}

	client->fd = socket(result->ai_family, result->ai_socktype,
	    result->ai_protocol);
	if (client->fd == -1) {
		perror("socket");
		return false;
	}

	if (connect(client->fd, result->ai_addr, result->ai_addrlen) != 0) {
		perror("connect");
		return false;
	}

	if (fcntl(client->fd, F_SETFL, O_NONBLOCK) != 0) {
		perror("fcntl");
		return false;
	}

	if (cix_event_init_fd(&client->event, client->fd,
	    cix_client_event_handler, client) == false) {
		fprintf(stderr, "failed to initialize event handler\n");
		return false;
	}

	return true;
}

bool
cix_client_send_order(struct cix_client *client,
    const struct cix_message_order *order)
{
	unsigned char buf[sizeof(*order) + 1];

	buf[0] = CIX_MESSAGE_ORDER;
	memcpy(buf + 1, order, sizeof *order);

	if (cix_buffer_append(&client->write_buf, buf, sizeof buf) == false) {
		fprintf(stderr, "Failed to buffer write\n");
		return false;
	}

	return cix_client_send(client);
}

void
cix_client_batch_start(struct cix_client *client)
{

	client->in_batch = true;
	return;
}

bool
cix_client_batch_end(struct cix_client *client)
{

	client->in_batch = false;
	return cix_client_send(client);
}
