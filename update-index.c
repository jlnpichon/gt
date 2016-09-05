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
	fprintf(stderr, "usage: %s [--add|-a] [--help|-h] [--] <file>...\n", program);

	return return_value;
}

int main(int argc, char *argv[])
{
	int add;
	int i;
	int stop_options;
	int verbose;
	struct index *index;	
	uint8_t sha1[20];
	char *error;

	if (argc < 2)
		return usage(argv[0], 1, NULL);

	index = index_open(&error);
	if (!index) {
		fprintf(stderr, "%s\n", error);
		free(error);
		return 1;
	}

	add = stop_options = verbose = 0;
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (!stop_options) {
			if (!strncmp(arg, "--add", sizeof("--add")) ||
					!strncmp(arg, "-a", sizeof("-a"))) {
				add = 1;
				continue;
			}
			if (!strncmp(arg, "--help", sizeof("--help")) ||
					!strncmp(arg, "-h", sizeof("-h"))) {
				return usage(argv[0], 0, NULL);
			}
			if (!strncmp(arg, "--verbose", sizeof("--verbose")) ||
					!strncmp(arg, "-v", sizeof("-v"))) {
				verbose = 1;
				continue;
			}
			if (!strncmp(arg, "--", sizeof("--"))) {
				stop_options = 1;
				continue;
			}

			if (arg[0] == '-')
				return usage(argv[0], 1, "Unknown option '%s'", arg);
		}

		if (!add)
			return usage(argv[0], 1, "An action must be provided (--add|-a)");

		if (index_file_add(index, arg, sha1, &error) < 0) {
			fprintf(stderr, "index_file_add '%s' fail: %s\n", arg, error);
			free(error);
			continue;
		}
		if (verbose)
			fprintf(stdout, "%s %s\n", sha12hex(sha1), arg);
	}


	index_close(index, &error);

	return 0;
}
