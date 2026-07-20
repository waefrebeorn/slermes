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
#include "hermes_cli.h"
#include "hermes_cron.h"
#include "hermes_skills.h"

/* ================================================================
 *  Entry Points
 * ================================================================ */
/* Port of Python: cli_main */
int  cli_main(int argc, char **argv);
int  hermes_gateway_main(int argc, char **argv);

/* Command system — declared in hermes_cli.h (included above) */

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

/* Registry accessors — declared in registry.h (included below).
 * Kept as a re-include so legacy consumers of hermes.h keep compiling
 * while translation units that only need the registry can include
 * registry.h directly (god-header elimination). */
#include "registry.h"

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
/* Recursively scrub surrogate code points from a json_t tree (string values +
 * key names), returning true if any were replaced. Port of Python
 * _sanitize_structure_surrogates. Caller owns the mutated node. */
bool  agent_message_sanitize_structure_surrogates(json_t *node);
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

/* Cron/Scheduler (P169-P178) and Skills (P179-P188) declarations moved to
 * hermes_cron.h and hermes_skills.h respectively (both included above).
 * This keeps the umbrella hermes.h from being a god header. */

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

/* ------------------------------------------------------------------ */
/* CLI config-setup helpers (src/cli/config_setup.c — ports of         */
/* hermes_cli/setup.py).                                               */
/* ------------------------------------------------------------------ */

/* Gateway platform short label: strip trailing parenthetical qualifier.
 * Returns malloc'd string (caller frees). Port of setup.py:
 * _gateway_platform_short_label. */
char *setup_gateway_platform_short_label(const char *label);

/* Whether a provider supports multi-key "same provider pool" rotation.
 * Port of setup.py:_supports_same_provider_pool_setup. True for providers
 * whose auth_type is api_key/oauth_device_code (custom/openrouter handled
 * as special cases; unknown providers -> false). */
bool config_setup_supports_same_provider_pool_setup(const char *provider);

#endif /* HERMES_H */

