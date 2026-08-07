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

/* Load the permanent allowlist of pre-approved command patterns. */
void approval_load_allowlist(void);

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

/* PoP: is_unsafe_class @ hermes_cli/approvals_suggest.py:is_unsafe_class */
bool approval_is_unsafe_class(const char *description);

/* PoP: derive_glob @ hermes_cli/approvals_suggest.py:derive_glob */
/* Returns a malloc'd command glob, or NULL for compound/unsafe commands. */
char *approval_derive_glob(const char *normalized);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_APPROVAL_H */
