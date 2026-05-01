#include "SArc.h"

#include <malloc.h>
#include <string.h>

SArchive *sarc_create(const uint32_t capacity) {
	SArchive *archive = SARC_MALLOC(sizeof(SArchive));
	if (!archive) return NULL;

	archive->files = SARC_MALLOC(sizeof(SArchiveFile) * capacity);
	if (!archive->files) {
		SARC_FREE(archive);
		return NULL;
	}

	archive->file_count = 0;
	archive->_capacity = capacity;

	return archive;
}

void sarc_free(SArchive *archive) {
	for (uint32_t i = 0; i < archive->file_count; i++) {
		SARC_FREE((void*)archive->files[i].filename);
		SARC_FREE(archive->files[i].data);
	}

	SARC_FREE(archive->files);
	SARC_FREE(archive);
}

void sarc_add_file(SArchive *archive, const SArchiveFile file) {
	if (archive->file_count >= archive->_capacity) {
		const uint32_t new_capacity = archive->_capacity == 0 ? 4 : archive->_capacity * 2;
		archive->files = SARC_REALLOC(archive->files, new_capacity * sizeof(SArchiveFile));
		archive->_capacity = new_capacity;
	}

	SArchiveFile *dst = &archive->files[archive->file_count++];

	const size_t len = strlen(file.filename) + 1;
	dst->filename = SARC_MALLOC(len);
	memcpy((void*)dst->filename, file.filename, len);

	dst->data = SARC_MALLOC(file.size);
	memcpy(dst->data, file.data, file.size);

	dst->size = file.size;
}

void sarc_remove_file(SArchive *archive, const uint32_t index) {
	if (index >= archive->file_count) return;

	const SArchiveFile *file = &archive->files[index];

	SARC_FREE((void*)file->filename);
	SARC_FREE(file->data);

	archive->files[index] = archive->files[archive->file_count - 1];
	archive->files[archive->file_count - 1] = (SArchiveFile){0};

	archive->file_count--;
}