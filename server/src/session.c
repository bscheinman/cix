#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>

#include "event.h"
#include "misc.h"
#include "session.h"

/* XXX: Make these configurable */
#define CIX_SESSION_THREAD_COUNT 2
#define CIX_SESSION_ACCEPT_SOCKET "13579"

struct cix_session {
	int fd;
	cix_event_t event;
};

struct cix_session_thread {
	cix_event_manager_t event_manager;
	struct {
		cix_event_t event;

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
};

static struct cix_session_thread *cix_session_threads;
static size_t cix_session_thread_count;

/* XXX: Use slab allocation instead of calling malloc each time */
static struct cix_session *
cix_session_create(int fd)
{
	struct cix_session *session;

	/* XXX */
	session = malloc(sizeof *session);
	if (session == NULL) {
		perror("allocating session object");
		exit(EXIT_FAILURE);
	}

	session->fd = fd;
	return session;
}

static void
cix_session_handler(cix_event_t *event, cix_event_flags_t flags, void *closure)
{
	struct cix_session_thread *thread = closure;
	struct cix_session *session =
	    container_of(event, struct cix_session, event);

	(void)flags;
	(void)thread;
	(void)session;

	/* XXX */
	return;
}

static int
cix_session_socket(void)
{
	struct addrinfo hints;
	struct addrinfo *info;
	int r, fd;
	int one = 1;

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
		socklen_t client_addr_len;

		fd = accept(socket_fd, &client_addr, &client_addr_len);
		if (fd == -1) {
			if (errno != EAGAIN)
				perror("accepting connection");

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

static void
cix_session_thread_accept(cix_event_t *event, cix_event_flags_t flags,
    void *closure)
{
	struct cix_session_thread *thread = closure;
	struct cix_session *session;

	assert(thread->accept.waiting > 0);

	session = cix_session_create(thread->accept.waiting);

	if (cix_event_init_fd(event, session->fd, cix_session_handler,
	    thread) == false ||
	    cix_event_add(&thread->event_manager, event) == false) {
		fprintf(stderr, "failed to register session event\n");
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
		fputs("failed to initialize session thread accept event\n",
		    stderr);
		exit(EXIT_FAILURE);
	}

	if (cix_event_add(&thread->event_manager, &thread->accept.event) ==
	    false) {
		fputs("failed to initialize session accept queue\n", stderr);
		exit(EXIT_FAILURE);
	}

	cix_event_manager_run(&thread->event_manager);
	return NULL;
}

/*
 * Use a single thread to listen for new connections and then assign those
 * connections to a thread pool for actual processing.
 */
void
cix_session_listen(unsigned int n)
{
	pthread_t accept_thread;
	int r;
	unsigned int i;

	if (n == 0) {
		fputs("session thread count must be positive", stderr);
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
