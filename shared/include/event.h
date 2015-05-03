#ifndef _CIX_EVENT_H
#define _CIX_EVENT_H

#include <stdbool.h>
#include <sys/epoll.h>

struct cix_event;
typedef struct cix_event cix_event_t;

typedef unsigned long cix_event_flags_t;

typedef void (*cix_event_handler_t)(cix_event_t *, cix_event_flags_t, void *);

struct cix_event_manager {
	int epoll_fd;
};
typedef struct cix_event_manager cix_event_manager_t;

enum cix_event_type {
	CIX_EVENT_FD,
	CIX_EVENT_MANAGED,
	CIX_EVENT_TIMER
};

struct cix_event {
	int fd;
	cix_event_handler_t handler;
	void *closure;

	enum cix_event_type type;
	union {
		struct {
			unsigned long long ns;
		} timer;
	} data;
};

bool cix_event_manager_init(cix_event_manager_t *);
bool cix_event_manager_run(cix_event_manager_t *);

bool cix_event_init_fd(struct cix_event *, int, cix_event_handler_t, void *);
bool cix_event_init_managed(struct cix_event *, cix_event_handler_t, void *);
bool cix_event_init_timer(struct cix_event *, cix_event_handler_t, void *);
bool cix_event_add(struct cix_event_manager *, struct cix_event *);
bool cix_event_remove(struct cix_event_manager *, struct cix_event *);

bool cix_event_managed_trigger(struct cix_event *);

bool cix_event_timer_set(struct cix_event *, unsigned long long);
bool cix_event_timer_stop(struct cix_event *);

static inline int
cix_event_fd(cix_event_t *event)
{

	return event->fd;
}

bool cix_event_flags_read(cix_event_flags_t);
bool cix_event_flags_write(cix_event_flags_t);
bool cix_event_flags_close(cix_event_flags_t);

#endif /* _CIX_EVENT_H */
