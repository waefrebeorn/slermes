#ifndef HERMES_SYSTEM_PROMPT_H
#define HERMES_SYSTEM_PROMPT_H

/*
 * hermes_system_prompt.h — Dynamic system prompt assembly for Hermes C.
 * Port of Python agent/system_prompt.py + prompt_builder.py constants.
 *
 * Builds a three-tier system prompt:
 *   stable   — identity, tool guidance, skills prompt, env/platform hints
 *   context  — context files, caller-supplied system_message
 *   volatile — memory snapshot, user profile, timestamp
 *
 * Returns a malloc'd string (caller must free) or NULL on error.
 *
 * Context file loading (SOUL.md, .hermes.md, AGENTS.md, CLAUDE.md, .cursorrules)
 * with prompt-injection threat detection. Port of Python prompt_builder.py.
 */

#include "hermes.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  Guidance constants (ported from Python prompt_builder.py)
 * ================================================================ */

/* Default AIAgent identity when no SOUL.md is present */
extern const char *SYSPRMPT_DEFAULT_IDENTITY;

/* Pointer to hermes-agent docs for user questions about Hermes itself */
extern const char *SYSPRMPT_HERMES_HELP;

/* Memory tool usage guidance */
extern const char *SYSPRMPT_MEMORY_GUIDANCE;

/* Session search tool guidance */
extern const char *SYSPRMPT_SESSION_SEARCH_GUIDANCE;

/* Skills tool usage guidance */
extern const char *SYSPRMPT_SKILLS_GUIDANCE;

/* Kanban task execution protocol */
extern const char *SYSPRMPT_KANBAN_GUIDANCE;

/* Tool-use enforcement (tell model to actually call tools) */
extern const char *SYSPRMPT_TOOL_ENFORCE;

/* OpenAI/Grok execution discipline */
extern const char *SYSPRMPT_OPENAI_EXEC;

/* Google model operational directives */
extern const char *SYSPRMPT_GOOGLE_OPS;

/* Computer-use guidance */
extern const char *SYSPRMPT_COMPUTER_USE;

/* Task-completion "finish the job" guidance (all models) */
extern const char *SYSPRMPT_TASK_COMPLETION;

/* ================================================================
 *  Context file threat detection constants
 * ================================================================ */

#define CONTEXT_FILE_MAX_CHARS    20000
#define CONTEXT_TRUNCATE_HEAD_RATIO 0.7
#define CONTEXT_TRUNCATE_TAIL_RATIO 0.2

/* Threat pattern match result */
typedef struct {
    const char *id;   /* pattern identifier */
} context_threat_match_t;

/* ================================================================
 *  Context file loading API
 * ================================================================ */

/* Scan content for prompt-injection patterns. Returns sanitized content
 * (malloc'd), or NULL if content is clean. Caller must free result.
 * When threats are found, returns a [BLOCKED: ...] message. */
char *context_scan_content(const char *content, const char *filename);

/* Walk up from start_dir looking for a .git directory. Returns malloc'd
 * path to the git root, or NULL. Caller must free. */
char *context_find_git_root(const char *start_dir);

/* Load SOUL.md from HERMES_HOME (~/.hermes/). Returns malloc'd content
 * (with threat scan applied) or NULL. Caller must free. */
char *context_load_soul_md(void);

/* Load .hermes.md / HERMES.md by walking to git root from cwd.
 * Returns malloc'd content or NULL. Caller must free. */
char *context_load_hermes_md(const char *cwd);

/* Load AGENTS.md / agents.md from cwd. Returns malloc'd or NULL. */
char *context_load_agents_md(const char *cwd);

/* Load CLAUDE.md / claude.md from cwd. Returns malloc'd or NULL. */
char *context_load_claude_md(const char *cwd);

/* Load .cursorrules + .cursor/rules *.mdc from cwd. Returns malloc'd or NULL. */
char *context_load_cursorrules(const char *cwd);

/* Build full context files prompt block (SOUL.md + highest-priority project
 * context). Returns malloc'd string or empty string (if no context found).
 * If skip_soul is true, SOUL.md is not included (loaded separately). */
char *build_context_files_prompt(const char *cwd, bool skip_soul);

/* ================================================================
 *  Platform hints (PLATFORM_HINTS dict from prompt_builder.py)
 * ================================================================ */

/* Get platform-specific hint string for system prompt. Returns a static
 * string or NULL if platform is unknown. */
const char *platform_hint_get(const char *platform_name);

/* ================================================================
 *  Environment hints (build_environment_hints from prompt_builder.py)
 * ================================================================ */

/* Build environment-specific guidance string (WSL, host info, remote
 * backends). Returns malloc'd string or NULL. Caller must free. */
char *build_environment_hints(void);

/* ================================================================
 *  System prompt assembly
 * ================================================================ */

/* Configuration for building a system prompt */
typedef struct {
    /* Stable tier flags */
    bool     use_soul;          /* true = try SOUL.md, false = use DEFAULT_IDENTITY */
    bool     has_memory;        /* inject MEMORY_GUIDANCE if memory tool available */
    bool     has_session_search;/* inject SESSION_SEARCH_GUIDANCE */
    bool     has_skills;        /* inject SKILLS_GUIDANCE */
    bool     has_kanban;        /* inject KANBAN_GUIDANCE */
    bool     has_computer_use;  /* inject COMPUTER_USE_GUIDANCE */
    bool     enforce_tools;     /* inject TOOL_ENFORCE guidance */
    bool     is_openai_family;  /* inject OPENAI_EXEC guidance */
    bool     is_google_family;  /* inject GOOGLE_OPS guidance */
    bool     is_alibaba;        /* inject Alibaba model-name workaround */
    bool     use_task_completion; /* inject TASK_COMPLETION guidance */
    bool     registry_has_tools;  /* true if tool registry has entries (enables steer note) */
    const char *alibaba_model;  /* actual model name for Alibaba workaround */
    const char *env_hints;      /* WSL, Termux, etc. environment hints */
    const char *platform_hint;  /* Platform-specific hint (telegram, etc.) */

    /* Context tier */
    const char *system_message; /* caller-supplied system_message */
    const char *context_files;  /* AGENTS.md / .cursorrules content */

    /* Volatile tier */
    const char *memory_snapshot;   /* memory store formatted for prompt */
    const char *user_profile;      /* USER.md profile content */
    const char *ext_memory_block;  /* external memory provider block */
    const char *model_name;        /* model name for timestamp */
    const char *provider_name;     /* provider name for timestamp */
    const char *session_id;        /* session ID (optional) */
    bool         pass_session_id;  /* whether to include session_id */
    void (*memory_reload_fn)(void); /* optional: reload memory from disk on invalidation */
} system_prompt_config_t;

/* Build a system prompt string. Returns malloc'd string, caller must free. */
char *system_prompt_build(const system_prompt_config_t *cfg);

/* Build the stable tier only (for caching) */
char *system_prompt_build_stable(const system_prompt_config_t *cfg);

/* Build the volatile tier only (for per-turn updates) */
char *system_prompt_build_volatile(const system_prompt_config_t *cfg);

/* Invalidate cached system prompt, forcing rebuild on next call.
 * Port of Python system_prompt.py::invalidate_system_prompt().
 * When cfg is provided with memory_reload_fn, reloads memory from disk
 * matching Python's behavior. Otherwise a no-op. */
void invalidate_system_prompt(void);
void invalidate_system_prompt_with_cfg(const system_prompt_config_t *cfg);

/* Build environment hints string: Host, Home, cwd, OS, WSL hint.
 * Returns malloc'd string (caller must free) or NULL on failure. */
char *build_environment_hints(void);

/* Probe remote terminal backend for environment hints.
 * Returns malloc'd hint string (caller must free) or NULL. */
char *probe_remote_backend(const char *env_type);

/* Check if model should use 'developer' role instead of 'system' at API boundary.
 * Port of Python prompt_builder.py:DEVELOPER_ROLE_MODELS. */
bool is_developer_role_model(const char *model_name);

/* ================================================================
 *  Utility: truncate content with head/tail marker
 * ================================================================ */

/* Truncate content with head/tail marker. Returns malloc'd string or NULL.
 * Caller must free. Always returns a copy (even if no truncation). */
char *context_truncate_content(const char *content, const char *name, int max_chars);

/* Strip YAML frontmatter from content. Returns malloc'd string (caller
 * must free) or NULL on error. */
char *context_strip_frontmatter(const char *content);

/* ================================================================
 *  Continuation prompt (port of conversation_loop._get_continuation_prompt)
 * ================================================================ */

/* Build a continuation prompt when the previous response was truncated.
 * is_partial_stub: true if a network/stream error cut the response.
 * dropped_tools_json: optional JSON array of tool names that were dropped
 *   due to size limits (pass NULL if none).
 * Returns a malloc'd string (caller must free) or NULL on error.
 * Port of Python conversation_loop._get_continuation_prompt(). */
char *agent_get_continuation_prompt(bool is_partial_stub,
                                     const char *dropped_tools_json);

/* ================================================================
 *  Skills prompt snapshot caching (AG03)
 * ================================================================ */

/* Clear the in-process skills prompt cache (and optionally disk snapshot).
 * hermes_home: path to HERMES_HOME directory.
 * clear_snapshot: if true, also delete the on-disk snapshot file. */
void clear_skills_system_prompt_cache(const char *hermes_home, bool clear_snapshot);

/* Build an mtime/size manifest of SKILL.md and DESCRIPTION.md files.
 * Returns JSON object: { "relative/path": [mtime, size], ... } */
json_node_t *build_skills_manifest(const char *skills_dir);

/* Load the disk snapshot if it exists and its manifest still matches.
 * Returns JSON snapshot object or NULL. Caller must json_free(). */
json_node_t *load_skills_snapshot(const char *skills_dir, const char *hermes_home);

/* Write skill metadata to disk for fast cold-start reuse. */
void write_skills_snapshot(const char *skills_dir, const char *hermes_home,
                            json_node_t *manifest, json_node_t *skill_entries,
                            json_node_t *category_descriptions);

/* Build a serializable metadata dict for one skill.
 * Returns JSON object with skill_name, category, frontmatter_name,
 * description, platforms, conditions. Caller must json_free(). */
json_node_t *build_snapshot_entry(const char *skill_file, const char *skills_dir,
                                   json_node_t *frontmatter, const char *description);

/* ================================================================
 *  Runtime CWD resolution (ported from agent/runtime_cwd.py)
 * ================================================================ */

/* Pin the logical cwd for the current context (gateway per-session). */
void set_session_cwd(const char *cwd);

/* Clear the session cwd override. */
void clear_session_cwd(void);

/* Resolve the agent working directory.
 * Priority: session override > TERMINAL_CWD env > PWD env > getcwd().
 * Returns a pointer to a static buffer (valid until next call). */
const char *resolve_agent_cwd(void);

/* Resolve context cwd — returns NULL if no configured cwd.
 * Unlike resolve_agent_cwd(), does NOT fall back to getcwd(). */
const char *resolve_context_cwd(void);

/* Build a compact skill index for the system prompt.
 * Scans the skills directory, groups by category, and formats as the
 * <available_skills> block. Returns malloc'd string or empty string.
 * Caller must free.
 * Port of Python prompt_builder.build_skills_system_prompt(). */
char *build_skills_system_prompt(const char *disabled_csv);

/* Build a Nous subscription capability block for the system prompt.
 * Returns empty string in C (Nous subscription system is Nous-specific).
 * Caller must free.
 * Port of Python prompt_builder.build_nous_subscription_prompt(). */
char *build_nous_subscription_prompt(void);

/* Format tool definitions as a JSON array for trajectory/system message use.
 * Returns malloc'd string (caller must free) or NULL on error.
 * Mirrors Python agent/system_prompt.py:format_tools_for_system_message(). */
char *format_tools_for_system_message(tool_registry_t *reg);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_SYSTEM_PROMPT_H */
