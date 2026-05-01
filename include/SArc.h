#pragma once

#include <stdint.h>

#define SARC_MAGIC 0x53417263
#define SARC_VERSION 0x01

#ifndef SARC_MALLOC
#define SARC_MALLOC malloc
#endif
#ifndef SARC_REALLOC
#define SARC_REALLOC realloc
#endif
#ifndef SARC_FREE
#define SARC_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	const char *filename;
	uint8_t *data;
	uint32_t size;
	uint32_t _capacity;
} SArchiveFile;

typedef struct {
	uint32_t file_count;
	uint32_t _capacity;
	SArchiveFile *files;
} SArchive;

SArchive *sarc_create(uint32_t capacity);
void sarc_free(SArchive *archive);

SArchive *sarc_load_memory(const uint8_t *input);
void sarc_write_archive(SArchive *archive, uint8_t *output);

void sarc_add_file(SArchive *archive, SArchiveFile file);
void sarc_remove_file(SArchive *archive, uint32_t index);

#ifdef __cplusplus
}
#endif