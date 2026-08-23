/*
 * browser_extension_router.c — C11 port of tools/browser_extension_router.py
 *
 * Routing contract:
 * - Feature off ⇒ legacy, untouched; fallback() exactly once.
 * - No server-bound identity ⇒ legacy (generic Hermes callers keep their
 *   backend).
 * - Bound identity ⇒ authoritative extension lane: missing/ambiguous scope,
 *   disconnect, or capability mismatch fail closed.
 * - Selected controller ⇒ authoritative: its result is returned and the
 *   legacy backend is never retried, even on failure.
 * - args are never mutated; the broker copies arguments into the frame.
 */
#define _POSIX_C_SOURCE 200809L

#include "browser_extension_router.h"
#include "browser_control_broker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static bcr_env_fn bcr_env_get = NULL;
static void *bcr_env_ctx = NULL;
static bcr_broker_vtab_t bcr_broker;

void bcr_set_env_getter(bcr_env_fn fn, void *ctx)
{
    bcr_env_get = fn;
    bcr_env_ctx = ctx;
}

void bcr_set_broker_vtab(const bcr_broker_vtab_t *vtab)
{
    if (vtab) bcr_broker = *vtab;
    else memset(&bcr_broker, 0, sizeof(bcr_broker));
}

/* get_session_env(key, "") analog */
static const char *bcr_session_env(const char *key)
{
    if (bcr_env_get) return bcr_env_get(key, bcr_env_ctx);
    return getenv(key);
}

static int bcr_str_blank(const char *s)
{
    if (!s) return 1;
    for (const char *p = s; *p; p++)
        if (!isspace((unsigned char)*p)) return 0;
    return 1;
}

/* PoP: extension_controller_available @ tools/browser_extension_router.py:
 * extension_controller_available */
int bcr_extension_controller_available(const char *action)
{
    /* try-imports degrade to "unavailable" — fail closed. */
    if (!action) return 0;
    if (!bcb_browser_control_enabled(NULL))
        return 0;
    const char *session_id = bcr_session_env("HERMES_SESSION_ID");
    const char *principal_id =
        bcr_session_env("HERMES_BROWSER_CONTROL_PRINCIPAL");
    const char *family =
        bcr_session_env("HERMES_BROWSER_CONTROL_TRANSPORT_FAMILY");
    /* `or None`: blank env values read as missing */
    if (!session_id || !session_id[0] || !principal_id || !principal_id[0] ||
        !family || !family[0])
        return 0;
    const bcr_broker_vtab_t *broker = &bcr_broker;
    if (!broker->scope_for_session || !broker->select) {
        /* live broker twin: consult the process-local C broker directly */
        bcb_broker_t *b = bcb_get_browser_control_broker();
        bcb_scope_t *scope = bcb_broker_scope_for_session(
            b, session_id, NULL, principal_id, family);
        if (!scope) return 0;
        int ok = bcb_broker_select(b, scope, action);
        bcb_scope_free(scope);
        free(scope);
        return ok;
    }
    void *scope = broker->scope_for_session(broker->ctx, session_id, NULL,
                                            principal_id, family);
    if (!scope) return 0;
    return broker->select(broker->ctx, scope, action);
}

/* PoP: route_browser_tool @ tools/browser_extension_router.py:route_browser_tool */
bcr_route_result_t bcr_route_browser_tool(
    const char *action, const char *args_json, bcr_fallback_fn fallback,
    void *fallback_ctx, const bcr_broker_vtab_t *broker, int enabled,
    const char *session_id, const char *task_id, const char *principal_id,
    const char *transport_family, const char *tool_call_id, char **out)
{
    if (out) *out = NULL;
    if (!enabled)
        goto fallback;

    /* not str(principal_id or "").strip() or not str(transport_family...).strip() */
    if (!broker) goto fallback;
    if (bcr_str_blank(principal_id) || bcr_str_blank(transport_family))
        goto fallback;

    void *scope = NULL;
    if (broker->scope_for_session)
        scope = broker->scope_for_session(broker->ctx, session_id, task_id,
                                          principal_id, transport_family);
    if (!scope) {
        /* A stamped identity alone does not make the extension lane
         * authoritative: with no lane ever registered, generic callers keep
         * the legacy backend; once a lane registered, fail closed. */
        int lane_bound = 1; /* callable(lane_bound) default in Python */
        if (broker->lane_registered)
            lane_bound = broker->lane_registered(
                broker->ctx, session_id, task_id, principal_id,
                transport_family);
        if (!lane_bound)
            goto fallback;
        return BCR_ERR_CONTROLLER_UNAVAILABLE; /* ControllerUnavailable */
    }

    if (!broker->select || !broker->select(broker->ctx, scope, action))
        return BCR_ERR_CONTROLLER_UNAVAILABLE;

    /* A controller was selected: authoritative. Never retry through the
     * existing backend, whatever happens here. Decoded values serialize at
     * this boundary; string results pass byte-for-byte. */
    char *result =
        broker->dispatch(broker->ctx, scope, action,
                         args_json ? args_json : "{}",
                         tool_call_id ? tool_call_id : "");
    if (!result)
        return BCR_ERR_CONTROLLER_UNAVAILABLE; /* dispatch failure propagates */
    if (out) *out = result;
    else free(result);
    return BCR_ROUTED;

fallback:
    if (fallback) {
        char *fb_result = fallback(fallback_ctx); /* legacy return value */
        if (fb_result) {
            if (out) *out = fb_result;
            else free(fb_result);
        }
    }
    return BCR_FELL_BACK;
}

/* PoP: current_tool_call_id @ tools/browser_extension_router.py:
 * current_tool_call_id — the approval observability binding; bare/offline
 * callers have none and get "". */
char *bcr_current_tool_call_id(void)
{
    /* tools.approval._approval_tool_call_id binding: the observability
     * context set immediately before registry dispatch. */
    extern void approval_get_current_observability_context(char *buf,
                                                           size_t sz);
    size_t cap = 256;
    char *id = malloc(cap);
    if (!id) return strdup("");
    approval_get_current_observability_context(id, cap);
    if (!id[0]) {
        free(id);
        return strdup("");
    }
    return id;
}

/* PoP: routed_browser_handler @ tools/browser_extension_router.py:
 * routed_browser_handler */
bcr_route_result_t bcr_routed_browser_handler(
    const char *action, const char *args_json, bcr_fallback_fn fallback,
    void *fallback_ctx, const char *session_id, const char *task_id,
    const char *principal_id, const char *transport_family,
    const char *tool_call_id, char **out)
{
    if (out) *out = NULL;

    /* Lazy feature-flag resolution so the default path costs one cached
     * config read + immediate fallback. Gateway unavailable ⇒ legacy. */
    if (!bcb_browser_control_enabled(NULL)) {
        if (fallback) fallback(fallback_ctx);
        return BCR_FELL_BACK;
    }

    char *resolved_call_id = NULL;
    if (!tool_call_id) {
        resolved_call_id = bcr_current_tool_call_id();
        tool_call_id = resolved_call_id;
    }
    if (!session_id || !session_id[0])
        session_id = bcr_session_env("HERMES_SESSION_ID");
    if (!principal_id || !principal_id[0])
        principal_id = bcr_session_env("HERMES_BROWSER_CONTROL_PRINCIPAL");
    if (!transport_family || !transport_family[0])
        transport_family =
            bcr_session_env("HERMES_BROWSER_CONTROL_TRANSPORT_FAMILY");

    bcr_route_result_t rc = bcr_route_browser_tool(
        action, args_json, fallback, fallback_ctx, &bcr_broker, 1,
        session_id, task_id, principal_id, transport_family, tool_call_id,
        out);
    free(resolved_call_id);
    return rc;
}
