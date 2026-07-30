#ifndef HERMES_FILE_SAFETY_H
#define HERMES_FILE_SAFETY_H

/*
 * file_safety.h — File path safety checks for Hermes C.
 * Mirrors Python agent/file_safety.py: denied write paths,
 * safe write root, read-block for credential files.
 *
 * Security sector (P1): prevents prompt-injected file writes
 * from overwriting SSH keys, shell config, .env, etc.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Return type for sandbox/container mirror classification.
 * Port of Python dict with target_path, mirror_root, inner_path keys. */
typedef struct {
    char *target_path;  /* The resolved path that was checked */
    char *mirror_root;  /* The sandbox-mirror root prefix */
    char *inner_path;   /* Portion under mirror root */
} sandbox_mirror_info_t;

/**
 * Check if a path is blocked by the write denylist.
 *
 * Checks against exact denied paths (SSH keys, .env, shell configs,
 * /etc/sudoers, /etc/passwd) and denied directory prefixes (.ssh/,
 * .aws/, .gnupg/, /etc/sudoers.d/, etc.), plus Hermes control files
 * (auth.json, config.yaml, webhook_subscriptions.json, mcp-tokens/).
 *
 * If HERMES_WRITE_SAFE_ROOT is set, all paths outside that root are
 * also denied.
 *
 * @param path  Absolute or relative path to check (will be resolved).
 * @return true if the path must not be written.
 */
bool is_write_denied(const char *path);

/**
 * Return an error message string when a read targets a denied Hermes path.
 *
 * Blocks reads of internal cache files (skills/.hub/), credential stores
 * (auth.json, .env, .anthropic_oauth.json, webhook_subscriptions.json),
 * and mcp-tokens/.
 *
 * @param path  Absolute or relative path to check.
 * @return Newly allocated error string if blocked, NULL if allowed.
 *         Caller must free().
 */
char *get_read_block_error(const char *path);

/* Port of Python: agent/file_safety.py:get_write_denied_error(). */
char *get_write_denied_error(const char *path, const char *verb);

/* Port of Python: agent/file_safety.py:raise_if_read_blocked(). */
void raise_if_read_blocked(const char *path);

void file_safety_set_test_paths(const char *hermes_home, const char *hermes_root);

/**
 * Check if a path is a cross-profile write target.
 * Detects writes to another profile's scoped areas (skills/plugins/cron/memories).
 *
 * @param path        Absolute or relative path to check.
 * @param warning_out Buffer for warning message (can be NULL).
 * @param warning_sz  Size of warning_out buffer.
 * @return true if path is a cross-profile target, false otherwise.
 */
bool file_is_cross_profile_target(const char *path, char *warning_out, size_t warning_sz);

/* Port of Python: classify_sandbox_mirror_target.
 * Detect sandbox-mirror path pattern "sandboxes/<backend>/<task>/home/.hermes/...".
 * Returns info struct or NULL. Caller must sandbox_mirror_info_free(). */
sandbox_mirror_info_t *classify_sandbox_mirror_target(const char *path);

/* Port of Python: get_sandbox_mirror_warning.
 * Return warning string for sandbox-mirror paths, or NULL if not a mirror. */
char *get_sandbox_mirror_warning(const char *path);

/* Port of Python: classify_container_mirror_target.
 * Detect if path is under a container-side sandbox mirror prefix. */
sandbox_mirror_info_t *classify_container_mirror_target(const char *path, const char *mirror_prefix);

/* Port of Python: get_container_mirror_warning.
 * Return warning string for container-side sandbox mirrors. */
char *get_container_mirror_warning(const char *path, const char *mirror_prefix);

/* Free a sandbox_mirror_info_t struct. */
void sandbox_mirror_info_free(sandbox_mirror_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_FILE_SAFETY_H */
