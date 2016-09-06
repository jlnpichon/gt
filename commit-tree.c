#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "index.h"
#include "buffer.h"
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
	fprintf(stderr, "usage: %s <sha1> [--help|-h] [--parent|-p <sha1>]* < changelog\n", program);

	return return_value;
}

#define PARENTS_MAX 20

int tree_commit(uint8_t *tree_sha1, uint8_t *commit_sha1,
		char *author, char *committer, char *date,
		uint8_t parents[PARENTS_MAX][20], size_t parents_count,
		char *comment,
		char **error)
{
	int i;
	size_t commit_size;
	char ascii_size[16];
	size_t offset;
	size_t prefix_bytes;
	DECLARE_BUFFER(buffer);

	buffer_init(&buffer);
	offset = sizeof("commit ") + sizeof(ascii_size) + 1;
	buffer_seek(&buffer, offset);

	buffer_sprintf(&buffer, "tree %s\n", sha12hex(tree_sha1));
	for (i = 0; i < parents_count; i++)
		buffer_sprintf(&buffer, "parent %s\n", sha12hex(parents[i]));

	buffer_sprintf(&buffer, "author %s %s\n", author, date);
	buffer_sprintf(&buffer, "committer %s %s\n\n", committer, date);
	buffer_sprintf(&buffer, "%s", comment);

	commit_size = buffer.data_bytes - offset;
	sprintf(ascii_size, "%ld", commit_size);
	prefix_bytes = strlen(ascii_size) + sizeof("commit ");
	offset -= prefix_bytes;
	buffer_seek(&buffer, offset);
	buffer_sprintf(&buffer, "commit %ld", commit_size);

	file_sha1_write(buffer.data + offset, commit_size + prefix_bytes,
			commit_sha1, error);
	buffer_uninit(&buffer);

	return 0;
}

int main(int argc, char *argv[])
{
	int i;
	int stop_options;
	uint8_t parents[PARENTS_MAX][20];
	size_t parents_count;
	uint8_t *comment;
	char *error;
	uint8_t tree_sha1[20];
	uint8_t commit_sha1[20];
	size_t comment_bytes;
	struct passwd *pw;
	char date[20];

	uid_t uid;

	if (argc < 2)
		return usage(argv[0], 1, "missing argument");

	if (hex2sha1(argv[1], tree_sha1) < 0)
		return usage(argv[0], 0, "invalid sha1: %s", argv[1]);

	stop_options = parents_count = 0;
	for (i = 2; i < argc; i += 2) {
		const char *arg_name = argv[i];
		const char *arg_value = argv[i + 1];

		if (!stop_options) {
			if (!strncmp(arg_name, "--parent", sizeof("--parent")) ||
					!strncmp(arg_name, "-p", sizeof("-p"))) {
					if (hex2sha1(arg_value, parents[parents_count++]) < 0)
						return usage(argv[0], 0, "invalid sha1: %s", arg_value);
					continue;
			}
			if (!strncmp(arg_name, "--help", sizeof("--help")) ||
					!strncmp(arg_name, "-h", sizeof("-h"))) {
				return usage(argv[0], 0, NULL);
			}
			if (!strncmp(arg_name, "--", sizeof("--"))) {
				stop_options = 1;
				break;
			}

			if (arg_name[0] == '-')
				return usage(argv[0], 1, "Unknown option %s", arg_name);
		}
	}

	uid = geteuid();
	pw = getpwuid(uid);
	snprintf(date, sizeof(date), "%ld", time(NULL));

	if (fd_read(STDIN_FILENO, &comment, &comment_bytes, &error) < 0) {
		fprintf(stderr, "%s", error);
		free(error);
		return 1;
	}

	/* TODO: check that the sha1 object actually exist and is a tree */
	if (tree_commit(tree_sha1, commit_sha1, pw->pw_name, pw->pw_name, date,
			parents, parents_count, (char *) comment, &error) < 0) {
		fprintf(stderr, "%s", error);
		free(error);
		return 1;
	}

	fprintf(stdout, "%s\n", sha12hex(commit_sha1));

	free(comment);

	return 0;
}
