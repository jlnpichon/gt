#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <openssl/sha.h>
#include <zlib.h>

#include "common.h"
#include "index.h"

/* The index represents a place where you want to put your files before commiting.
 * It is a staging area where the new commit is prepared. The entries in the
 * index are kept in ascending order of name */

#define GT_DEFAULT_DIRECTORY "./.gt"

static int index_header_check(struct index_header *header, size_t size)
{
	SHA_CTX ctx;
	uint8_t sha1[20];

	if (header->signature != GT_SIGNATURE)
		return 0;
	if (header->version != GT_VERSION)
		return 0;

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, header, offsetof(struct index_header, sha1));
	SHA1_Update(&ctx, header + 1, size - sizeof(struct index_header));
	SHA1_Final(sha1, &ctx);

	if (memcmp(header->sha1, sha1, sizeof(sha1)))
		return 0;

	return 1;
}

/* make sure the directory ./.gt/objects/xx exists */
static int object_directory_create(const char *filename, char **error)
{
	char directory[PATH_MAX];
	
	strcpy(directory, filename);
	dirname(directory);

	if (mkdir(directory, 0774) < 0) {
		asprintf(error, "mkdir '%s' fail: %m", directory);
		return -1;
	}

	return 0;
}

int gt_directory_check(char **error)
{
	char *directory;
	char objects[PATH_MAX];
	struct stat st;

	if (!(directory = getenv("GT_DIRECTORY")))
		directory = GT_DEFAULT_DIRECTORY;

	if (stat(directory, &st) < 0) {
		if (errno == ENOENT)
			asprintf(error, "missing gt directory '%s'", directory);
		else
			asprintf(error, "stat '%s' fail: %m", directory);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		asprintf(error, "file '%s' is not a directory", directory);
		return 0;
	}

	snprintf(objects, sizeof(objects), "%s/objects", directory);
	if (stat(objects, &st) < 0) {
		if (errno == ENOENT)
			asprintf(error, "missing gt directory '%s'", objects);
		else
			asprintf(error, "stat '%s' fail: %m", objects);
		return 0;
	}
	if (!S_ISDIR(st.st_mode)) {
		asprintf(error, "file '%s' is not a directory", objects);
		return 0;
	}

	return 1;
}

int exact_write(int fd,
		const void *data, size_t bytes,
		char **error)
{
	ssize_t written, n;

	written = 0;
	while (written < bytes) {
		n = write(fd, data + written, bytes - written);
		if (n < 0) {
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
				continue;
			asprintf(error, "write fail: %m");
			return -1;
		}
		written += n;
	}

	return 0;
}

static int buffer_deflate(uint8_t *buffer, size_t buffer_bytes,
		uint8_t **deflated, size_t *deflated_bytes)
{
	z_stream stream;
	size_t size;

	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, Z_BEST_COMPRESSION);
	size = deflateBound(&stream, buffer_bytes);
	*deflated = malloc(size * sizeof(uint8_t));
	if (!*deflated)
		return -1;

	stream.next_in = buffer;
	stream.avail_in = buffer_bytes;
	stream.next_out = *deflated;
	stream.avail_out = size;
	while (deflate(&stream, Z_FINISH) == Z_OK);
	deflateEnd(&stream);
	*deflated_bytes = stream.total_out;

	return 0;
}

char *sha12hex(uint8_t *sha1)
{
	int i;
	static char sha1_ascii[41];

	for (i = 0; i < 20; i++) {
		static char *hex = "0123456789abcdef";
		sha1_ascii[i * 2] = hex[(sha1[i] >> 4) & 0x0f];
		sha1_ascii[i * 2 + 1] = hex[sha1[i] & 0x0f];
	}
	sha1_ascii[40] = '\0';

	return sha1_ascii;
}

static uint8_t hexval(uint8_t c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	else
		return -1;
}

int hex2sha1(const char *hex, uint8_t *sha1)
{
	int i;

	if (strlen(hex) != 40)
		return -1;

	for (i = 0; i < 20; i++) {
		if (*hex == '\0' || !isxdigit(*hex))
			return -1;
		sha1[i] = (hexval(hex[2 * i]) << 4) & 0xf0;
		sha1[i] |= hexval(hex[2 * i + 1]) & 0x0f;
	}

	return 0;
}

char *sha1_filename(uint8_t *sha1)
{
	char *directory;
	char *sha1_ascii;
	static char filename[PATH_MAX];

	if (!(directory = getenv("GT_DIRECTORY")))
		directory = GT_DEFAULT_DIRECTORY;

	sha1_ascii = sha12hex(sha1);
	snprintf(filename, PATH_MAX, "%s/objects/%c%c/%s",
			directory, sha1_ascii[0], sha1_ascii[1], &sha1_ascii[2]);

	return filename;
}

void *file_sha1_map(uint8_t *sha1, size_t *map_bytes, char **error)
{
	int fd;
	void *map;
	const char *filename;
	struct stat st;

	*map_bytes = 0;
	filename = sha1_filename(sha1);
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		asprintf(error, "open '%s' fail: %m", filename);
		return NULL;
	}
	if (fstat(fd, &st) < 0) {
		close(fd);
		return NULL;
	}
	map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == (void *) -1)
		return NULL;
	*map_bytes = st.st_size;

	return map;
}

uint8_t *file_sha1_inflate(void *map, size_t map_bytes, uint64_t *buffer_bytes)
{
	int result;
	char chunk[8192];
	char type[32];
	int bytes;
	uint8_t *buffer;
	z_stream stream;

	memset(&stream, 0, sizeof(stream));
	stream.next_in = map;
	stream.avail_in = map_bytes;
	stream.next_out = (uint8_t *) chunk;
	stream.avail_out = sizeof(chunk);

	inflateInit(&stream);
	result = inflate(&stream, 0);
	if (sscanf(chunk, "%10s %lu", type, buffer_bytes) != 2)
		return NULL;

	bytes = strlen(chunk) + 1;
	buffer = malloc(*buffer_bytes);
	if (!buffer)
		return NULL;

	memcpy(buffer, chunk + bytes, stream.total_out - bytes);
	bytes = stream.total_out - bytes;
	if (bytes < *buffer_bytes && result == Z_OK) {
		stream.next_out = buffer + bytes;
		stream.avail_out = *buffer_bytes - bytes;
		while (inflate(&stream, Z_FINISH) == Z_OK);
	}
	inflateEnd(&stream);
	return buffer;
}

uint8_t *file_sha1_read(uint8_t *sha1, uint64_t *buffer_bytes, char **error)
{
	void *map;
	uint8_t *buffer;
	size_t map_bytes;

	map = file_sha1_map(sha1, &map_bytes, error);
	if (!map) {
		*buffer_bytes = 0;
		return NULL;
	}
	buffer = file_sha1_inflate(map, map_bytes, buffer_bytes);
	munmap(map, map_bytes);

	return buffer;
}

int buffer_sha1_write(uint8_t *sha1,
		uint8_t *buffer, size_t bytes,
		char **error)
{
	char *filename;
	int fd;

	filename = sha1_filename(sha1);

retry:
	fd = open(filename, O_CREAT|O_EXCL|O_WRONLY, 0444);
	if (fd < 0) {
		if (errno == EEXIST) {
			return 0;
		} else if (errno == ENOENT) {
			if (object_directory_create(filename, error) < 0)
			 return -1;
			goto retry;
		} else {
			asprintf(error, "open '%s' %m", filename);
			return -1;
		}
	}

	if (exact_write(fd, buffer, bytes, error) < 0)
		return -1;

	close(fd);

	return 0;
}

int buffer_sha1(uint8_t *buffer, size_t bytes,
		int write, uint8_t *sha1,
		char **error)
{
	uint8_t *deflated;
	size_t deflated_bytes;
	SHA_CTX ctx;

	buffer_deflate(buffer, bytes, &deflated, &deflated_bytes);

	SHA1_Init(&ctx);
	SHA1_Update(&ctx, deflated, deflated_bytes);
	SHA1_Final(sha1, &ctx);

	if (write && buffer_sha1_write(sha1, deflated, deflated_bytes, error) < 0) {
		free(deflated);
		return -1;
	}

	free(deflated);

	return 0;
}

int file_sha1_write(uint8_t *buffer, size_t bytes, uint8_t *sha1, char **error)
{
	return buffer_sha1(buffer, bytes, 1, sha1, error);
}

int object_hash(uint8_t *buffer, size_t bytes, char *type,
		int write, uint8_t *sha1,
		char **error)
{
	uint8_t *object;
	size_t object_bytes;
	size_t offset;
	char size_ascii[16];

	sprintf(size_ascii, "%ld", bytes);
	offset = strlen(type) + strlen(size_ascii) + 2;
	object_bytes = bytes + offset;

	object = malloc(object_bytes);
	sprintf((char *) object, "%s %ld", type, bytes);
	memcpy(object + offset, buffer, bytes);

	if (buffer_sha1(object, object_bytes, write, sha1, error) < 0) {
		free(object);
		return -1;
	}

	free(object);

	return 0;
}

struct index *index_open(char **error)
{
	int fd;
	int i;
	size_t size;
	char filename[PATH_MAX];
	struct index *index;
	struct index_header *header;
	struct stat st;
	void *map;
	char *directory;
	void *offset;

	if (!(directory = getenv("GT_DIRECTORY")))
		directory = GT_DEFAULT_DIRECTORY;

	index = calloc(1, sizeof(*index));
	if (!index) {
		if (error)
			asprintf(error, "calloc: %m");
		return NULL;
	}

	snprintf(filename, sizeof(filename), "%s/index", directory);
	index->path = strdup(filename);

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return index;
		if (error)
			asprintf(error, "open '%s': %m", filename);
		return NULL;
	}

	map = (void *) -1;
	if (!fstat(fd, &st)) {
		size = st.st_size;
		if (size >= sizeof(struct index_header))
			map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	}
	close(fd);
	if (map == (void *) -1) {
		if (error)
			asprintf(error, "%m");
		return NULL;
	}

	header = (struct index_header *) map;
	if (!index_header_check(header, size)) {
		munmap(map, size);
		return NULL;
	}

	index->entries_count = header->entries_count;
	index->entries = calloc(1, index->entries_count * sizeof(void *));
	offset = header + 1;
	for (i = 0; i < index->entries_count; i++) {
		struct index_entry *entry = offset;
		size_t entry_bytes = sizeof(*entry) + entry->name_bytes;

		index->entries[i] = malloc(entry_bytes);
		memcpy(index->entries[i], entry, entry_bytes);
		offset += entry_bytes;
	}

	munmap(map, size);

	return index;
}

int index_close(struct index *index, char **error)
{
	struct index_header header;
	int fd;
	int i;
	SHA_CTX ctx;

	fd = open(index->path, O_WRONLY|O_CREAT, 0774);
	if (fd < 0) {
		if (error)
			asprintf(error, "open '%s': %m", index->path);
		return -1;
	}

	header.signature = GT_SIGNATURE;
	header.version = GT_VERSION;
	header.entries_count = index->entries_count;
	SHA1_Init(&ctx);
	SHA1_Update(&ctx, &header, offsetof(struct index_header, sha1));

	for (i = 0; i < header.entries_count; i++) {
		struct index_entry *entry = index->entries[i];
		SHA1_Update(&ctx, entry, sizeof(*entry) + entry->name_bytes);
	}
	SHA1_Final(header.sha1, &ctx);

	exact_write(fd, &header, sizeof(header), error);
	for (i = 0; i < header.entries_count; i++) {
		struct index_entry *entry = index->entries[i];
		exact_write(fd, entry, sizeof(*entry) + entry->name_bytes, error);
		free(entry);
	}

	close(fd);

	free(index->entries);
	free(index->path);
	free(index);

	return 0;
}

int fd_hash(int fd,
		int write, uint8_t *sha1,
		char **error)
{
	uint8_t *buffer;
	size_t bytes;

	if (fd_read(fd, &buffer, &bytes, error) < 0)
		return -1;

	if (object_hash(buffer, bytes, "blob", write, sha1, error) < 0)
		return -1;

	free(buffer);

	return 0;
}

static int name_binary_search(struct index *index, char *name, size_t name_bytes)
{
	int l, r;

	if (index->entries_count == 0)
		return -0 - 1;

	l = 0;
	r = index->entries_count;
	while (l < r) {
		int result;
		int m;
		struct index_entry *e;

		m = (l + r) / 2;
		e = index->entries[m];
		result = strncmp(name, e->name, name_bytes);
		if (!result)
			return m;
		if (result < 0) {
			r = m;
			continue;
		}
		l = m + 1;
	}
	return -l - 1;
}

int index_file_add(struct index *index, const char *filename,
		uint8_t *sha1, char **error)
{
	struct index_entry *entry;
	int fd;
	struct stat st;
	int position;
	
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		asprintf(error, "open '%s' fail: %m", filename);
		return -1;
	}
	if (fstat(fd, &st) < 0) {
		asprintf(error, "fstat '%s' fail: %m", filename);
		close(fd);
		return -1;
	}

	if (fd_hash(fd, 1, sha1, error) < 0) {
		close(fd);
		return -1;
	}

	entry = calloc(1, sizeof(*entry) + strlen(filename));
	if (!entry) {
		close(fd);
		return -1;
	}
	entry->st_dev = st.st_dev;
	entry->st_ino = st.st_ino;
	entry->st_mode = st.st_mode;
	entry->st_uid = st.st_uid;
	entry->st_gid = st.st_gid;
	entry->st_size = st.st_size;
	entry->mtime.seconds = st.st_mtim.tv_sec;
	entry->ctime.seconds = st.st_ctim.tv_sec;
	memcpy(entry->sha1, sha1, sizeof(entry->sha1));
	entry->name_bytes = strlen(filename);
	memcpy(entry->name, filename, strlen(filename));

	position = name_binary_search(index, entry->name, entry->name_bytes);
	if (position >= 0) {
		/* Already exist, update the entry */
		free(index->entries[position]);
		index->entries[position] = entry;
	} else {
		void *ptr;

		position = -position - 1;
		ptr = realloc(index->entries, (index->entries_count + 1) * sizeof(void *));
		if (!ptr) {
			free(entry);
			asprintf(error, "realloc fail: %m");
			return -1;
		}
		index->entries = ptr;
		index->entries_count++;
		if (index->entries_count > 1) {
			memmove(index->entries + position + 1, index->entries + position,
					(index->entries_count - position - 1) * sizeof(void *));
		}
		index->entries[position] = entry;
	}

	close(fd);

	return 0;
}
