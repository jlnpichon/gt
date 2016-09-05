#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "index.h"
#include "common.h"

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
	fprintf(stderr, "usage: %s [--help|-h] [--write|-w] [--] <file>...\n", program);

	return return_value;
}

int main(int argc, char *argv[])
{
	int i;
	int fd;
	int stop_options;
	int write;
	const char *filename;
	uint8_t *buffer;
	char *error;
	uint8_t sha1[20];
	size_t bytes;

	write = stop_options = 0;
	filename = NULL;
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (!stop_options) {
			if (!strncmp(arg, "--write", sizeof("--write")) ||
					!strncmp(arg, "-w", sizeof("-w"))) {
				write = 1;
				continue;
			}
			if (!strncmp(arg, "--help", sizeof("--help")) ||
					!strncmp(arg, "-h", sizeof("-h"))) {
				return usage(argv[0], 0, NULL);
			}
			if (!strncmp(arg, "--", sizeof("--"))) {
				stop_options = 1;
				continue;
			}

			if (arg[0] == '-')
				return usage(argv[0], 1, "Unknown option %s", arg);
		}

		filename = arg;
	}

	if (!filename) {
		fd = STDIN_FILENO;
	} else {
		fd = open(filename, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "open '%s' fail: %m\n", filename);
			return 1;
		}
	}

	if (!gt_directory_check(&error)) {
		fprintf(stderr, "%s\n", error);
		free(error);
		return 1;
	}

	if (fd_read(fd, &buffer, &bytes, &error) < 0) {
		fprintf(stderr, "%s", error);
		free(error);
		return 1;
	}

	if (fd != STDIN_FILENO)
		close(fd);

	if (object_hash(buffer, bytes, "blob", write, sha1, &error) < 0) {
		fprintf(stderr, "%s\n", error);
		free(buffer);
		free(error);
		return 1;
	}

	fprintf(stdout, "%s\n", sha12hex(sha1));

	free(buffer);

	return 0;
}
