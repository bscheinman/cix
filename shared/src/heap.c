#include "heap.h"

#define CIX_HEAP_PARENT(N) (((N) - 1) >> 1)
#define CIX_HEAP_CHILDL(N) (((N) << 1) + 1)
#define CIX_HEAP_CHILDR(N) (((N) << 1) + 2)

#define CIX_HEAP_SWAP(H, X, Y)				\
do {							\
	typeof(*(H)->elements) _tmp;			\
	_tmp = (H)->elements[(X)];			\
	(H)->elements[(X)] = (H)->elements[(Y)];	\
	(H)->elements[(Y)] = _tmp;			\
} while (0);

#define CIX_HEAP_COMPARE(H, X, Y)	\
((((H)->elements[(Y)].score - (H)->elements[(X)].score) * (H)->multiplier) < 0)

bool
cix_heap_init(struct cix_heap *heap, bool min_max, size_t size)
{

	heap->multiplier = min_max ? 1 : -1;
	heap->n_elements = 0;
	heap->capacity = size;
	heap->elements = malloc(heap->capacity * sizeof *heap->elements);
	return heap->elements != NULL;
}

bool
cix_heap_push(struct cix_heap *heap, void *value, cix_heap_score_t score)
{
	size_t cursor, parent;
	void *resize;

	if (heap->n_elements == heap->capacity) {
		resize = realloc(heap->elements, heap->capacity << 1);
		if (resize == NULL)
			return false;
		heap->elements = resize;
		heap->capacity <<= 1;
	}

	cursor = heap->n_elements;
	parent = CIX_HEAP_PARENT(cursor);
	heap->elements[cursor].item = value;
	heap->elements[cursor].score = score;
	while (cursor > 0 && CIX_HEAP_COMPARE(heap, cursor, parent) < 0) {
		CIX_HEAP_SWAP(heap, cursor, parent);
		cursor = parent;
		parent = CIX_HEAP_PARENT(cursor);
	}

	++heap->n_elements;
	return true;
}

void *
cix_heap_peek(struct cix_heap *heap)
{

	return heap->n_elements == 0 ? NULL : heap->elements[0].item;
}

void *
cix_heap_pop(struct cix_heap *heap)
{
	void *result;
	size_t cursor, left, right;

	if (heap->n_elements == 0)
		return NULL;
	
	result = heap->elements[0].item;
	heap->elements[0] = heap->elements[heap->n_elements--];

	cursor = 0;
	left = 1;
	right = 2;
	while (left < heap->n_elements) {
		size_t swap;

		if (right < heap->n_elements &&
		    CIX_HEAP_COMPARE(heap, cursor, right) == false) {
			swap = CIX_HEAP_COMPARE(heap, right, left) ?
			    right : left;
		} else if (CIX_HEAP_COMPARE(heap, cursor, left) == false) {
			swap = left;
		} else {
			break;
		}

		CIX_HEAP_SWAP(heap, cursor, swap);
		cursor = swap;
		left = CIX_HEAP_CHILDL(cursor);
		right = CIX_HEAP_CHILDR(cursor);
	}

	return result;
}

void
cix_heap_destroy(struct cix_heap *heap)
{

	free(heap->elements);
}
