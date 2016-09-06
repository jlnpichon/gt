#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"

static int buffer_grow(struct buffer *buffer, size_t needed)
{
	void *ptr;
	size_t new_size;

	if (buffer->allocated > needed)
		return 0;

	new_size = buffer->allocated * 2;
	while (new_size < needed)
		new_size *= 2;

	ptr = realloc(buffer->data, new_size);
	if (!ptr)
		return -1;

	buffer->data = ptr;
	buffer->allocated = new_size;

	return 0;
}

int buffer_sprintf(struct buffer *buffer, const char *fmt, ...)
{
	int count;
	int room;
	va_list ap;

retry:
	room = buffer->allocated - buffer->data_bytes;
	va_start(ap, fmt);
	count = vsnprintf((char *) buffer->data + buffer->data_bytes,
			room, fmt, ap);
	va_end(ap);

	if (count >= room) {
		if (buffer_grow(buffer, buffer->data_bytes + count) < 0)
			return -1;
		goto retry;
	}

	buffer->data_bytes += count;

	return 0;
}

int buffer_concat(struct buffer *buffer, void *data, size_t bytes)
{
	if (buffer_grow(buffer, buffer->data_bytes + bytes) < 0)
		return -1;

	memcpy(buffer->data + buffer->data_bytes, data, bytes);
	buffer->data_bytes += bytes;

	return 0;
}

int buffer_init(struct buffer *buffer)
{
	buffer->allocated = 2;
	buffer->data_bytes = 0;
	buffer->data = malloc(buffer->allocated);
	if (!buffer->data) {
		buffer->allocated = 0;
		return -1;
	}
	return 0;
}

void buffer_uninit(struct buffer *buffer)
{
	if (buffer->data)
		free(buffer->data);
	buffer->allocated = buffer->data_bytes = 0;
}

int buffer_seek(struct buffer *buffer, int offset)
{
	if (buffer_grow(buffer, offset + 1) < 0)
		return -1;

	buffer->data_bytes = offset;

	return 0;
}
