/*
 * file_batch_fs.h - file-batch filesystem operation wrappers.
 *
 * Module-internal header (file_batch.c <-> file_batch_fs.c).
 */
#ifndef FILE_BATCH_FS_H
#define FILE_BATCH_FS_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "hermes_json.h"   /* json_t */

#define COPY_BUF_SIZE 65536

const char * check_file_access(const char *path, bool need_write);
const char * check_parent_writable(const char *path);
bool chmod_file(const char *path, mode_t mode);
bool copy_file(const char *src, const char *dst);
bool delete_file(const char *path);
json_t * hash_file_json(const char *path);
bool move_file(const char *src, const char *dst);
mode_t parse_mode_string(const char *mode_str);
json_t * stat_file_json(const char *path);
bool touch_file(const char *path);

#endif /* FILE_BATCH_FS_H */
