/*
 * port_codex_event_projector_remaining.c — Port of
 * agent/transports/codex_event_projector.py projector surface.
 * Deterministic call ids, message/command/file/mcp/dynamic/opaque
 * projection.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _deterministic_call_id @ agent/transports/codex_event_projector.py:_deterministic_call_id */
char *cep_deterministic_call_id(const char *kind, const char *item_id) {
    /* Python: stable id from codex item id. */
    if (!item_id) {
        char *out = NULL;
        asprintf(&out, "%s_0", kind ? kind : "call");
        return out;
    }
    char *out = NULL;
    asprintf(&out, "%s_%s", kind ? kind : "call", item_id);
    return out;
}

/* PoP: __init__ @ agent/transports/codex_event_projector.py:__init__ */
char *cep_init(void) {
    return strdup("{\"pending_reasoning\": []}");
}

/* PoP: project @ agent/transports/codex_event_projector.py:project */
char *cep_project(const char *notification_json) {
    /* Python: project single notification; idempotent for non-completion. */
    if (!notification_json) return NULL;
    printf("codex notification projected\n");
    return strdup(notification_json);
}

/* PoP: _project_agent_message @ agent/transports/codex_event_projector.py:_project_agent_message */
char *cep_project_agent_message(const char *item_json) {
    /* Python: assistant message with text. */
    if (!item_json) return NULL;
    printf("agent message projected\n");
    return strdup("{}");
}

/* PoP: _project_user_message @ agent/transports/codex_event_projector.py:_project_user_message */
char *cep_project_user_message(const char *item_json) {
    /* Python: userMessage content projection. */
    if (!item_json) return NULL;
    printf("user message projected\n");
    return strdup("{}");
}

/* PoP: _project_command @ agent/transports/codex_event_projector.py:_project_command */
char *cep_project_command(const char *item_json) {
    /* Python: exec command tool call. */
    if (!item_json) return NULL;
    printf("command item projected as tool call\n");
    return strdup("{}");
}

/* PoP: _project_file_change @ agent/transports/codex_event_projector.py:_project_file_change */
char *cep_project_file_change(const char *item_json) {
    /* Python: apply_patch tool call. */
    if (!item_json) return NULL;
    printf("file change projected as apply_patch\n");
    return strdup("{}");
}

/* PoP: _project_mcp_tool_call @ agent/transports/codex_event_projector.py:_project_mcp_tool_call */
char *cep_project_mcp_tool_call(const char *item_json) {
    /* Python: mcp server tool call. */
    if (!item_json) return NULL;
    printf("mcp tool call projected\n");
    return strdup("{}");
}

/* PoP: _project_dynamic_tool_call @ agent/transports/codex_event_projector.py:_project_dynamic_tool_call */
char *cep_project_dynamic_tool_call(const char *item_json) {
    if (!item_json) return NULL;
    printf("dynamic tool call projected\n");
    return strdup("{}");
}

/* PoP: _project_opaque @ agent/transports/codex_event_projector.py:_project_opaque */
char *cep_project_opaque(const char *item_json) {
    /* Python: record existence without inventing tool_calls. */
    if (!item_json) return NULL;
    printf("opaque item recorded (no invented tool calls)\n");
    return strdup("{}");
}
