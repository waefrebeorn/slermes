/*
 * checkpoint_store.h - checkpoint store paths and git invocation.
 *
 * Module-internal header (checkpoint_manager.c <-> checkpoint_store.c).
 */
#ifndef CHECKPOINT_STORE_H
#define CHECKPOINT_STORE_H

#include <stddef.h>
#include <stdbool.h>
#include "checkpoint_manager.h"

#define CHECKPOINT_DIR ".hermes_checkpoints"

void _index_path(const char *base, const char *dir_hash, char *out_path, size_t out_len);
void _project_hash(const char *working_dir, char *out_hash, size_t out_len);
void _project_meta_path(const char *base, const char *dir_hash, char *out_path, size_t out_len);
void _ref_name(const char *dir_hash, char *out_ref, size_t out_len);
bool _run_git(const char *repo_path, const char *cmd, char *out, size_t out_len);
bool _run_git_idx(const char *shadow_repo, const char *working_dir, const char *index_file, const char *cmd, char *out, size_t out_len);
void _shadow_repo_path(const char *working_dir, char *out_path, size_t out_len);
void _store_path(const char *base, char *out_path, size_t out_len);

#endif /* CHECKPOINT_STORE_H */
