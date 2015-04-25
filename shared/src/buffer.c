#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buffer.h"

#define CIX_BUFFER_FD_DEFAULT_SIZE (1 << 14)

bool
cix_buffer_init(struct cix_buffer **b, size_t size)
{
	struct cix_buffer *buffer = malloc(sizeof(*buffer) + size);

	if (buffer == NULL)
		return false;

	buffer->length = 0;
	buffer->capacity = size;
	*b = buffer;
	return true;
}

static bool
cix_buffer_expand(struct cix_buffer **b, size_t size)
{
	struct cix_buffer *buffer = *b;
	size_t new_capacity;
	struct cix_buffer *new_buffer;

	if (buffer->capacity >= buffer->length + size) {
		return true;
	}

	new_capacity = buffer->capacity << 1;
	while (new_capacity < buffer->length + size) {
		new_capacity <<= 1;
	}

	new_buffer = realloc(buffer, sizeof(*buffer) + new_capacity);
	if (new_buffer == NULL) {
		return false;
	}

	new_buffer->capacity = new_capacity;
	*b = new_buffer;
	return true;
}

bool
cix_buffer_append(struct cix_buffer **b, void *data, size_t size)
{
	struct cix_buffer *buffer;

	if (cix_buffer_expand(b, size) == false) {
		return false;
	}

	buffer = *b;
	memcpy(buffer->data + buffer->length, data, size);
	buffer->length += size;
	return true;
}

void
cix_buffer_drain(struct cix_buffer *buffer, size_t size)
{

	assert(size <= buffer->length);
	memmove(buffer->data, buffer->data + buffer->length,
	    buffer->length- size);
	buffer->length -= size;
	return;
}

void
cix_buffer_destroy(struct cix_buffer **buffer)
{

	free(*buffer);
	*buffer = NULL;
	return;
}

void
cix_buffer_fd_read(struct cix_buffer **b, int fd, size_t bytes,
    unsigned long flags, struct cix_buffer_result *result)
{
	size_t target;

	(void)flags;

	result->value.bytes = 0;

	if (bytes > 0) {
		target = bytes;
	} else {
		target = CIX_BUFFER_FD_DEFAULT_SIZE;
	}

	for (;;) {
		struct cix_buffer *buffer;
		ssize_t r;
		size_t bytes_read;

		if (cix_buffer_expand(b, target) == false) {
			result->code = CIX_BUFFER_ERROR;
			return;
		}

		buffer = *b;
		r = read(fd, buffer->data + buffer->length, target);

		if (r < 0) {
			fprintf(stderr, "Error reading from socket: %s\n",
			    strerror(errno));
			result->code = CIX_BUFFER_ERROR;
			result->value.error = CIX_BUFFER_ERROR_FD;
			return;
		}

		bytes_read = (size_t)r;
		buffer->length += bytes_read;

		if (bytes > 0) {
			if (result->value.bytes == bytes) {
				result->code = CIX_BUFFER_OK;
			} else {
				result->code = CIX_BUFFER_PARTIAL;
			}
			result->value.bytes = bytes_read;
			return;
		}

		if (bytes_read < target) {
			result->code = CIX_BUFFER_OK;
			result->value.bytes += bytes_read;
			return;
		}
	}

	return;
}

void
cix_buffer_fd_write(struct cix_buffer *buffer, int fd, size_t bytes,
    unsigned long flags, struct cix_buffer_result *result)
{
	size_t target, bytes_written;
	ssize_t w;

	(void)flags;

	result->value.bytes = 0;

	target = cix_buffer_length(buffer);
	if (bytes > 0 && bytes < target) {
		target = bytes;
	}

	w = write(fd, buffer->data, target);

	if (w < 0) {
		fprintf(stderr, "Error writing to socket: %s\n",
		    strerror(errno));
		result->code = CIX_BUFFER_ERROR;
		result->value.error = CIX_BUFFER_ERROR_FD;
		return;
	}

	bytes_written = (size_t)w;
	cix_buffer_drain(buffer, bytes_written);
	
	if (bytes_written == target) {
		result->code = CIX_BUFFER_OK;
	} else {
		result->code = CIX_BUFFER_PARTIAL;
	}
	result->value.bytes = bytes_written;
	return;
}
