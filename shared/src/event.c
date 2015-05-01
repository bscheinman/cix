#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>

#include "event.h"

#define CIX_EVENT_MAX 8

bool
cix_event_manager_init(struct cix_event_manager *manager)
{

	manager->epoll_fd = epoll_create(1);
	if (manager->epoll_fd == -1) {
		fprintf(stderr, "failed to initialize event loop\n");
		return false;
	}

	return true;
}

static void
cix_event_managed_drain(struct cix_event *event)
{
	int64_t value;
	ssize_t r;

	assert(event->managed == true);

	for (;;) {
		r = read(event->fd, &value, sizeof value);
		if (r > 0)
			return;

		if (errno == EINTR)
			continue;

		if (errno == EAGAIN)
			return;

		fprintf(stderr, "failed to drain managed event\n");
		return;
	}
	
	return;
}

bool
cix_event_manager_run(struct cix_event_manager *manager)
{
	int r, i;
	struct epoll_event events[CIX_EVENT_MAX];

	for (;;) {
		r = epoll_wait(manager->epoll_fd, events, CIX_EVENT_MAX, -1);
		if (r == -1) {
			if (errno == EINTR)
				continue;

			fprintf(stderr, "event loop failed\n");
			return false;
		}

		for (i = 0; i < r; ++i) {
			struct cix_event *event = events[i].data.ptr;
			uint32_t flags = events[i].events;

			if (event->managed == true) {
				if ((flags & EPOLLIN) == 0) {
					continue;
				}
				cix_event_managed_drain(event);
			}

			event->handler(event, flags, event->closure);
		}
	}

	return true;
}

bool
cix_event_init_managed(struct cix_event *event,
    cix_event_handler_t *handler, void *closure)
{

	event->fd = eventfd(0, EFD_NONBLOCK);
	if (event->fd == -1) {
		perror("initializing managed event");
		return false;
	}

	event->managed = true;
	event->handler = handler;
	event->closure = closure;
	return true;
}

bool
cix_event_init_fd(struct cix_event *event, int fd,
    cix_event_handler_t *handler, void *closure)
{

	event->fd = fd;
	event->managed = false;
	event->handler = handler;
	event->closure = closure;

	return true;
}

bool
cix_event_add(struct cix_event_manager *manager, struct cix_event *event)
{
	struct epoll_event epoll;
	int r;

	epoll.data.ptr = event;
	/* XXX: Allow custom flags */
	epoll.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET;

	r = epoll_ctl(manager->epoll_fd, EPOLL_CTL_ADD, event->fd, &epoll);
	if (r == -1) {
		perror("registering event");
		return false;
	}

	return true;
}

bool
cix_event_remove(struct cix_event_manager *manager, struct cix_event *event)
{
	int r;

	r = epoll_ctl(manager->epoll_fd, EPOLL_CTL_DEL, event->fd, NULL);
	if (r == -1) {
		fprintf(stderr, "failed to unregister event\n");
		return false;
	}

	return true;
}

bool
cix_event_managed_trigger(struct cix_event *event)
{
	int64_t value = 1;
	ssize_t w;

	assert(event->managed == true);

	for (;;) {
		w = write(event->fd, &value, sizeof value);
		if (w > 0)
			return true;

		if (errno == EINTR)
			continue;

		fprintf(stderr, "failed to trigger managed event\n");
		return false;
	}

	return false;
}

bool
cix_event_flags_read(cix_event_flags_t flags)
{

	return flags & EPOLLIN;
}

bool
cix_event_flags_write(cix_event_flags_t flags)
{

	return flags & EPOLLOUT;
}

bool
cix_event_flags_close(cix_event_flags_t flags)
{

	return flags & (EPOLLHUP | EPOLLRDHUP);
}
