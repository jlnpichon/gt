#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"
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
	fprintf(stderr, "usage: %s\n", program);

	return return_value;
}

int tree_write(struct index *index, uint8_t *sha1, char **error)
{
	int i;
	size_t tree_size;
	char ascii_size[16];
	size_t offset;
	size_t prefix_bytes;
	DECLARE_BUFFER(buffer);

	buffer_init(&buffer);
	offset = sizeof("tree ") + sizeof(ascii_size) + 1;
	buffer_seek(&buffer, offset);
	for (i = 0; i < index->entries_count; i++) {
		struct index_entry *entry = index->entries[i];

		buffer_sprintf(&buffer, "%o %.*s", entry->st_mode, entry->name_bytes, entry->name);
		buffer_concat(&buffer, "\0", 1);
		buffer_concat(&buffer, entry->sha1, sizeof(entry->sha1));
	}

	tree_size = buffer.data_bytes - offset;
	sprintf(ascii_size, "%ld", tree_size);
	prefix_bytes = strlen(ascii_size) + sizeof("tree ");
	offset -= prefix_bytes;
	buffer_seek(&buffer, offset);
	buffer_sprintf(&buffer, "tree %ld", tree_size);

	file_sha1_write(buffer.data + offset, tree_size + prefix_bytes, sha1, error);
	buffer_uninit(&buffer);

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	char *error;
	struct index *index;
	uint8_t sha1[20];

	index = index_open(&error);
	if (!index) {
		fprintf(stderr, "%s\n", error);
		free(error);
		return 1;
	}

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (!strncmp(arg, "--help", sizeof("--help")) ||
				!strncmp(arg, "-h", sizeof("--h")))
				return usage(argv[0], 1, NULL);

		if (arg[0] == '-')
			return usage(argv[0], 1, "Unknown option '%s'", arg);
	}

	if (tree_write(index, sha1, &error) < 0) {
		fprintf(stderr, "%s", error);
		free(error);
		return 1;
	}

	fprintf(stdout, "%s\n", sha12hex(sha1));

	index_close(index, &error);

	return 0;
}
