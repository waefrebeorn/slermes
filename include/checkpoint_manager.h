/*
 * checkpoint_manager.h — Git-based filesystem checkpoint/snapshot manager.
 * Port of Python tools/checkpoint_manager.py (CheckpointManager, v2).
 *
 * Opaque struct + minimal includes. The C port already provides the
 * git-shadow helper layer (src/tools/checkpoint_manager.c); this header
 * exposes the manager object and its methods.
 */
#ifndef CHECKPOINT_MANAGER_H
#define CHECKPOINT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bounded cache of dirs checkpointed this turn (Python's
 * self._checkpointed_dirs set). */
#define CM_MAX_CHECKPOINTED_DIRS 256
#define CM_MAX_PATH 4096

/* Opaque manager state. Mirrors the Python CheckpointManager:
 *   enabled, max_snapshots, max_total_size_mb, max_file_size_mb,
 *   lazy _git_available, and a per-turn dedup set. */
typedef struct checkpoint_manager tool_checkpoint_mgr_t;

/* Create a manager. enabled is the master switch (config / CLI flag).
 * Any NULL pointer returns NULL. */
tool_checkpoint_mgr_t *checkpoint_manager_create(bool enabled,
                                              int max_snapshots,
                                              int max_total_size_mb,
                                              int max_file_size_mb);

/* Free the manager. */
void checkpoint_manager_free(tool_checkpoint_mgr_t *self);

/* Reset per-turn dedup. Call at the start of each agent iteration. */
void checkpoint_manager_new_turn(tool_checkpoint_mgr_t *self);

/* Take a checkpoint if enabled and not already done this turn.
 * Returns true if a snapshot was taken. Never raises. */
bool checkpoint_manager_ensure(tool_checkpoint_mgr_t *self,
                             const char *working_dir,
                             const char *reason);

/* List available checkpoints for a directory (most recent first).
 * Returns a malloc'd JSON array string (caller frees) or NULL. */
char *checkpoint_manager_list(tool_checkpoint_mgr_t *self,
                            const char *working_dir);

/* Show diff between a checkpoint and the current working tree.
 * commit_hash must be a valid 7-40 hex sha. Returns a malloc'd
 * JSON object string (caller frees) or NULL. */
char *checkpoint_manager_diff(tool_checkpoint_mgr_t *self,
                            const char *working_dir,
                            const char *commit_hash);

/* Restore files to a checkpoint state. file_path may be NULL (restore all).
 * Returns a malloc'd JSON object string (caller frees) or NULL. */
char *checkpoint_manager_restore(tool_checkpoint_mgr_t *self,
                               const char *working_dir,
                               const char *commit_hash,
                               const char *file_path);

/* Resolve a file path to its working directory for checkpointing
 * (walks up to the nearest project marker). Returns malloc'd string
 * (caller frees) or NULL. */
char *checkpoint_manager_working_dir_for_path(tool_checkpoint_mgr_t *self,
                                           const char *file_path);

/* ---- Module-level helpers (also ported, used by CLI/web_server) ---- */

/* Validate a commit hash (7-40 hex chars). */
bool checkpoint_validate_commit_hash(const char *commit_hash);

/* Validate a file path (non-empty, no ".." traversal). */
bool checkpoint_validate_file_path(const char *file_path,
                                 const char *working_dir);

/* Normalize a path via realpath (falls back to the input). Caller frees. */
char *checkpoint_normalize_path(const char *path_value);

/* Take a snapshot directly (no enabled/dedup guards). Returns true on success. */
bool checkpoint_manager_take(tool_checkpoint_mgr_t *self,
                           const char *working_dir,
                           const char *reason);

/* Prune old checkpoints (keep N per project). Module-level entry point. */
bool checkpoint_prune(const char *store, const char *working_dir, int keep);

/* Enforce the global size cap across all projects. */
void checkpoint_maybe_auto_prune(const char *store, const char *working_dir);

/* Status JSON for the store. Caller frees. */
char *checkpoint_store_status(const char *checkpoint_base);

/* Clear all / clear legacy checkpoints. Caller frees result JSON. */
char *checkpoint_clear_all(const char *checkpoint_base);
char *checkpoint_clear_legacy(const char *checkpoint_base);

/* Format a checkpoint list JSON for display. Caller frees. */
char *checkpoint_format_list(const char *checkpoints_json,
                            const char *directory);

#ifdef __cplusplus
}
#endif

#endif /* CHECKPOINT_MANAGER_H */
