#ifndef SRC_TOOLS_SKILLS_SYNC_FS_H
#define SRC_TOOLS_SKILLS_SYNC_FS_H

/*
 * skills_sync_fs.h — focused extraction from tools/skills_sync.py
 *
 * Pure filesystem helpers: MD5 hash of a directory's contents (change
 * detection), and a path-traversal-safe relative-install sanitizer.
 * No god header, no void* passthrough. Reused by port_skills_sync.c
 * (restore_official_optional_skill / backfill_optional_provenance) so the
 * MD5 + sanitizer logic lives in ONE place (no double-coding).
 */

#ifdef __cplusplus
extern "C" {
#endif

/* MD5 hash of all regular-file contents under `directory`, keyed by
 * sorted relative path. Returns malloc'd hex string (caller frees) or NULL.
 * Mirrors Python tools/skills_sync.py:_dir_hash (hashlib.md5 over
 * sorted(relative_path bytes + file bytes)). */
char *skills_sync_fs_dir_hash(const char *directory);

/* Normalize `path` relative to `base`; reject absolute paths and any
 * `..` segment (traversal). Returns malloc'd POSIX join (caller frees)
 * or NULL when unsafe. Mirrors tools/skills_sync.py:_safe_rel_install_path. */
char *skills_sync_fs_safe_rel_install_path(const char *path, const char *base);

/* Relative destination of `skill_dir` under `bundled_dir`, mapped into
 * $HERMES_HOME/skills/<rel>. Returns malloc'd string or NULL.
 * Mirrors tools/skills_sync.py:_compute_relative_dest. */
char *skills_sync_fs_compute_relative_dest(const char *skill_dir,
                                                 const char *bundled_dir);

#ifdef __cplusplus
}
#endif

#endif /* SRC_TOOLS_SKILLS_SYNC_FS_H */
