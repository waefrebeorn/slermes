/*
 * skill_manager_val.h — Skill Manager validation/security core (faithful C11
 * port of tools/skill_manager_tool.py validation + delete-guard logic).
 *
 * Pure, self-contained validators: skill name/category, frontmatter, content
 * size, file-path allow-listing, path-redirect (symlink) detection, and the
 * defense-in-depth delete guard. No YAML/DB/skills-root globbing — callers
 * supply the known skills roots explicitly so the core stays testable.
 *
 * These mirror the agent-facing skill_manage tool's safety invariants, which
 * protect against the class of bug where a poisoned skills tree or bad
 * discovery hands a recursive delete a path outside the skills roots.
 */

#ifndef SKILL_MANAGER_VAL_H
#define SKILL_MANAGER_VAL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SKILL_VAL_MAX_NAME_LENGTH 64
#define SKILL_VAL_MAX_DESCRIPTION_LENGTH 1024
#define SKILL_VAL_MAX_SKILL_CONTENT_CHARS 100000
/* Allowed supporting-file subdirectories under a skill dir. */
#define SKILL_VAL_NUM_ALLOWED_SUBDIRS 4
extern const char *SKILL_VAL_ALLOWED_SUBDIRS[SKILL_VAL_NUM_ALLOWED_SUBDIRS];

/* Returns an error string (caller-owned) on failure, or NULL when valid.
 * Caller frees the returned string with free(). */
char *skill_val_validate_name(const char *name);
char *skill_val_validate_category(const char *category);  /* NULL category => NULL (ok) */
char *skill_val_validate_content_size(const char *content, const char *label);
char *skill_val_validate_frontmatter(const char *content);

/* True when path is a symlink (redirect that rmtree would follow). */
bool skill_val_is_path_redirect(const char *path);

/* Validate a supporting-file path for write_file/remove_file. Returns an error
 * string or NULL. Accepts SKILL.md (root or <name>/SKILL.md); otherwise the
 * first path component must be an allowed subdirectory and a filename present.
 * Rejects path traversal ('..'). */
char *skill_val_validate_file_path(const char *file_path);

/* Resolve a supporting-file path within skill_dir; returns the joined path
 * (caller-owned) and *out_error NULL when safe, else NULL + error string. */
char *skill_val_resolve_skill_target(const char *skill_dir, const char *file_path,
                                     char **out_error);

/* Delete guard: refuse if `skill_dir` is a redirect, equals a known root, or
 * does not resolve strictly inside one of `roots` (count = nroots). Returns an
 * error string or NULL. `roots` are absolute directory paths. */
char *skill_val_validate_delete_target(const char *skill_dir,
                                       char **roots, int nroots);

/* Return the first root in `roots` that contains `skill_path` (strictly
 * inside), or NULL. Caller-owned copy. */
char *skill_val_containing_skills_root(const char *skill_path,
                                       char **roots, int nroots);

#ifdef __cplusplus
}
#endif

#endif /* SKILL_MANAGER_VAL_H */
