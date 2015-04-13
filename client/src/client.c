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

#include "client.h"
#include "messages.h"

bool
cix_client_init(struct cix_client *client, const char *address, uint16_t port)
{
	struct addrinfo hints;
	struct addrinfo *result;
	int r;
	char port_buf[8];

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

	/*
	if (fcntl(client->fd, F_SETFL, O_NONBLOCK) != 0) {
		perror("fcntl");
		return false;
	}
	*/

	if (connect(client->fd, result->ai_addr, result->ai_addrlen) != 0) {
		perror("connect");
		return false;
	}

	/* XXX: cleanup in case of failure */
	return true;
}

bool
cix_client_send_order(struct cix_client *client,
    const struct cix_message_order *order)
{
	ssize_t b;
	unsigned char buf[sizeof(*order) + 1];

	/* XXX: Provide better API to avoid memcpy */
	buf[0] = CIX_MESSAGE_ORDER;
	memcpy(buf + 1, order, sizeof *order);

try_send:
	b = send(client->fd, buf, sizeof buf, 0);
	if (b == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			/* XXX: Consider buffering for resend later */
			goto try_send;
		}

		perror("send");
		return false;
	}

	if ((size_t)b < sizeof buf) {
		fprintf(stderr, "partial send\n");
		return false;
	}

	return true;
}
