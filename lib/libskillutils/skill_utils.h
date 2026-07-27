/*
 * skill_utils.h — Lightweight skill metadata utilities for C Hermes.
 *
 * Port of Python agent/skill_utils.py (566 lines).
 * Shared by system_prompt.c, skill_preprocessing.c, and plugin_skills.c.
 *
 * MIT License — WuBu Hermes Project
 */

#ifndef SKILL_UTILS_H
#define SKILL_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants ────────────────────────────────────────────── */

/** Maximum skill name length */
#define SKILL_UTILS_NAME_MAX 128

/** Maximum frontmatter value length */
#define SKILL_UTILS_VAL_MAX  512

/** Maximum config var entries per skill */
#define SKILL_UTILS_CFG_MAX  32

/** Storage prefix for skill config vars: skills.config.<key> */
#define SKILL_UTILS_CFG_PREFIX "skills.config"

/* ── Excluded directory check ────────────────────────────── */

/** Return true if any path component matches EXCLUDED_SKILL_DIRS. */
bool is_excluded_skill_path(const char *path);

/* ── Frontmatter parsing ─────────────────────────────────── */

/** A simple key-value frontmatter entry. */
typedef struct {
    char key[SKILL_UTILS_NAME_MAX];
    char value[SKILL_UTILS_VAL_MAX];
} skill_frontmatter_entry_t;

/** Parsed frontmatter result. */
typedef struct {
    skill_frontmatter_entry_t entries[64];
    int count;
} skill_frontmatter_t;

/**
 * Parse YAML frontmatter from a markdown string.
 * Returns number of key:value pairs found (-1 on error).
 * Supports: key, description, category, metadata.hermes.{config,conditions,etc}
 * Uses dotted-key flattening for nested YAML.
 */
int parse_frontmatter(const char *content, skill_frontmatter_t *fm);

/** Get a frontmatter value by key. Returns NULL if not found. */
const char *skill_fm_get(const skill_frontmatter_t *fm, const char *key);

/** Get a bool frontmatter value. */
bool skill_fm_get_bool(const skill_frontmatter_t *fm, const char *key, bool def);

/* ── Platform matching ───────────────────────────────────── */

/**
 * Return true when the skill is compatible with the current OS.
 * Checks frontmatter["platforms"] list. If absent or empty, compatible
 * with all platforms (backward-compatible default).
 */
bool skill_matches_platform(const skill_frontmatter_t *fm);

/* ── Disabled skills ─────────────────────────────────────── */

/**
 * Read disabled skill names from config.yaml.
 * Checks skills.disabled and skills.platform_disabled.<platform>.
 * Returns comma-separated string (caller must free) or NULL.
 * Returns only unique, non-empty entries.
 */
char *skill_get_disabled_names(const char *config_path, const char *platform);

/* ── External skills directories ──────────────────────────── */

/**
 * Read skills.external_dirs from config.yaml and return validated paths.
 * Each entry is expanded (~ and ${VAR} → actual path).
 * Only existing directories returned; duplicates and local skills dir skipped.
 * Returns malloc'd array of path strings; *count set. Caller frees each
 * path and the array. Pass hermes_home for relative path resolution.
 */
char **skill_get_external_dirs(const char *config_path,
                               const char *hermes_home,
                               const char *local_skills_dir,
                               size_t *count);

/* ── All skills directories ──────────────────────────────── */

/** Build an ordered list: [local_skills] + [external_dirs] (null-terminated). */
char **skill_get_all_dirs(const char *config_path,
                          const char *hermes_home,
                          const char *local_skills_dir,
                          size_t *count);

/* ── Condition extraction ────────────────────────────────── */

/** Conditional activation fields from frontmatter metadata.hermes. */
typedef struct {
    char fallback_for_toolsets[4096]; /** comma-sep list */
    char requires_toolsets[4096];
    char fallback_for_tools[4096];
    char requires_tools[4096];
} skill_conditions_t;

/**
 * Extract conditional activation fields from parsed frontmatter's
 * metadata.hermes section.
 */
void extract_skill_conditions(const skill_frontmatter_t *fm,
                              skill_conditions_t *conds);

/* ── Config variable extraction ──────────────────────────── */

/** A single config variable declaration. */
typedef struct {
    char key[SKILL_UTILS_NAME_MAX];
    char description[SKILL_UTILS_VAL_MAX];
    char default_val[SKILL_UTILS_VAL_MAX];
    char prompt[SKILL_UTILS_VAL_MAX];
} skill_config_var_t;

/**
 * Extract config variable declarations from frontmatter's
 * metadata.hermes.config section. Returns count (0 = none).
 */
int extract_skill_config_vars(const skill_frontmatter_t *fm,
                              skill_config_var_t *vars, int max_vars);

/**
 * Resolve current values for skill config vars from config.yaml under the
 * "skills.config.<key>" dotpath. Falls back to each var's declared default
 * when unset/blank; expands ~ and ${VAR} in path-like values. Returns a
 * malloc'd JSON object string {logical_key: value, ...} ("{}" when no vars).
 * Caller frees.
 */
char *resolve_skill_config_values(const char *config_path,
                                  const skill_config_var_t *vars, int n_vars);

/* ── Description extraction ──────────────────────────────── */

/** Extract truncated description (max 60 chars) from frontmatter. */
void extract_skill_description(const skill_frontmatter_t *fm,
                               char *out, size_t out_size);

/* ── File iteration ──────────────────────────────────────── */

/** Callback for iter_skill_index_files. Return non-zero to stop iteration. */
typedef int (*skill_iter_cb)(const char *full_path, void *user);

/**
 * Walk skills_dir yielding sorted paths matching filename.
 * Excludes EXCLUDED_SKILL_DIRS. Follows symlinks.
 * Returns number of files yielded.
 */
int iter_skill_index_files(const char *skills_dir, const char *filename,
                           skill_iter_cb callback, void *user);

/* ── Namespace helpers ───────────────────────────────────── */

/**
 * Split 'namespace:skill-name' into (namespace, bare_name).
 * Returns namespace string (malloc'd) and bare_name (malloc'd).
 * Sets *namespace to NULL when there is no ':'.
 * Caller must free both.
 */
void parse_qualified_name(const char *name,
                                char **namespace_out,
                                char **bare_name_out);

/** Check whether candidate is a valid namespace [a-zA-Z0-9_-]+. */
bool is_valid_namespace(const char *candidate);

/* ──── New thin wrappers (port of Python) ──────────────── */

/** Deduplicate comma/space-separated string set. Port of Python _normalize_string_set. */
char *skill_normalize_string_set(const char *input);

/** Resolve dot-separated path in frontmatter. Port of Python _resolve_dotpath. */
const char *skill_resolve_dotpath(const skill_frontmatter_t *fm,
                                  const char *dotpath);

/** Clear external dirs cache (no-op in C). Port of Python _external_dirs_cache_clear. */
void skill_external_dirs_cache_clear(void);

/** Parse YAML string. Port of Python yaml_load. */
struct yaml_doc;
typedef struct yaml_doc yaml_doc_t;

yaml_doc_t *skill_yaml_load(const char *yaml_str);

/** Detect if a named runtime environment is active. Port of Python _detect_environment. */
bool skill_detect_environment(const char *env_name);

/** Check frontmatter "environments" against current runtime env. Port of Python skill_matches_environment. */
bool skill_matches_environment(const skill_frontmatter_t *fm);

#ifdef __cplusplus
}
#endif

#endif /* SKILL_UTILS_H */
