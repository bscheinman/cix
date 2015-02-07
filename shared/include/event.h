#ifndef _CIX_EVENT_H
#define _CIX_EVENT_H

#include <stdbool.h>
#include <sys/epoll.h>

struct cix_event;
typedef struct cix_event cix_event_t;

typedef unsigned long cix_event_flags_t;

typedef void cix_event_handler_t(cix_event_t *, cix_event_flags_t, void *);

struct cix_event_manager {
	int epoll_fd;
};
typedef struct cix_event_manager cix_event_manager_t;

struct cix_event {
	int fd;
	bool managed;
	cix_event_handler_t *handler;
	void *closure;
};

void cix_event_manager_init(cix_event_manager_t *);
void cix_event_manager_run(cix_event_manager_t *);

bool cix_event_init_fd(cix_event_t *, int, cix_event_handler_t *, void *);
bool cix_event_init_managed(cix_event_t *, cix_event_handler_t *, void *);
bool cix_event_add(cix_event_manager_t *, cix_event_t *);
bool cix_event_remove(cix_event_manager_t *, cix_event_t *);
bool cix_event_managed_trigger(cix_event_t *);

static inline int
cix_event_fd(cix_event_t *event)
{

	return event->fd;
}

bool cix_event_flags_read(cix_event_flags_t);
bool cix_event_flags_write(cix_event_flags_t);

#endif /* _CIX_EVENT_H */
