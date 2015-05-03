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

	result->bytes = 0;

	if (bytes > 0) {
		target = bytes;
	} else {
		target = CIX_BUFFER_FD_DEFAULT_SIZE;
	}

	for (;;) {
		struct cix_buffer *buffer;
		ssize_t r;

		if (cix_buffer_expand(b, target) == false) {
			result->code = CIX_BUFFER_ERROR;
			return;
		}

		buffer = *b;
		r = read(fd, buffer->data + buffer->length, target);

		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}

			fprintf(stderr, "Error reading from socket: %s\n",
			    strerror(errno));
			result->code = CIX_BUFFER_ERROR;
			result->error = CIX_BUFFER_ERROR_FD;
			return;
		}

		if (r == 0) {
			break;
		}

		result->bytes += (size_t)r;
		buffer->length += (size_t)r;

		if (bytes > 0) {
			if (result->bytes == bytes) {
				break;
			}

			if (flags & CIX_BUFFER_FD_RETRY_PARTIAL) {
				continue;
			}

			break;
		} else if (result->bytes < target) {
			break;
		}
	}

	if (result->bytes == bytes || bytes == 0) {
		result->code = CIX_BUFFER_OK;
	} else {
		result->code = CIX_BUFFER_PARTIAL;
	}

	return;
}

void
cix_buffer_fd_write(struct cix_buffer *buffer, int fd, size_t bytes,
    unsigned long flags, struct cix_buffer_result *result)
{
	size_t target;

	result->bytes = 0;

	target = cix_buffer_length(buffer);
	if (target == 0) {
		result->code = CIX_BUFFER_OK;
		return;
	}

	if (bytes > 0 && bytes < target) {
		target = bytes;
	}

	for (;;) {
		ssize_t w = write(fd, buffer->data, target - result->bytes);

		if (w == 0) {
			break;
		}

		if (w > 0) {
			result->bytes += (size_t)w;

			if (result->bytes == target) {
				break;
			}

			if (flags & CIX_BUFFER_FD_RETRY_PARTIAL) {
				continue;
			}

			break;
		}

		if (errno == EINTR) {
			continue;
		}

		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			if (flags & CIX_BUFFER_FD_BLOCK) {
				continue;
			}
			
			result->code = CIX_BUFFER_BLOCKED;
			return;
		}

		fprintf(stderr, "Error writing to socket: %s\n",
		    strerror(errno));
		result->code = CIX_BUFFER_ERROR;
		result->error = CIX_BUFFER_ERROR_FD;
		return;
	}

	cix_buffer_drain(buffer, result->bytes);
	
	if (result->bytes == target) {
		result->code = CIX_BUFFER_OK;
	} else {
		result->code = CIX_BUFFER_PARTIAL;
	}

	return;
}
