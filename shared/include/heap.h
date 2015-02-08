#ifndef _CIX_HEAP_H
#define _CIX_HEAP_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint64_t cix_heap_score_t;

struct cix_heap_node {
	void *item;
	cix_heap_score_t score;
};

/*
 * This is not thread-safe
 */
struct cix_heap {
	bool is_min;
	size_t n_elements;
	size_t capacity;
	struct cix_heap_node *elements;
};

bool cix_heap_init(struct cix_heap *, bool, size_t);
bool cix_heap_push(struct cix_heap *, void *, cix_heap_score_t);
void *cix_heap_peek(struct cix_heap *);
void *cix_heap_pop(struct cix_heap *);
void cix_heap_destroy(struct cix_heap *);


#endif /* _CIX_HEAP_H */
