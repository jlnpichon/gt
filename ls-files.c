#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "index.h"

int main(int argc, char *argv[])
{
	int i;
	struct index *index;
	char *error;

	index = index_open(&error);
	if (!index) {
		fprintf(stderr, "%s\n", error);
		free(error);
		return 1;
	}

	for (i = 0; i < index->entries_count; i++) {
		struct index_entry *entry = index->entries[i];
		fprintf(stdout, "%o %s %.*s\n",
				entry->st_mode, sha12hex(entry->sha1), entry->name_bytes, entry->name);
	}

	return 0;
}
