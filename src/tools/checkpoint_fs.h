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

bool _cm_git_available(void);
void _cm_parse_shortstat(const char *stat_line, long *files, long *ins, long *del);
int _dir_file_count(const char *path);
long _dir_size_bytes(const char *path);
char* _normalize_path(const char *path_value);
bool _validate_commit_hash(const char *commit_hash);
bool _validate_file_path(const char *file_path, const char *working_dir);

#endif /* CHECKPOINT_FS_H */
