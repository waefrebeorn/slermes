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

#ifdef __cplusplus
}
#endif

#endif /* HERMES_APPROVAL_H */
