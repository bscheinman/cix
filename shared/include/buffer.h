#ifndef _CIX_BUFFER_H
#define _CIX_BUFFER_H

#include <stdbool.h>
#include <stdlib.h>

struct cix_buffer {
	size_t length;
	size_t capacity;
	unsigned char data[];
};

bool cix_buffer_init(struct cix_buffer **, size_t);
bool cix_buffer_append(struct cix_buffer **, void *, size_t);
void cix_buffer_drain(struct cix_buffer *, size_t);
void cix_buffer_destroy(struct cix_buffer **);

struct cix_buffer_result {
	enum {
		CIX_BUFFER_OK = 0,
		CIX_BUFFER_PARTIAL,
		CIX_BUFFER_BLOCKED,
		CIX_BUFFER_ERROR
	} code;

	union {
		size_t bytes;
		enum {
			CIX_BUFFER_ERROR_ALLOC,
			CIX_BUFFER_ERROR_FD
		} error;
	} value;
};

void cix_buffer_fd_read(struct cix_buffer **, int, size_t, unsigned long,
    struct cix_buffer_result *);
void cix_buffer_fd_write(struct cix_buffer *, int, size_t, unsigned long,
    struct cix_buffer_result *);

static inline const unsigned char *
cix_buffer_data(const struct cix_buffer *buf)
{

	return buf->data;
}

static inline size_t
cix_buffer_length(const struct cix_buffer *buf)
{

	return buf->length;
}


#endif /* _CIX_BUFFER_H */
