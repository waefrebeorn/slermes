/**
 * @defgroup hermes Main API
 * @brief Master header for WuBu Slermes C implementation.
 *
 * Defines core types: provider_config_t, agent_state_t, message_t,
 * tool_call_t, llm_config_t, llm_response_t, gateway_config_t,
 * plugin_config_t. Includes all subsystem headers.
 *
 * @{
 */
#ifndef HERMES_H
#define HERMES_H

/*
 * hermes.h — Master header for WuBu Slermes C implementation.
 * Defines core types, config, and forward declarations.
 */


/* ================================================================
 *  Core types + constants (extracted to keep this header an
 *  umbrella, not a god-header). Subsystem headers now include
 *  hermes_core_types.h directly instead of dragging in everything.
 * ================================================================ */
#include "hermes_core_types.h"


/* ================================================================
 *  Include sub-headers for dependency wrappers
 * ================================================================ */
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include "hermes_crypto.h"
#include "hermes_db.h"
#include "hermes_display.h"
#include "hermes_skin.h"
#include "hermes_agent.h"
#include "hermes_credits_tracker.h"
#include "hermes_plugin.h"
#include "hermes_memory.h"
#include "hermes_tirith.h"
#include "hermes_media_cache.h"

/* ================================================================
 *  Entry Points
 * ================================================================ */
/* Port of Python: cli_main */
int  cli_main(int argc, char **argv);
int  hermes_gateway_main(int argc, char **argv);

/* Command system */
typedef struct command_def_t {
    const char *name;
    const char *alias;
    const char *description;
    const char *category;   /* Grouping category for help display (e.g. "Session", "Config") */
    const char *args_hint;  /* Usage hint for args (e.g. "[key] [val]" or "<session_id>") */
    void (*handler)(const char *args, agent_state_t *state);
    const char *subcommands;/* Comma-separated subcommand list, e.g. "list,show,set", NULL if none */
    bool cli_only;          /* Only available in CLI mode */
    bool gateway_only;      /* Only available in gateway/messaging mode */
    const char *gateway_config_gate; /* If set, command is available in gateway when this config dotpath is truthy */
} command_def_t;
bool commands_dispatch(const char *input, agent_state_t *state);
bool commands_try_skill(const char *input, agent_state_t *state);
bool commands_try_quick(const char *input, agent_state_t *state);
void commands_set_quick_config(const hermes_config_t *cfg);
const command_def_t *commands_resolve(const char *input);
const command_def_t *commands_get_all(void);
int commands_count(void);
const char *commands_list_json(void);

/* Security approval */
int approval_check(const char *tool_name, const char *args_json);
void approval_reset_session(void);
void approval_set_yolo(bool enabled);  /* When true, skip all approval prompts */
bool approval_is_yolo_enabled(void);   /* PoP: tools/approval.py:is_current_session_yolo_enabled */
void approval_set_allowlist_path(const char *path);
void approval_load_allowlist(void);
void approval_save_allowlist(void);

/* Gateway approval — wire messaging platform send/response for async approval prompts */
void approval_set_gateway_send(bool (*fn)(const char *, const char *, const char *),
                                const char *platform, const char *chat_id);
void approval_set_gateway_wait(char *(*fn)(int timeout_sec));
/* Called from approval.c during gateway path to register pending approval context */
void gw_approval_set_context(const char *platform, const char *chat_id);

/* Gateway clarify — wire messaging platform send/response for async clarify prompts */
void clarify_set_gateway_send(bool (*fn)(const char *, const char *, const char *,
                                         const char **, int, const char *),
                               const char *platform, const char *chat_id);
void clarify_set_gateway_wait(char *(*fn)(int timeout_sec));
void clarify_set_gateway_begin(void (*fn)(const char *, const char *, const char *,
                                          const char *, const char (*)[256], int));
void clarify_set_gateway_context(const char *platform, const char *chat_id,
                                  bool (*send_fn)(const char *, const char *, const char *));

/* P39: Approval cache query */
int approval_cache_count(void);
const char *approval_cache_entry(int index);
void approval_cache_clear_last(int n);

/* Config runtime toggles (wired from config.yaml or /commands) */
void commands_set_verbose(int level);   /* 0=off, 1=normal, 2=verbose */
void commands_set_yolo(bool enabled);
void commands_set_fast(bool enabled);
int  commands_get_verbose(void);
bool commands_get_yolo(void);
bool commands_get_fast(void);

/* CDP browser URL config */
void cdp_set_url(const char *url);
const char *cdp_get_url(void);

/* Registry accessors */
size_t registry_get_count(void);
const char *registry_get_name(size_t i);

/* Generation counter — bumped on every mutation.
 * Callers can cache tool metadata and check generation for staleness. */
uint64_t registry_generation(void);

/* Find tool by name in registry. Returns NULL if not found. */
tool_t *registry_find(const char *name);

/* P52: Per-tool timeout. Set seconds, 0 = default, -1 = no timeout. */
void registry_set_timeout(const char *name, int seconds);
/* P150: Filter tool registry by enabled/disabled toolsets. Marks matching tools unavailable. */
void registry_filter_by_toolset(tool_registry_t *reg, const char *enabled_csv, const char *disabled_csv);
/* P150: Get toolset name for a registered tool. Returns "" if not set. */
const char *registry_get_toolset(const char *name);
/* P150: Set toolset name for a registered tool (after registration). */
void registry_set_toolset(const char *name, const char *toolset);
int  registry_get_timeout(const char *name);

/* P55: Tool wildcard matching — enable/disable all tools matching a pattern */
/* Pattern supports '*' wildcard: "discord:*", "browser_*", "*_search" */
void registry_set_available_pattern(const char *pattern, bool available);

/* S14 gap #9: Toolset availability check — register check function for toolset.
 * Once set, registry_refresh_availability() calls check_fn (with 30s cache) to
 * mark tools in this toolset as available/unavailable. */
void registry_set_toolset_check_fn(const char *toolset, bool (*fn)(void));

/* Refresh availability of all tools that have check_fn registered.
 * Caches results for 30 seconds (check_fn_last). */
void registry_refresh_availability(void);

/* S14 gap #11: Deregister a tool by name. Used for MCP dynamic tool removal.
 * Returns true if tool was found and removed, false if not found. */
bool registry_deregister(const char *name);

/* S14 gap #16: Rich query API — get schema JSON for a tool (returns "" if not found) */
const char *registry_get_schema(const char *name);
/* Rich query: return display emoji for a tool, or default (⚡) if unset */
const char *registry_get_emoji(const char *name, const char *default_emoji);

/* S14 gap #16: Rich query API — check if any tool in a toolset is available */
bool registry_is_toolset_available(const char *toolset);

/* S14 gap #2: Tool Search bridge — search tools by keyword (name/description).
 * Returns JSON array of matching tool names, or ["error":"..."] on failure.
 * Caller must free the returned string. */
char *registry_search(const char *keyword);

/* S14 gap #2: Tool Search bridge — describe a tool.
 * Returns JSON object with name, description, schema, toolset, or {"error":"..."}.
 * Caller must free the returned string. */
char *registry_describe(const char *name);

/* S14 gap #8: Mark a tool as async (handlers that should run in detached thread) */
void registry_set_async(const char *name, bool async);

/* Check if tool name matches a wildcard pattern. Returns true on match. */
bool registry_name_matches(const char *name, const char *pattern);

/* P49-P50: Tool result storage and output limits */
char *tool_result_store(const char *data, size_t size, size_t max_inline);
void tool_result_cleanup(int max_age_seconds);

/* ================================================================
 *  P141-P150: Session DB depth — metadata, search, CRUD, export, branching, migration
 * ================================================================ */

/* P141: Save session metadata (title, model, token count, timestamps) to sidecar file */
bool agent_save_meta(agent_state_t *state);

/* P141: Load session metadata */
bool agent_load_meta(agent_state_t *state, session_meta_t *meta);

/* P143: Session CRUD — create new session with metadata */
bool agent_session_create(agent_state_t *state, const char *session_id);

/* P143: List sessions with metadata filtering. Returns malloc'd array. Caller must free. */
session_entry_t *agent_session_list(size_t *count, const char *tag_filter, int limit);

/* P143: Delete a session */
bool agent_session_delete(agent_state_t *state, const char *session_id);

/* P144: Auto-save current session (checks interval config) */
bool agent_auto_save(agent_state_t *state, int interval);

/* P144: Crash recovery — check for and restore backup */
bool agent_crash_recover(agent_state_t *state);

/* P145: Auto-prune — remove old sessions based on retention_days. Returns number removed. */
int agent_auto_prune(agent_state_t *state, int retention_days);

/* P146: Add tag to session metadata */
bool agent_session_add_tag(agent_state_t *state, const char *tag);

/* P146: Remove tag from session metadata */
bool agent_session_remove_tag(agent_state_t *state, const char *tag);

/* P148: Export session as JSON string. Caller must free. */
char *agent_session_export_json(agent_state_t *state, const char *session_id);

/* P148: Export session as Markdown string. Caller must free. */
char *agent_session_export_markdown(agent_state_t *state, const char *session_id);

/* P149: Branch session — fork at message index N into new session */
bool agent_session_branch(agent_state_t *state, const char *new_id, int branch_point);

/* P150: Migrate all sessions to latest schema. Returns number migrated. */
int agent_session_migrate(agent_state_t *state);

/* ================================================================
 *  P159-P168: Security phase functions
 * ================================================================ */

/* P159: Secrets redaction */
char *hermes_redact(const char *input);
char *hermes_redact_code_file(const char *input); /* Skips key:value patterns for source code */
bool hermes_redact_add_pattern(const char *pattern);
void hermes_redact_clear_patterns(void);
void hermes_redact_load_config(const char *patterns_str);

/* P160: Website blocklist */
void url_blocklist_enable(bool enabled);
bool url_blocklist_add_domain(const char *domain);
bool url_blocklist_remove_domain(const char *domain);
bool url_blocklist_add_category(const char *category);
bool url_blocklist_remove_category(const char *category);
void url_blocklist_clear(void);
void url_blocklist_load_config(const security_config_t *cfg);

/* P161: Command allowlist */
bool allowlist_add(const char *tool, const char *pattern);
bool allowlist_remove(const char *tool, const char *pattern);
void allowlist_clear(void);
bool allowlist_check(const char *tool, const char *command);

/* P162: Approval timeout */
void approval_set_timeout(int seconds);
int approval_get_timeout(void);

/* P163: Tirith security policy (wired via approval/tool dispatch) */

/* P164: Audit log */
bool audit_init(const char *log_dir);
void audit_shutdown(void);
void audit_log_security(const char *category, const char *action,
                         const char *result, const char *reason,
                         const char *detail);
void audit_log_approval(const char *tool, const char *command, bool approved);
void audit_log_redaction(const char *context, const char *pattern_matched);
void audit_log_violation(const char *rule, const char *detail);

/* P165: Rate limiting */
bool rate_limit_init_tool(const char *tool_name, int max_per_minute);
bool rate_limit_init_provider(const char *provider_name, int max_per_minute);
bool rate_limit_check_tool(const char *tool_name);
bool rate_limit_check_provider(const char *provider_name);
int rate_limit_remaining_tool(const char *tool_name);
void rate_limit_reset_all(void);
void rate_limit_clear(void);

/* P166: Output sanitization */
char *hermes_sanitize_output(const char *tool_name, const char *raw_output);

/* B34: Surrogate character sanitization — replace lone UTF-8 surrogates with U+FFFD */
char *sanitize_surrogates(const char *text);
bool  sanitize_json_surrogates(void *json_obj);

/* AG16: Message sanitization gaps — sanitize messages and structures */
bool  sanitize_messages_surrogates(message_t *messages, int count);
bool  sanitize_messages_non_ascii(message_t *messages, int count);
json_node_t *sanitize_tools_non_ascii(json_node_t *tools);
char *escape_invalid_chars_in_json_strings(const char *raw);

/* P167: Credential vault */
bool vault_set_master_key(const char *passphrase);
bool vault_has_master_key(void);
void vault_lock(void);
void vault_set_path(const char *path);
bool vault_save(void);
bool vault_load(void);
bool vault_store(const char *service, const char *key, const char *value);
const char *vault_retrieve(const char *service, const char *key);
bool vault_delete(const char *service, const char *key);
int vault_list_services(char services[][128], int max_count);
bool vault_rotate_key(const char *old_passphrase, const char *new_passphrase);

/* O12: Audit log rotation parameters */
void audit_set_rotation(size_t max_size_kb, int max_files, int max_age_days);

/* P168: File sandbox */
void sandbox_init(void);
void sandbox_enable(bool enabled);
bool sandbox_add_allowed_dir(const char *dir);
bool sandbox_remove_allowed_dir(const char *dir);
void sandbox_clear(void);
bool sandbox_check_path(const char *path);
void sandbox_set_symlink_check(bool enabled);

/* ================================================================
 *  P169-P178: Cron/Scheduler phase functions
 * ================================================================ */

/* P169: SQLite job store */
typedef struct cron_sqlite_store_t cron_sqlite_store_t;
cron_sqlite_store_t *cron_sqlite_open(const char *path);
void cron_sqlite_close(cron_sqlite_store_t *store);
bool cron_sqlite_save_job(cron_sqlite_store_t *store, const char *name,
                           const char *schedule, const char *command,
                           bool active, int retry_count, int max_retries,
                           const char *chain_from, const char *template_name,
                           const char *script_type);
bool cron_sqlite_load_jobs(cron_sqlite_store_t *store);
char *cron_sqlite_list_to_json(cron_sqlite_store_t *store);
/* Inject a human-readable repeat_display field into a cron job JSON object.
 * Mirrors Python cronjob_tools._repeat_display(). */
void cron_inject_repeat_display(json_node_t *job);
bool cron_sqlite_delete_job(cron_sqlite_store_t *store, const char *name);
bool cron_sqlite_update_job(cron_sqlite_store_t *store, const char *name,
                             const char *field, const char *value);

/* P170: Cron expression parser (libcron already exists — see lib/libcron/cron.h) */

/* P171: Job locking */
void  cron_lock_set_dir(const char *dir);
bool  cron_lock_acquire(const char *lock_name);
void  cron_lock_release(const char *lock_name);
bool  cron_lock_is_locked(const char *lock_name);
bool  cron_shutdown_requested(void);
void  cron_release_all_locks(void);

/* P172: Job retry */
bool cron_job_set_retry(const char *job_name, int max_retries, int backoff_sec);
int  cron_job_get_retry_count(const char *job_name);
int  cron_job_get_max_retries(const char *job_name);

/* P173: Job notification */
bool cron_notify_set_channel(const char *channel_id);
void cron_notify_set_send_fn(bool (*fn)(const char *platform, const char *chat_id, const char *text));
bool cron_notify_on_complete(const char *job_name, bool enabled);
bool cron_notify_on_failure(const char *job_name, bool enabled);

/* P174: Job chaining */
bool cron_chain_set_context(const char *job_name, const char *context_from);
const char *cron_chain_get_context(const char *job_name);
char *cron_chain_get_output(const char *job_name);
void cron_chain_store_output(const char *job_name, const char *output);

/* P176: Cron utility functions (port of cronjob_tools.py helpers) */
char **cron_canonical_skills(const char *skill, json_node_t *skills, size_t *out_count);
char  *normalize_optional_job_value(const char *value, bool strip_trailing_slash);
char  *normalize_deliver_param(json_node_t *deliver);
int    cron_parse_duration(const char *s);   /* Parse "30m", "2h", "1d" → minutes */
bool   cron_secure_dir(const char *path);     /* chmod 0700 */
bool   cron_secure_file(const char *path);    /* chmod 0600 */
const char *cron_coerce_job_text(const char *value, const char *fallback); /* nullable string coercion */
const char *cron_schedule_display_for_job(json_node_t *job);              /* Extract display string from job schedule */
bool        cron_ensure_dirs(const char *hermes_home);                    /* mkdir -p ~/.hermes/cron/ + ~/.hermes/cron/output/ */
bool        cron_validate_job_id(const char *job_id, char *out_err);      /* Reject path-escape job IDs */
char       *cron_job_output_dir(const char *hermes_home, const char *job_id, char *out_err); /* Build safe output dir path */
char       *cron_normalize_workdir(const char *workdir, char *out_err);   /* Validate + resolve cron workdir path */
json_node_t *cron_apply_skill_fields(json_node_t *job);                  /* Align skill + skills fields in job JSON */

/* P175: Job templating */
bool cron_template_create(const char *name, const char *schedule,
                           const char *command, const char *params_json);
bool cron_template_instantiate(const char *template_name,
                                const char *params_json,
                                char *out_name, size_t out_name_sz,
                                char *out_schedule, size_t out_sched_sz,
                                char *out_command, size_t out_cmd_sz);

/* P176: Scheduler CLI */
char *cron_cmd_handler(const char *args_json, const char *task_id);

/* P177: Script-based jobs */
bool cron_script_set_interpreter(const char *job_name, const char *interpreter);
char *cron_run_script(const char *script_path, const char *interpreter,
                       const char *args, int *exit_code);

/* P178: Watchdog mode */
bool cron_watchdog_enable(void);
void cron_watchdog_disable(void);
bool cron_watchdog_is_active(void);
void cron_watchdog_ping(void);
int  cron_watchdog_check(time_t timeout_sec);

/* ================================================================
 *  P179-P188: Skills system depth
 * ================================================================ */

/* Skill provenance/origin */
typedef enum {
    SKILL_ORIGIN_LOCAL,
    SKILL_ORIGIN_HUB,
    SKILL_ORIGIN_BUNDLE,
    SKILL_ORIGIN_UNKNOWN,
} skill_origin_t;

/* Skill metadata (full) */
typedef struct {
    char  name[128];              /* skill name (directory name) */
    char  path[HERMES_PATH_MAX];  /* full path to skill directory */
    char  version[32];            /* semver from SKILL.md frontmatter */
    char  author[256];            /* author field */
    char  description[512];       /* description */
    char  category[128];          /* category tag */
    char  tags[1024];             /* comma-separated tags */
    char  dependencies[1024];     /* comma-separated skill deps */
    char  bundles[1024];          /* comma-separated bundle aliases */
    skill_origin_t origin;        /* provenance: local/hub/bundle */
    time_t last_updated;          /* mtime of SKILL.md */
    time_t last_used;             /* last time skill was invoked */
    int   usage_count;            /* how many times skill was used */
    bool  validated;              /* passed frontmatter validation */
    char  validation_error[256];  /* validation error if any */
} skill_meta_t;

/* Cache entry for LRU cache */
typedef struct skill_cache_entry_t {
    char  name[128];
    char *content;                /* full SKILL.md content */
    skill_meta_t meta;
    time_t loaded_at;
    struct skill_cache_entry_t *prev;
    struct skill_cache_entry_t *next;
} skill_cache_entry_t;

/* Skill cache (doubly-linked list for LRU) */
typedef struct {
    skill_cache_entry_t *head;    /* most recently used */
    skill_cache_entry_t *tail;    /* least recently used */
    size_t count;
    size_t max_entries;           /* 0 = unlimited */
} skill_cache_t;

/* Skill scanning result (list of metadata) */
typedef struct {
    skill_meta_t *skills;
    size_t count;
    size_t capacity;
} skill_list_t;

/* Dependency node for resolution */
typedef struct skill_dep_node_t {
    char  name[128];
    char  version[32];
    bool  resolved;
    struct skill_dep_node_t **deps; /* array of dependency names */
    size_t deps_count;
} skill_dep_node_t;

/* Search result */
typedef struct {
    char  name[128];
    char  path[HERMES_PATH_MAX];
    float score;                  /* relevance score 0.0-1.0 */
} skill_search_result_t;

/* P179: Scan skills directory — recursive, extract metadata, cache */
skill_list_t *skills_scan_all(void);
void skills_scan_free(skill_list_t *list);

/* P180: Validate skill frontmatter */
bool skill_validate(const char *skill_name, char *error_out, size_t err_sz);
bool skill_validate_all(void);

/* P181: Get/set skill provenance */
skill_origin_t skill_get_origin(const char *skill_name);
bool skill_set_origin(const char *skill_name, skill_origin_t origin);

/* P182: Sync skills from git-based hub */
bool skill_sync_from_hub(const char *hub_url, const char *branch, char *log_out, size_t log_sz);

/* P183: Bundle management */
bool skill_bundle_create(const char *bundle_name, const char *skills_csv);
bool skill_bundle_delete(const char *bundle_name);
skill_list_t *skill_bundle_get_skills(const char *bundle_name);

/* P184: Usage tracking */
void skill_record_usage(const char *skill_name);
int  skill_get_usage_count(const char *skill_name);
time_t skill_get_last_used(const char *skill_name);
void skill_get_recommendations(skill_meta_t *out, size_t *count, size_t max_count);

/* P185: Cache management */
bool skill_cache_init(size_t max_entries);
void skill_cache_destroy(void);
bool skill_cache_preload(const char *skill_name);
void skill_cache_evict(const char *skill_name);
const char *skill_cache_get(const char *skill_name);
size_t skill_cache_count(void);

/* P186: Search skills */
skill_search_result_t *skill_search(const char *query, const char *tag_filter,
                                     size_t *result_count, size_t max_results);
void skill_search_free(skill_search_result_t *results, size_t count);

/* L12: Browse.sh skills hub — search and install */
skill_search_result_t *skill_search_hub(const char *query,
                                         size_t *result_count, size_t max_results);
void skill_search_hub_free(skill_search_result_t *results, size_t count);
bool skill_install_from_hub(const char *slug, char *error_out, size_t err_sz);

/* P187: Curator — stale detection and auto-update */
bool skill_curator_run(char *report_out, size_t report_sz);
bool skill_curator_set_stale_days(int days);
int  skill_curator_get_stale_days(void);

/* P188: Dependency resolution */
bool skill_deps_resolve(const char *skill_name,
                         char ordered[][128], size_t *count, size_t max);
char **skill_deps_get_missing(const char *skill_name, size_t *count);
bool skill_deps_validate_order(const char ordered[][128], size_t count);

/* L04: xAI model retirement detection */
bool xai_is_model_retired(const char *model_name,
                           char *replacement_out, size_t replacement_sz,
                           char *reasoning_out, size_t reasoning_sz);

/* P49: Tool result preview generation — truncate at last newline within max_chars */
char *generate_preview(const char *content, int max_chars, bool *has_more);

/* D07: Delegate spawn pause — global gate for TUI/gateway */
bool set_spawn_paused(bool paused);
/* PoP: is_spawn_paused @ tools/delegate_tool.py:is_spawn_paused */
bool is_spawn_paused(void);

/** @} */ /* end of hermes group */

#endif /* HERMES_H */

