#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "vector.h"

bool
cix_vector_init(struct cix_vector **vector, size_t item_size,
    unsigned int length)
{
	struct cix_vector *v = malloc(sizeof(*v) + length * item_size);

	if (v == NULL)
		return false;
	
	v->length = 0;
	v->capacity = length;
	v->item_size = item_size;
	*vector = v;
	return true;
}

static bool
cix_vector_grow(struct cix_vector **v)
{
	struct cix_vector *vector = *v;
	struct cix_vector *new;
	unsigned int new_length;

	if (vector->length < vector->capacity) {
		return true;
	}

	new_length = vector->length << 1;
	new = realloc(vector, sizeof(*vector) + new_length * vector->item_size);

	if (new == NULL)
		return false;

	*v = new;
	return true;
}

bool
cix_vector_append(struct cix_vector **v, void *item)
{
	struct cix_vector *vector;

	if (cix_vector_grow(v) == false) {
		return false;
	}

	vector = *v;
	memcpy(vector->data + vector->length * vector->item_size,
	    item, vector->item_size);
	++vector->length;
	return true;
}

void *
cix_vector_next(struct cix_vector **v)
{
	struct cix_vector *vector;
	void *item;

	if (cix_vector_grow(v) == false) {
		return NULL;
	}

	vector = *v;
	item = vector->data + vector->length * vector->item_size;
	++vector->length;
	return item;
}

void *
cix_vector_item(struct cix_vector *vector, unsigned int index)
{

	if (index >= vector->length)
		return NULL;

	return vector->data + index * vector->item_size;
}

void
cix_vector_remove(struct cix_vector *vector, unsigned int index)
{
	unsigned char *item = vector->data + index * vector->item_size;
	size_t copy_size = (vector->length - (index + 1)) * vector->item_size;

	if (index >= vector->length)
		return;
	
	memmove(item, item + vector->item_size, copy_size);
	--vector->length;
	return;
}

void
cix_vector_destroy(struct cix_vector **vector)
{

	free(*vector);
	*vector = NULL;
	return;
}

unsigned int
cix_vector_length(const struct cix_vector *v)
{

	return v->length;
}
