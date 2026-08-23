/*
 * browser_extension_router.h — C11 port of tools/browser_extension_router.py
 *
 * Registry-level browser extension router (agent side): decides, for one
 * `browser_*` handler invocation, whether the command runs on an attached
 * extension controller or the legacy backend. Feature off ⇒ legacy,
 * untouched. Missing server-bound identity ⇒ legacy. Bound identity ⇒
 * authoritative extension lane (fail closed). A selected controller is
 * authoritative — never retried through the legacy backend.
 */
#ifndef SLERMES_BROWSER_EXTENSION_ROUTER_H
#define SLERMES_BROWSER_EXTENSION_ROUTER_H

#include "hermes_json.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Route outcome codes. */
typedef enum {
    BCR_ROUTED = 0,          /* dispatched to a controller; out = result JSON string */
    BCR_FELL_BACK = 1,       /* legacy fallback() invoked exactly once */
    BCR_ERR_CONTROLLER_UNAVAILABLE = -1 /* bound lane but no usable controller */
} bcr_route_result_t;

/* Broker vtable — injectable twin of gateway.browser_control_broker.
 * scope handles are opaque json_t* payloads minted by the broker side. */
typedef struct {
    void *ctx;
    /* scope_for_session(session_id, task_id, principal_id, transport_family)
     * → scope handle (non-NULL) or NULL. */
    void *(*scope_for_session)(void *ctx, const char *session_id,
                               const char *task_id, const char *principal_id,
                               const char *transport_family);
    /* select(scope, capability) → 1 when exactly one connected controller
     * matches identity + negotiated capability. */
    int (*select)(void *ctx, void *scope, const char *capability);
    /* dispatch(scope, action, arguments_json, tool_call_id) → malloc'd JSON
     * string of the completion result; NULL on failure. */
    char *(*dispatch)(void *ctx, void *scope, const char *action,
                      const char *arguments_json, const char *tool_call_id);
    /* lane_registered(session_id, task_id, principal_id, family) → 1 when
     * ANY controller ever registered for this lane. */
    int (*lane_registered)(void *ctx, const char *session_id,
                           const char *task_id, const char *principal_id,
                           const char *transport_family);
} bcr_broker_vtab_t;

/* Legacy backend callback: invoked at most once per route. */
typedef char *(*bcr_fallback_fn)(void *ctx);

/* Session-env getter for server-bound identity (defaults to getenv). */
typedef const char *(*bcr_env_fn)(const char *key, void *ctx);

void bcr_set_env_getter(bcr_env_fn fn, void *ctx);
void bcr_set_broker_vtab(const bcr_broker_vtab_t *vtab);

/* extension_controller_available @ tools/browser_extension_router.py:
 * extension_controller_available — whether this request owns one exact
 * controller capable of ``action``. Fails closed. */
int bcr_extension_controller_available(const char *action);

/* route_browser_tool @ ...:route_browser_tool — route one action.
 * args_json: tool arguments as received from the model (never mutated).
 * On BCR_ROUTED, *out is a malloc'd result JSON string; else NULL. */
bcr_route_result_t bcr_route_browser_tool(
    const char *action, const char *args_json, bcr_fallback_fn fallback,
    void *fallback_ctx, const bcr_broker_vtab_t *broker, int enabled,
    const char *session_id, const char *task_id, const char *principal_id,
    const char *transport_family, const char *tool_call_id, char **out);

/* current_tool_call_id @ ...:current_tool_call_id — active observability
 * binding or "" (malloc'd). */
char *bcr_current_tool_call_id(void);

/* routed_browser_handler @ ...:routed_browser_handler — lazy wrapper that
 * resolves the feature flag and broker before routing. */
bcr_route_result_t bcr_routed_browser_handler(
    const char *action, const char *args_json, bcr_fallback_fn fallback,
    void *fallback_ctx, const char *session_id, const char *task_id,
    const char *principal_id, const char *transport_family,
    const char *tool_call_id, char **out);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BROWSER_EXTENSION_ROUTER_H */
