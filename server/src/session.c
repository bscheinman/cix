#include <assert.h>
#include <ck_pr.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include "book.h"
#include "buffer.h"
#include "event.h"
#include "market.h"
#include "messages.h"
#include "misc.h"
#include "session.h"
#include "trade_data.h"
#include "worq.h"

/* XXX: Make these configurable */
#define CIX_SESSION_ACCEPT_SOCKET "13579"
#define CIX_SESSION_BUFFER_SIZE (1 << 14)
#define CIX_SESSION_INTERNAL_QUEUE_SIZE (1 << 16)

struct cix_session_internal_event {
	struct cix_message message;
};

enum cix_session_read_state {
	/* XXX: Ignore authentication and identification for now */
	CIX_READ_STATE_MESSAGE_TYPE,
	CIX_READ_STATE_MESSAGE_DATA
};

struct cix_session {
	int fd;
	struct cix_event fd_event;

	/* XXX: replace this with a buffer to reduce syscalls */
	struct {
		enum cix_session_read_state state;
		struct cix_message message;
		size_t payload_target;
		size_t bytes_read;
	} read;

	struct cix_buffer *write_buf;
	struct {
		struct cix_worq queue;
		struct cix_event event;
	} internal;

	struct cix_session_thread *thread;
	cix_user_id_t user_id;
};

struct cix_session_thread {
	struct cix_event_manager event_manager;

	struct {
		struct cix_event event;

		/*
		 * XXX: Right now the accept thread will write an accepted fd
		 * here to indicate that the session thread should process it.
		 * Once the session thread has acknowledged this fd, it will
		 * reset this variable to -1.  As long as its value is not -1,
		 * the accept thread will not try to assign more fds to this
		 * thread.  This is effectively equivalent to each session
		 * thread having an incoming queue size of 1.  If all threads
		 * have value > 0, the accept thread will block waiting to
		 * assign the fd to one of them.  In the future we should
		 * replace this with an actual queue to reduce the probability
		 * that the accept thread blocks.  For now synchronization is
		 * simple because the accept thread only writes -1 --> fd and
		 * the session thread only writes fd --> -1.
		 */
		int waiting;
	} accept;

	pthread_t tid;
	struct cix_market *market;
};

static struct cix_session_thread *cix_session_threads;
static size_t cix_session_thread_count;

static unsigned int cix_global_user_id;

static void
cix_session_process_message(struct cix_session *session,
    struct cix_message *message)
{
	struct cix_message_cancel *cancel;
	struct cix_message_order *order;

	switch (message->type) {
	case CIX_MESSAGE_ORDER:
		order = &message->payload.order;

		/*
		printf("received order message: %s %" PRIu32 " shares of "
		    "%s at %" PRIu32 "\n",
		    order->side == CIX_TRADE_SIDE_BUY ? "BUY" : "SELL",
		    order->quantity, order->symbol.symbol, order->price);
		*/

		if (cix_market_order(session->thread->market, order, session) ==
		    false) {
			fprintf(stderr, "failed to process order\n");
		}

		break;
	case CIX_MESSAGE_CANCEL:
		cancel = &message->payload.cancel;
		(void)cancel;
		//printf("cancel order ID %s\n", cancel->internal_id);
		break;
	}

	return;
}

static void
cix_session_close(struct cix_session *session)
{

	while (close(session->fd) == -1 && errno == EINTR);

	cix_buffer_destroy(&session->write_buf);
	cix_worq_destroy(&session->internal.queue);
	cix_event_remove(&session->thread->event_manager, &session->fd_event);
	cix_event_remove(&session->thread->event_manager,
	    &session->internal.event);
	free(session);
	return;
}

static void
cix_session_read(struct cix_session *session)
{
	ssize_t r;

	switch (session->read.state) {
	case CIX_READ_STATE_MESSAGE_TYPE:
read_type:
		r = read(session->fd,
		    &session->read.message.type + session->read.bytes_read,
		    sizeof(session->read.message.type) -
		    session->read.bytes_read);
		if (r == -1) {
			switch (errno) {
			case EINTR:
				goto read_type;
			case EAGAIN:
				return;
			default:
				perror("reading message type");
				return;
			}
		}

		session->read.bytes_read += r;
		if (session->read.bytes_read <
		    sizeof session->read.message.type) {
			return;
		}

		session->read.bytes_read = 0;
		session->read.payload_target = cix_message_length(
		    (enum cix_message_type)(session->read.message.type & 0xFF));
		if (session->read.payload_target == 0) {
			fprintf(stderr, "invalid message type\n");
			return;
		}
		session->read.state = CIX_READ_STATE_MESSAGE_DATA;
		/* fall-through */

	case CIX_READ_STATE_MESSAGE_DATA:
read_payload:
		r = read(session->fd,
		    &session->read.message.payload + session->read.bytes_read,
		    session->read.payload_target - session->read.bytes_read);
		if (r == -1) {
			switch (errno) {
			case EINTR:
				goto read_payload;
			case EAGAIN:
				return;
			default:
				perror("reading message payload");
				return;
			}
		}

		session->read.bytes_read += r;
		if (session->read.bytes_read < session->read.payload_target) {
			fprintf(stderr, "waiting for remainder of payload\n");
			return;
		}

		cix_session_process_message(session, &session->read.message);
		session->read.bytes_read = 0;
		session->read.state = CIX_READ_STATE_MESSAGE_TYPE;

		/* Try reading another message if possible */
		goto read_type;
	default:
		break;
	}

	return;
}

static void
cix_session_write_flush(struct cix_session *session)
{
	struct cix_buffer_result result;

	cix_buffer_fd_write(session->write_buf, session->fd, 0, 0, &result);
	switch (result.code) {
	case CIX_BUFFER_OK:
		break;
	case CIX_BUFFER_PARTIAL:
		fprintf(stderr, "partial session write\n");
		break;
	case CIX_BUFFER_BLOCKED:
		fprintf(stderr, "session write blocked\n");
		break;
	case CIX_BUFFER_ERROR:
		fprintf(stderr, "session write error\n");
		break;
	default:
		fprintf(stderr, "unrecognized buffer state\n");
		break;
	}

	return;
}

static void
cix_session_message(cix_event_t *event, cix_event_flags_t flags, void *closure)
{
	struct cix_session *session = closure;

	if (cix_event_flags_close(flags) == true) {
		cix_session_close(session);
		return;
	}

	if (cix_event_flags_read(flags) == true) {
		cix_session_read(session);
	}

	if (cix_event_flags_write(flags) == true) {
		cix_session_write_flush(session);
	}

	return;
}

static void
cix_session_internal_event(struct cix_event *event, cix_event_flags_t flags,
    void *closure)
{
	struct cix_session *session = closure;
	struct cix_worq *queue = &session->internal.queue;

	(void)flags;

	for (;;) {
		struct cix_session_internal_event *internal;
		size_t message_size;
		
		internal = cix_worq_pop(queue, CIX_WORQ_WAIT_BLOCK_SLOT);
		if (internal == NULL) {
			break;
		}

		message_size = cix_message_length(internal->message.type) + 1;
		if (cix_buffer_append(&session->write_buf, &internal->message,
		    message_size) == false) {
			fprintf(stderr, "failed to write to session buffer\n");
		}
		
		cix_worq_complete(queue, internal);
	}

	cix_session_write_flush(session);
	return;
}

static int
cix_session_socket(void)
{
	struct addrinfo hints;
	struct addrinfo *info;
	int r, fd;
	int one = 1;
	int flags;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	r = getaddrinfo(NULL, CIX_SESSION_ACCEPT_SOCKET, &hints, &info);
	if (r == -1) {
		fprintf(stderr, "error determining network address: %s\n",
		    gai_strerror(r));
		exit(EXIT_FAILURE);
	}

	/* XXX: For now just use the first valid address */
	fd = socket(info[0].ai_family, info[0].ai_socktype,
	    info[0].ai_protocol);
	if (fd == -1) {
		perror("opening network socket");
		exit(EXIT_FAILURE);
	}

	r = bind(fd, info[0].ai_addr, info[0].ai_addrlen);
	if (r == -1) {
		perror("binding network socket");
		exit(EXIT_FAILURE);

	}

	r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
	if (r == -1) {
		perror("setting socket options");
		exit(EXIT_FAILURE);
	}

	flags = fcntl(fd, F_GETFL);
	r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (r == -1) {
		perror("setting non-blocking I/O");
		exit(EXIT_FAILURE);
	}

	r = listen(fd, 8);
	if (r == -1) {
		perror("listening to socket");
		exit(EXIT_FAILURE);
	}

	return fd;
}

static void *
cix_session_accept(void *unused)
{
	int socket_fd, fd;
	size_t session_index = 0;

	(void)unused;

	socket_fd = cix_session_socket();

	for (;;) {
		struct sockaddr client_addr;
		socklen_t client_addr_len = sizeof client_addr;
		int flags;

		fd = accept(socket_fd, &client_addr, &client_addr_len);
		if (fd == -1) {
			if (errno != EAGAIN)
				perror("accepting connection");

			continue;
		}

		flags = fcntl(fd, F_GETFL);
		if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
			fprintf(stderr, "failed to make connection "
			    "non-blocking: %s\n", strerror(errno));
			while (close(fd) && errno == EINTR);
			continue;
		}

		/*
		 * XXX: just use round robin assignment for now, but in the
		 * future explore a least-loaded approach or insert into a
		 * SPMC queue and have threads pull from there.
		 */
		for (;;) {
			struct cix_session_thread *thread =
			    &cix_session_threads[session_index];

			session_index = (session_index + 1) %
			    cix_session_thread_count;

			/*
			 * Thread already has a pending connection to accept,
			 * so try another one.
			 */
			if (thread->accept.waiting > 0)
				continue;

			thread->accept.waiting = fd;
			/* XXX: fence */
			if (cix_event_managed_trigger(&thread->accept.event) ==
			    true) {
				break;
			}

			/*
			 * If we failed to notify the session thread, reset
			 * its waiting token so that it isn't permanently
			 * blocked, and move on to the next thread.
			 */
			thread->accept.waiting = -1;
		}
	}

	return NULL;
}

/* XXX: Use slab allocation instead of calling malloc each time */
static struct cix_session *
cix_session_create(int fd, struct cix_session_thread *thread)
{
	struct cix_session *session;

	session = malloc(sizeof *session);
	if (session == NULL) {
		return NULL;
	}

	if (cix_event_init_fd(&session->fd_event, fd, cix_session_message,
	    session) == false) {
		fprintf(stderr, "failed to create fd listener\n");
		goto fd_fail;
	}

	if (cix_buffer_init(&session->write_buf, CIX_SESSION_BUFFER_SIZE) ==
	    false) {
		fprintf(stderr, "failed to create session buffer\n");
		goto buffer_fail;
	}

	if (cix_worq_init(&session->internal.queue,
	    CIX_SESSION_INTERNAL_QUEUE_SIZE,
	    sizeof(struct cix_session_internal_event)) == false) {
		fprintf(stderr, "failed to create internal event queue\n");
		goto queue_fail;
	}

	if (cix_event_init_managed(&session->internal.event,
	    cix_session_internal_event, session) == false) {
		fprintf(stderr, "failed to create internal event listener\n");
		goto event_fail;
	}

	if (cix_worq_event_subscribe(&session->internal.queue,
	    &session->internal.event) == false) {
		fprintf(stderr, "failed to subscribe to internal event "
		    "queue\n");
		goto subscribe_fail;
	}

	session->fd = fd;
	session->read.state = CIX_READ_STATE_MESSAGE_TYPE;
	session->read.bytes_read = 0;

	/* XXX: Authenticate */
	session->user_id = ck_pr_faa_uint(&cix_global_user_id, 1);
	session->thread = thread;

	return session;

subscribe_fail:
event_fail:
	cix_worq_destroy(&session->internal.queue);

queue_fail:
	cix_buffer_destroy(&session->write_buf);

buffer_fail:
fd_fail:
	free(session);
	return NULL;
}

static void
cix_session_thread_accept(cix_event_t *event, cix_event_flags_t flags,
    void *closure)
{
	struct cix_session_thread *thread = closure;
	struct cix_session *session;

	if (cix_event_flags_read(flags) == false)
		return;

	assert(thread->accept.waiting > 0);

	session = cix_session_create(thread->accept.waiting, thread);
	if (session == NULL) {
		fprintf(stderr, "failed to create new session\n");
		exit(EXIT_FAILURE);
	}

	if (cix_event_add(&thread->event_manager, &session->fd_event) ==
	    false || cix_event_add(&thread->event_manager,
	    &session->internal.event) == false) {
		fprintf(stderr, "failed to register session events\n");
		exit(EXIT_FAILURE);
	}

	thread->accept.waiting = -1;
	return;
}

static void *
cix_session_thread(void *closure)
{
	struct cix_session_thread *thread = closure;

	cix_event_manager_init(&thread->event_manager);

	if (cix_event_init_managed(&thread->accept.event,
	    cix_session_thread_accept, thread) == false) {
		fprintf(stderr, "failed to initialize session thread accept "
		    "event\n");
		exit(EXIT_FAILURE);
	}
	
	if (cix_event_add(&thread->event_manager, &thread->accept.event) ==
	    false) {
		fputs("failed to initialize session accept queue\n", stderr);
		exit(EXIT_FAILURE);
	}

	if (cix_event_manager_run(&thread->event_manager) == false) {
		fprintf(stderr, "failed to run session event loop\n");
	}

	return NULL;
}

/*
 * Use a single thread to listen for new connections and then assign those
 * connections to a thread pool for actual processing.
 */
void
cix_session_listen(struct cix_market *market, unsigned int n)
{
	pthread_t accept_thread;
	int r;
	unsigned int i;

	if (n == 0) {
		fprintf(stderr, "session thread count must be positive");
		exit(EXIT_FAILURE);
	}

	cix_session_thread_count = n;
	cix_session_threads = malloc(n * sizeof(*cix_session_threads));
	if (cix_session_threads == NULL) {
		fprintf(stderr, "failed to allocate session thread contexts\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n; ++i) {
		struct cix_session_thread *thread = cix_session_threads + i;

		thread->market = market;
		thread->accept.waiting = -1;
		r = pthread_create(&thread->tid, NULL, cix_session_thread,
		    thread);
		if (r != 0) {
			fprintf(stderr, "failed to create session thread\n");
			exit(EXIT_FAILURE);
		}
	}

	/*
	 * XXX: Currently a race condition here-- we might accept connections
	 * before session threads have completed initialization.  Should fix
	 * by adding barrier before starting accept thread.
	 */
	r = pthread_create(&accept_thread, NULL, cix_session_accept, NULL);
	if (r != 0) {
		fprintf(stderr, "failed to create network listener thread\n");
		exit(EXIT_FAILURE);
	}

	return;
}

cix_user_id_t
cix_session_user_id(const struct cix_session *session)
{

	return session->user_id;
}

bool
cix_session_execution_report(struct cix_session *session,
    cix_order_id_t order_id, cix_price_t price, cix_quantity_t quantity)
{
	struct cix_session_internal_event *event;
	struct cix_worq *queue = &session->internal.queue;
	struct cix_message *message;

	event = cix_worq_claim(queue);

	/*
	 * XXX: Should just block here instead
	 * XXX: Add a flag to worq_claim to indicate blocking similar
	 * to cix_worq_pop behavior.
	 */
	if (event == NULL) {
		fprintf(stderr, "failed to report execution: message queue "
		    "is full\n");
		return false;
	}

	message = &event->message;
	message->type = CIX_MESSAGE_EXECUTION;
	message->payload.execution.order_id = order_id;
	message->payload.execution.price = price;
	message->payload.execution.quantity = quantity;

	cix_worq_publish(queue, event);
	return true;
}
