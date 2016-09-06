#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

int fd_read(int fd, uint8_t **buffer, size_t *bytes, char **error)
{
	ssize_t rd, n;
	size_t size;

	*bytes = 0;
	rd = 0;
	size = 512;
	*buffer = malloc(size);
	if (!*buffer)
		return -1;

	for (;;) {
		n = read(fd, *buffer + rd, size - rd);
		if (n == 0)
			break;
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
				continue;
			asprintf(error, "read %d fail: %m", fd);
			goto fail;
		}
		rd += n;

		if (rd == size) {
			uint8_t *ptr;
			ptr = realloc(*buffer, size * 2);
			if (!ptr) {
				goto fail;
			}
			*buffer = ptr;
			size *= 2;
		}
	}

	*bytes = rd;

	return 0;

fail:
	free(*buffer);
	*buffer = NULL;
	return -1;
}

int file_read(const char *filename,
		uint8_t **buffer, size_t *bytes,
		char **error)
{
	int fd;

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		asprintf(error, "open '%s' fail: %m", filename);
		return -1;
	}

	return fd_read(fd, buffer, bytes, error);
}
