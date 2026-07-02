/*
 * coding_context.h — Coding-context awareness API.
 */

#ifndef CODING_CONTEXT_H
#define CODING_CONTEXT_H

#include "hermes.h" /* for hermes_config_t */

/* Opaque forward declaration */
typedef struct coding_runtime_mode_s coding_runtime_mode_t;

/* Profile struct (mirrors Python ContextProfile dataclass) */
typedef struct {
    const char *name;
    const char *toolset;
    const char *guidance;
    const char *model_hint;
    const char *memory_policy;
    const char **compact_skill_cats;
} coding_context_profile_t;

/* Profile lookup */
const coding_context_profile_t *coding_context_get_profile(const char *name);

/* Config mode resolution */
const char *coding_context_resolve_mode(const hermes_config_t *config);

/* CWD resolution */
void coding_context_resolve_cwd(const hermes_config_t *config, char *out, size_t out_size);

/* Git root detection */
bool coding_context_find_git_root(const char *cwd, char *out, size_t out_size);

/* Home directory */
bool coding_context_get_home(char *out, size_t out_size);

/* Project marker root */
bool coding_context_find_marker_root(const char *cwd, char *out, size_t out_size);

/* Profile name detection */
const char *coding_context_detect_profile_name(
    const char *mode, const char *platform, const char *cwd);

/* Model family classification */
const char *coding_context_model_family(const char *model);

/* Edit format guidance line */
const char *coding_context_edit_format_line(const char *model);

/* RuntimeMode lifecycle */
coding_runtime_mode_t *coding_runtime_mode_create(
    const char *platform,
    const char *cwd,
    const hermes_config_t *config,
    const char *model);

void coding_runtime_mode_destroy(coding_runtime_mode_t *mode);

/* RuntimeMode properties */
bool coding_runtime_mode_is_coding(const coding_runtime_mode_t *mode);
const coding_context_profile_t *coding_runtime_mode_profile(const coding_runtime_mode_t *mode);
const char *coding_runtime_mode_kind(const coding_runtime_mode_t *mode);
const char *coding_runtime_mode_surface(const coding_runtime_mode_t *mode);
const char *coding_runtime_mode_cwd(const coding_runtime_mode_t *mode);
const char *coding_runtime_mode_config_mode(const coding_runtime_mode_t *mode);
const char *coding_runtime_mode_model(const coding_runtime_mode_t *mode);

/* Toolset selection */
const char **coding_runtime_mode_toolset_selection(
    const coding_runtime_mode_t *mode, const hermes_config_t *config);

/* System prompt blocks */
char **coding_runtime_mode_system_blocks(
    const coding_runtime_mode_t *mode, int *out_count);

/* Free system blocks array */
void coding_runtime_mode_free_blocks(char **blocks, int count);

/* Compact skill categories */
const char **coding_runtime_mode_compact_skill_categories(const coding_runtime_mode_t *mode);

/* Main entry point */
coding_runtime_mode_t *coding_context_resolve_runtime_mode(
    const char *platform,
    const char *cwd,
    const hermes_config_t *config,
    const char *model);

#endif /* CODING_CONTEXT_H */
