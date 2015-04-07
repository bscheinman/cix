#ifndef _CIX_VECTOR_H
#define _CIX_VECTOR_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

/*
 * XXX: Make this a macro with types to avoid runtime multiplication
 */

#define CIX_VECTOR_FOREACH(E, V)\
for ((E) = (void *)((V)->data);\
    (unsigned char *)(E) - ((V)->data) < ((V)->length * (V)->item_size);\
    ++(E))

struct cix_vector {
	unsigned int length;
	unsigned int capacity;
	size_t item_size;
	unsigned char data[];
};

bool cix_vector_init(struct cix_vector **, size_t, unsigned int);
void cix_vector_destroy(struct cix_vector **);

bool cix_vector_append(struct cix_vector **, void *);

/*
 * Access the next element in the vector for direct writing.
 * Returns NULL if memory cannot be allocated.
 */
void *cix_vector_next(struct cix_vector **);

void *cix_vector_item(struct cix_vector *, unsigned int);
void cix_vector_remove(struct cix_vector *, unsigned int);

unsigned int cix_vector_length(const struct cix_vector *v);
#endif /* _CIX_VECTOR_H */
