#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "index.h"

#define CTIME_CHANGED 0x01
#define MTIME_CHANGED 0x02
#define INODE_CHANGED 0x04
#define MODE_CHANGED  0x08
#define OWNER_CHANGED 0x10
#define SIZE_CHANGED  0x20

int stat_changed(struct index_entry *entry, struct stat *st)
{
	int changes = 0;

	if (entry->ctime.seconds != st->st_ctim.tv_sec)
		changes |= CTIME_CHANGED;
	if (entry->mtime.seconds != st->st_mtim.tv_sec)
		changes |= MTIME_CHANGED;
	if (entry->st_dev != st->st_dev ||
			entry->st_ino != st->st_ino)
		changes |= INODE_CHANGED;
	if (entry->st_mode != st->st_mode)
		changes |= MODE_CHANGED;
	if (entry->st_uid != st->st_uid ||
			entry->st_gid != st->st_gid)
		changes |= OWNER_CHANGED;
	if (entry->st_size != st->st_size)
		changes |= SIZE_CHANGED;

	return changes;
}

static int diff_empty_show(struct index_entry *entry)
{
	char *error;
	uint8_t *buffer;
	uint8_t *c;
	uint64_t buffer_bytes;
	int line;
	int newline;

	buffer = file_sha1_read(entry->sha1, &buffer_bytes, &error);
	if (!buffer) {
		fprintf(stderr, "Can't open sha1 file '%s': %s\n",
				sha12hex(entry->sha1), error);
		free(error);
		return -1;
	}

	c = buffer;
	line = 0;
	while (c != buffer + buffer_bytes) {
		if (*c == '\n')
			line++;
		c++;
	}

	fprintf(stdout , "--- %s\n", entry->name);
	fprintf(stdout , "+++ /dev/null\n");
	fprintf(stdout, "@@ -1,%d +0,0 @@\n", line);

	c = buffer;
	newline = 1;
	while (c != buffer + buffer_bytes) {
		if (newline) {
			fprintf(stdout, "-");
			newline = 0;
		}
		fprintf(stdout, "%c", *c);
		if (*c == '\n')
			newline = 1;
		c++;
	}

	free(buffer);

	return 0;
}

static int diff_show(uint8_t *sha1, const char *filename)
{
	char *error;
	FILE *f;
	uint8_t *buffer;
	uint64_t buffer_bytes;
	char diff_command[128];

	buffer = file_sha1_read(sha1, &buffer_bytes, &error);
	if (!buffer) {
		fprintf(stderr, "fail to read sha1 blob '%s': %s\n",
				sha12hex(sha1), error);
		free(error);
		return -1;
	}

	snprintf(diff_command, sizeof(diff_command), "diff -L %s -Nu - %s", filename, filename);
	f = popen(diff_command, "w");
	if (!f) {
		fprintf(stderr, "popen fail: %s\n", strerror(errno));
		return -1;
	}

	while (buffer_bytes > 0) {
		size_t written;

		written = fwrite(buffer, sizeof(char), buffer_bytes, f);
		if (written < 0) {
			fprintf(stderr, "fwrite fail: %s\n", strerror(errno));
			pclose(f);
			free(buffer);
			return -1;
		}
		buffer_bytes -= written;
	}

	pclose(f);
	free(buffer);

	return 0;
}

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
		struct stat st;
		int changes;

		if (stat(entry->name, &st) < 0) {
			if (errno != ENOENT) {
				fprintf(stderr, "stat(2) fail: %s\n", strerror(errno));
				return 1;
			}
			diff_empty_show(entry);
			continue;
		}

		changes = stat_changed(entry, &st);
		if (changes)
			diff_show(entry->sha1, entry->name);
	}

	return 0;
}
