/*
 * checkpoint_fs.h - checkpoint store path / dir helpers.
 *
 * Module-internal header (checkpoint_manager.c <-> checkpoint_fs.c).
 */
#ifndef CHECKPOINT_FS_H
#define CHECKPOINT_FS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

bool cm_git_available(void);
void cm_parse_shortstat(const char *stat_line, long *files, long *ins, long *del);
int dir_file_count(const char *path);
long dir_size_bytes(const char *path);
char* _normalize_path(const char *path_value);
bool validate_commit_hash(const char *commit_hash);
bool validate_file_path(const char *file_path, const char *working_dir);

#endif /* CHECKPOINT_FS_H */
