/*
 * approval.h — minimal public API for the process-approval subsystem.
 *
 * This header exists so god-header-free ports (e.g. src/tools/port_approval.c)
 * can call the live approval primitives without pulling in hermes.h. Only the
 * small set of functions one module may legitimately call from another is
 * declared here; everything else stays file-local.
 */

#ifndef HERMES_APPROVAL_H
#define HERMES_APPROVAL_H

#include <stdbool.h>
#include "hermes_core_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Enable/disable YOLO mode (skip approval prompts for the current session). */
void approval_set_yolo(bool enabled);

/* True when YOLO mode is active for the current session. */
bool approval_is_yolo_enabled(void);

/* True when process-scoped YOLO mode was frozen at startup (HERMES_YOLO_MODE
 * read once at import — a mid-process skill cannot flip it). */
bool is_yolo_mode_frozen(void);

/* _get_approval_mode — approvals.mode config accessor, default "ask". */
const char *get_approval_mode(void);

/* Load the permanent allowlist of pre-approved command patterns. */
void approval_load_allowlist(void);

/* Permanent allowlist enumeration + bulk persistence (tools/approval.py
 * load_permanent_allowlist / save_permanent_allowlist / load_permanent). */
/* List permanent allowlist command strings (malloc'd NULL-term array, caller frees). */
char **approval_permanent_allowlist_list(size_t *out_n);
/* Replace the permanent allowlist with the given patterns and persist. */
int approval_save_permanent_allowlist(const char **patterns, size_t n);
/* Reload permanent entries from disk (see new entries in a live process). */
void approval_load_permanent(const char **patterns, size_t n);

/* Register (or clear, with NULL fn) the gateway-send approval callback. */
void approval_set_gateway_send(bool (*fn)(const char *, const char *, const char *),
                               const char *platform, const char *chat_id);

/* Core allow/deny decision for a tool invocation (tools/approval.py:check). */
int approval_check(const char *tool_name, const char *args_json);

/* Gateway approval wiring + clarify async prompts + cache + timeout */
void approval_reset_session(void);
void approval_set_allowlist_path(const char *path);
void approval_save_allowlist(void);
void approval_set_gateway_wait(char *(*fn)(int timeout_sec));
void gw_approval_set_context(const char *platform, const char *chat_id);
void clarify_set_gateway_send(bool (*fn)(const char *, const char *, const char *,
                                         const char **, int, const char *),
                               const char *platform, const char *chat_id);
void clarify_set_gateway_wait(char *(*fn)(int timeout_sec));
void clarify_set_gateway_begin(void (*fn)(const char *, const char *, const char *,
                                          const char *, const char (*)[256], int));
void clarify_set_gateway_context(const char *platform, const char *chat_id,
                                  bool (*send_fn)(const char *, const char *, const char *));
int approval_cache_count(void);
const char *approval_cache_entry(int index);
void approval_cache_clear_last(int n);
void approval_set_timeout(int seconds);
int approval_get_timeout(void);

/* Declared in port_approval_ports.c (PoP: _has_allowlist_shell_operator).
 * Forward declaration so approval.c can use it for derive_glob. */
bool apr_has_allowlist_shell_operator(const char *command);

/* PoP: parse_apply_indices @ hermes_cli/approvals_suggest.py:parse_apply_indices */
/* Returns count, or -1 on ValueError (invalid/out of range/no valid). */
int approval_parse_apply_indices(const char *spec, int total, int *out, int max_out);

/* PoP: derive_glob @ hermes_cli/approvals_suggest.py:derive_glob */
/* Returns a malloc'd command glob, or NULL for compound/unsafe commands. */
char *approval_derive_glob(const char *normalized);

/* PoP: normalize_command @ hermes_cli/approvals_suggest.py:normalize_command */
/* Returns a malloc'd ANI/NFKC-stripped, whitespace-collapsed command. */
char *approval_normalize_command(const char *command);

/* Dangerous-command classification (tools/approval.py). Returns a malloc'd
 * tab-separated string "true|false\t<key>\t<description>" or NULL on OOM. */
char *apr_detect_dangerous_command(const char *command);
char *approval_detect_hardline_command(const char *command);

/* PoP: is_unsafe_class @ hermes_cli/approvals_suggest.py:is_unsafe_class */
bool approval_is_unsafe_class(const char *description);

/* ---- denial-breaker + bypass helpers (tools/approval.py) ----
 * Co-located in port_approval_helpers2.c. */

/* _is_cron_approval_context — true inside a cron session. */
bool apr2_is_cron_approval_context(void);
/* _get_denial_breaker_threshold — approvals.denial_breaker_threshold, default 3. */
int apr2_get_denial_breaker_threshold(void);
/* _record_denial — increment + return the session's consecutive-denial count. */
int apr2_record_denial(const char *session_key);
/* _reset_denials — clear the session's consecutive-denial tally. */
void apr2_reset_denials(const char *session_key);
/* _denial_breaker_addendum — escalated hard-stop text when the breaker trips. */
char *apr2_denial_breaker_addendum(const char *session_key);
/* is_approval_bypass_active_for_session — collapsed three-source bypass check. */
bool apr2_is_approval_bypass_active_for_session(const char *session_key);
/* _release_permission_mode_dependents — drop YOLO-derived per-session resources. */
void apr2_release_permission_mode_dependents(const char *session_key);
/* _get_smart_policy — approvals.smart_policy text (stripped), default "". */
char *apr2_get_smart_policy(void);
/* _mask_quoted_newlines — replace newlines inside quotes with a space. */
char *apr2_mask_quoted_newlines(const char *command);
/* _save_blocked_payload — persist a parser-limit-blocked command as a script. */
char *apr2_save_blocked_payload(const char *command);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_APPROVAL_H */
