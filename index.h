#ifndef INDEX_H
#define INDEX_H

#include <stdint.h>

#define GT_SIGNATURE	0x53494D50
#define GT_VERSION		1
#define GT_DEFAULT_DIRECTORY "./.gt"

struct time {
	uint32_t seconds;
	uint32_t nanoseconds;
};

struct index_entry {
	struct time ctime;
	struct time mtime;
	uint32_t st_dev;
	uint32_t st_ino;
	uint32_t st_mode;
	uint32_t st_uid;
	uint32_t st_gid;
	uint32_t st_size;
	uint8_t sha1[20];
	uint16_t name_bytes;
	char name[0];
};

struct index_header {
	uint32_t signature;
	uint32_t version;
	uint32_t entries_count;
	uint8_t sha1[20];
};

struct index {
	char *path;
	uint32_t entries_count;
	struct index_entry **entries; 
};

/* NOTE: these two functions are not reentrant */
char *sha12hex(uint8_t *sha1);
char *sha1_filename(uint8_t *sha1);

int gt_directory_check(char **error);

int object_hash(uint8_t *buffer, size_t bytes, char *type,
		int write, uint8_t *sha1,
		char **error);

struct index *index_open(char **error);
int index_close(struct index *index, char **error);

int index_file_add(struct index *index, const char *filename,
		uint8_t *sha1, char **error);

#endif /* INDEX_H */
