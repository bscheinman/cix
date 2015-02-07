#include <stdlib.h>
#include <string.h>

#include "vector.h"

bool
cix_vector_init(struct cix_vector **vector, unsigned int size)
{
	struct cix_vector *v = malloc(sizeof(*vector) + size * sizeof(void *));

	if (v == NULL)
		return false;
	
	v->size = 0;
	v->capacity = size;
	*vector = v;
	return true;
}

bool
cix_vector_append(struct cix_vector **v, union cix_vector_element *item)
{
	struct cix_vector *vector = *v;

	if (vector->size == vector->capacity) {
		unsigned int new_size = vector->size << 1;
		struct cix_vector *new = realloc(vector,
		    sizeof(*vector) + new_size * sizeof(vector->elements[0]));

		if (new == NULL)
			return false;

		vector = new;
		*v = new;
	}

	vector->elements[vector->size++] = *item;
	return true;
}

union cix_vector_element *
cix_vector_item(struct cix_vector *vector, unsigned int index)
{

	if (index >= vector->size)
		return NULL;

	return &vector->elements[index];
}

void
cix_vector_remove(struct cix_vector *vector, unsigned int index)
{

	if (index >= vector->size)
		return;
	
	memmove(&vector->elements[index], &vector->elements[index - 1],
	    (vector->size - (index + 1)) * sizeof(vector->elements[0]));
	--vector->size;
	return;
}

void
cix_vector_destroy(struct cix_vector **vector)
{

	free(*vector);
	*vector = NULL;
	return;
}
