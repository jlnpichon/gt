#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#include <inttypes.h>

int file_read(const char *filename,
		uint8_t **buffer, size_t *bytes,
		char **error);

int fd_read(int fd, uint8_t **buffer, size_t *bytes, char **error);

#endif /* COMMON_H */
