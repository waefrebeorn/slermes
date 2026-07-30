/*
 * port_gateway_hooks.c — C port of gateway/hooks.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Hook registry ──
 * Mirrors the Python HookManager._loaded_hooks list. Each entry is the
 * metadata dict a loaded hook exposes (name, events, source). The CLI port
 * keeps a real, inspectable registry even though dynamic handler dispatch
 * (Python HOOK.yaml + handler.py) is not loaded here.
 */
#define MAX_LOADED_HOOKS 64

typedef struct {
    char name[128];
    char events[256];   /* comma-separated event types */
    char source[256];   /* hook directory or "builtin" */
} loaded_hook_t;

static loaded_hook_t g_loaded_hooks[MAX_LOADED_HOOKS];
static int g_loaded_hooks_count = 0;

/* PoP: cli_gateway_hooks__register_builtin_hooks @ gateway/hooks.py:_register_builtin_hooks */

/* Port of Python gateway/hooks.py:_register_builtin_hooks().
 * Registers built-in hooks that are always active. Currently empty — no
 * shipped built-in hooks (matches Python: kept as the extension point for
 * future always-on gateway hooks). */
void cli_gateway_hooks__register_builtin_hooks(void)
{
    /* No built-in hooks shipped. The registry is the drop-in point. */
    hermes_log(LOG_DEBUG, "hooks", "register_builtin_hooks: none shipped (extension point)");
}

/* PoP: cli_gateway_hooks_loaded_hooks @ gateway/hooks.py:loaded_hooks */

/* Port of Python gateway/hooks.py:loaded_hooks().
 * Returns metadata about all loaded hooks as a JSON array string (caller
 * frees), mirroring `list(self._loaded_hooks)`. Returns "[]" when none. */
char *cli_gateway_hooks_loaded_hooks(void)
{
    size_t cap = 16;
    for (int i = 0; i < g_loaded_hooks_count; i++) {
        cap += strlen(g_loaded_hooks[i].name) + strlen(g_loaded_hooks[i].events)
             + strlen(g_loaded_hooks[i].source) + 48;
    }
    char *out = malloc(cap);
    if (!out) return NULL;
    int pos = 0;
    pos += snprintf(out + pos, cap - pos, "[");
    for (int i = 0; i < g_loaded_hooks_count; i++) {
        pos += snprintf(out + pos, cap - pos,
            "%s{\"name\":\"%s\",\"events\":\"%s\",\"source\":\"%s\"}",
            (i ? "," : ""),
            g_loaded_hooks[i].name,
            g_loaded_hooks[i].events,
            g_loaded_hooks[i].source);
    }
    snprintf(out + pos, cap - pos, "]");
    return out;
}

/* PoP: cli_gateway_hooks_discover_and_load @ gateway/hooks.py:discover_and_load */

/* Port of Python gateway/hooks.py:discover_and_load */
/* Scans the hooks directory for hook directories and loads their handlers. */
int cli_gateway_hooks_discover_and_load(const char *hooks_dir)
{
    cli_gateway_hooks__register_builtin_hooks();
    if (!hooks_dir || !hooks_dir[0]) {
        return g_loaded_hooks_count;
    }
    /* CLI port: hook discovery requires filesystem scan + dynamic module load. */
    hermes_log(LOG_DEBUG, "hooks", "hook discovery skipped in CLI port (dir: %s)", hooks_dir);
    return g_loaded_hooks_count;
}

/* PoP: cli_gateway_hooks__resolve_handlers @ gateway/hooks.py:_resolve_handlers */

/* Port of Python gateway/hooks.py:_resolve_handlers */
/* Returns all handlers that should fire for an event type. */
int cli_gateway_hooks__resolve_handlers(
    const char *event_type, char *handler_names[], int max_handlers)
{
    if (!event_type || !handler_names || max_handlers <= 0) {
        return 0;
    }
    /* CLI port: no handlers registered. */
    return 0;
}

/* PoP: cli_gateway_hooks_emit @ gateway/hooks.py:emit */

/* Port of Python gateway/hooks.py:emit */
/* Fires all handlers registered for an event. */
int cli_gateway_hooks_emit(const char *event_type, const char *context_json)
{
    if (!event_type) {
        return -1;
    }
    (void)context_json;
    /* CLI port: async handler dispatch not available. */
    hermes_log(LOG_DEBUG, "hooks", "emit(%s) — CLI port, no-op", event_type);
    return 0;
}

/* PoP: cli_gateway_hooks_emit_collect @ gateway/hooks.py:emit_collect */

/* Port of Python gateway/hooks.py:emit_collect */
/* Fires handlers and returns their non-None return values. */
int cli_gateway_hooks_emit_collect(
    const char *event_type, const char *context_json,
    char *results[], int max_results)
{
    if (!event_type || !results || max_results <= 0) {
        return 0;
    }
    (void)context_json;
    /* CLI port: async handler dispatch not available. */
    hermes_log(LOG_DEBUG, "hooks", "emit_collect(%s) — CLI port, no-op", event_type);
    return 0;
}
