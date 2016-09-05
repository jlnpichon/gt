#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "index.h"

static int usage(const char *program,
		int return_value,
		const char *message, ...)
{
	if (message) {
		va_list ap;

		va_start(ap, message);
		vfprintf(stderr, message, ap);
		va_end(ap);
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "usage: %s <sha1>...\n", program);

	return return_value;
}

int main(int argc, char *argv[])
{
	char *error;
	int i;
	uint64_t buffer_bytes;
	uint8_t *buffer;
	uint8_t sha1[20];

	if (argc < 2 || hex2sha1(argv[1], sha1) < 0)
		return usage(argv[0], 1, NULL);

	buffer = file_sha1_read(sha1, &buffer_bytes, &error);
	if (!buffer) {
		fprintf(stderr, "%s", error);
		free(error);
		return 1;
	}

	for (i = 0; i < buffer_bytes; i++)
		fprintf(stdout, "%c", buffer[i]);
	free(buffer);

	return 0;
}
