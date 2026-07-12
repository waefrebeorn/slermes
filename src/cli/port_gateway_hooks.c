/*
 * port_gateway_hooks.c — C port of gateway/hooks.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_hooks_discover_and_load @ gateway/hooks.py:discover_and_load */

/* Port of Python gateway/hooks.py:discover_and_load */
/* Scans the hooks directory for hook directories and loads their handlers. */
int cli_gateway_hooks_discover_and_load(const char *hooks_dir)
{
    if (!hooks_dir || !hooks_dir[0]) {
        return 0;
    }
    /* CLI port: hook discovery requires filesystem scan + dynamic module load. */
    /* Return 0 (no hooks loaded). */
    hermes_log(LOG_DEBUG, "hooks", "hook discovery skipped in CLI port (dir: %s)", hooks_dir);
    return 0;
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
