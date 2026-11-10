/*
 * file_batch_rename.h - batch rename wildcard helpers.
 *
 * Module-internal header (file_batch.c <-> file_batch_rename.c).
 */
#ifndef FILE_BATCH_RENAME_H
#define FILE_BATCH_RENAME_H

#include <stddef.h>
#include <stdbool.h>

char * apply_rename_pattern(const char *pattern, const char *dest_pattern, const char *filename);

#endif /* FILE_BATCH_RENAME_H */
