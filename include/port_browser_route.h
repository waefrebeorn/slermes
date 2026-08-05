/* port_browser_route.h — C11 port of tools/computer_use/browser_route.py
 *
 * Session-scoped typed-browser routing for the cua-driver. Strict adapter
 * between namespaced `cua_browser_*` actions and cua-driver's raw
 * `get_browser_state` / `browser_*` tools. Pure state machine (no I/O beyond
 * the injected call_tool/has_tool callbacks).
 *
 * Faithful-port notes:
 *  - `BrowserRouteState.refs` (ref -> set[action]) is modelled as a dynamic
 *    array of {ref, json_t* actions}. Action sets are json_t* string arrays.
 *  - `observe`/`prepare`/`mutate` take JSON-object args and return JSON-object
 *    payloads, exactly like the Python originals, so the oracle can compare
 *    full structured results (not just booleans).
 *  - `_require_ref`'s `actions` parameter is a json_t* string array (NULL = no
 *    action constraint).
 */

#ifndef PORT_BROWSER_ROUTE_H
#define PORT_BROWSER_ROUTE_H

#include <stdbool.h>
#include <stddef.h>
#include "../lib/libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Injected transport. call_tool returns a fresh json_t* payload (caller frees).
 * has_tool returns whether the named driver tool is advertised. */
typedef json_t *(*browser_route_call_fn)(const char *name, const json_t *args, void *ctx);
typedef bool    (*browser_route_has_fn)(const char *name, void *ctx);

/* One ref -> declared actions mapping. */
typedef struct browser_ref {
    char    *ref;
    json_t  *actions;   /* json_t* array of strings (owned) */
} browser_ref_t;

/* BrowserRouteState: capabilities minted for one explicit driver session. */
typedef struct browser_route_state {
    long     pid;                 /* -1 = None */
    long     window_id;          /* -1 = None */
    char    *target_id;          /* NULL = None */
    char   **tab_ids;            /* set of strings */
    size_t   n_tab_ids;
    char    *tab_id;             /* NULL = None */
    char    *binding_quality;    /* NULL = None */
    bool     mutation_allowed;
    browser_ref_t *refs;         /* ref -> actions */
    size_t   n_refs;
    char    *continuation;       /* NULL = None */
    bool     verification_required;
} browser_route_state_t;

void browser_route_state_init(browser_route_state_t *s);
void browser_route_state_free(browser_route_state_t *s);
void browser_route_state_clear_refs(browser_route_state_t *s);
void browser_route_state_clear(browser_route_state_t *s);

/* --- module-level pure helpers --- */
long   browser_route_positive_int(const json_t *value);
/* _tool_payload: extract structured driver payload without discarding refusals. */
json_t *browser_route_tool_payload(const json_t *out);
/* _ref_map: normalize semantic-v2 action refs to ref -> actions[]. */
json_t *browser_route_ref_map(const json_t *payload);
/* _continuation */
char *browser_route_continuation(const json_t *payload);
/* _tab_ids */
json_t *browser_route_tab_ids(const json_t *payload);
/* _refusal_code */
char *browser_route_refusal_code(const json_t *payload);
/* _refusal */
json_t *browser_route_refusal(const char *code, const char *message, bool native_fallback, const json_t *extra);

/* --- the adapter --- */
typedef struct cua_typed_browser_route cua_typed_browser_route_t;

cua_typed_browser_route_t *cua_typed_browser_route_new(
    const char *session_id,
    browser_route_call_fn call_tool, void *call_ctx,
    browser_route_has_fn has_tool, void *has_ctx);
void cua_typed_browser_route_free(cua_typed_browser_route_t *r);

const browser_route_state_t *cua_typed_browser_route_state(const cua_typed_browser_route_t *r);

/* observe(*, pid, window_id, tab_id, snapshot_format, query, scope_ref, continuation)
 * — args as a json_t* object. Returns a fresh payload (caller frees). */
json_t *cua_typed_browser_route_observe(cua_typed_browser_route_t *r, const json_t *args);
/* prepare(*, pid, window_id, profile_mode, profile_name, allow_launch) */
json_t *cua_typed_browser_route_prepare(cua_typed_browser_route_t *r, const json_t *args);
/* mutate(tool, *, tab_id, args) — args json_t* object; returns fresh payload. */
json_t *cua_typed_browser_route_mutate(cua_typed_browser_route_t *r, const char *tool, const json_t *args);

#ifdef __cplusplus
}
#endif

#endif /* PORT_BROWSER_ROUTE_H */
