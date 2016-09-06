#ifndef BUFFER_H
#define BUFFER_H

#include <inttypes.h>
#include <stdlib.h>

struct buffer {
	uint8_t *data;
	size_t data_bytes;
	size_t allocated;
};

#define DECLARE_BUFFER(_b) \
	struct buffer _b = { .data = NULL, .data_bytes = 0, .allocated = 0 }

int buffer_init(struct buffer *buffer);
void buffer_uninit(struct buffer *buffer);

int buffer_concat(struct buffer *buffer, void *data, size_t bytes);
int buffer_sprintf(struct buffer *buffer, const char *fmt, ...);

int buffer_seek(struct buffer *buffer, int offset);

#endif /* BUFFER_H */
