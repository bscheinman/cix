#ifndef _CIX_VECTOR_H
#define _CIX_VECTOR_H

#include <inttypes.h>
#include <stdbool.h>

#include "messages.h"

#define CIX_VECTOR_FOREACH(V, E, I)\
for ((I) = 0, (E) = &((V)->elements[0]); (I) < (V)->size, (E) = &((V)->elements[(I)]); ++(I))

union cix_vector_element {
	void *ptr;
	cix_symbol_t symbol;
	uint64_t uint;
};
typedef union cix_vector_element cix_vector_element_t;

struct cix_vector {
	unsigned int size;
	unsigned int capacity;
	union cix_vector_element elements[];
};

bool cix_vector_init(struct cix_vector **, unsigned int);
bool cix_vector_append(struct cix_vector **, cix_vector_element_t *);
cix_vector_element_t *cix_vector_item(struct cix_vector *, unsigned int);
void cix_vector_remove(struct cix_vector *, unsigned int);
void cix_vector_destroy(struct cix_vector **);

#endif /* _CIX_VECTOR_H */
