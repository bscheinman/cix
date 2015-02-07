#include "buffer.h"

size_t
bix_buffer_size(const struct cix_buffer *buffer)
{


}

bool
cix_buffer_init(struct cix_buffer *buffer, size_t size)
{

	buffer->data = malloc(size);
	if (buffer->data == NULL)
		return false;

	buffer->size = size;
	buffer->start = buffer->data;
	buffer->end = buffer->data;
	return true;
}

bool
cix_buffer_write(struct cix_buffer *buffer, void *data, size_t bytes)
{

	if (

}
