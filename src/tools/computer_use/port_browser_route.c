/* port_browser_route.c — C11 port of tools/computer_use/browser_route.py
 *
 * See port_browser_route.h for the faithful-port contract.
 */

#include "port_browser_route.h"

#include <stdlib.h>

/* forward decls for internal helpers (defined below) */
static json_t *br_deep_copy(const json_t *v);
static json_t *br_obj_get(const json_t *obj, const char *key);
#include <string.h>
#include <stdio.h>

/* ---- state ---- */
void browser_route_state_init(browser_route_state_t *s) {
    memset(s, 0, sizeof *s);
    s->pid = -1;
    s->window_id = -1;
}
void browser_route_state_free(browser_route_state_t *s) {
    if (!s) return;
    free(s->target_id);
    for (size_t i = 0; i < s->n_tab_ids; i++) free(s->tab_ids[i]);
    free(s->tab_ids);
    free(s->tab_id);
    free(s->binding_quality);
    for (size_t i = 0; i < s->n_refs; i++) {
        free(s->refs[i].ref);
        if (s->refs[i].actions) json_free(s->refs[i].actions);
    }
    free(s->refs);
    free(s->continuation);
}
/* PoP: clear_refs @ tools/computer_use/browser_route.py:BrowserRouteState.clear_refs */
void browser_route_state_clear_refs(browser_route_state_t *s) {
    for (size_t i = 0; i < s->n_refs; i++) {
        free(s->refs[i].ref);
        if (s->refs[i].actions) json_free(s->refs[i].actions);
    }
    free(s->refs);
    s->refs = NULL;
    s->n_refs = 0;
    free(s->continuation);
    s->continuation = NULL;
}
/* PoP: clear @ tools/computer_use/browser_route.py:BrowserRouteState.clear */
void browser_route_state_clear(browser_route_state_t *s) {
    s->pid = -1;
    s->window_id = -1;
    free(s->target_id); s->target_id = NULL;
    for (size_t i = 0; i < s->n_tab_ids; i++) free(s->tab_ids[i]);
    free(s->tab_ids); s->tab_ids = NULL; s->n_tab_ids = 0;
    free(s->tab_id); s->tab_id = NULL;
    free(s->binding_quality); s->binding_quality = NULL;
    s->mutation_allowed = false;
    browser_route_state_clear_refs(s);
    s->verification_required = false;
}

/* helper: set owned string */
static void set_str(char **dst, const char *v) {
    free(*dst);
    *dst = v ? strdup(v) : NULL;
}

/* ---- pure helpers ---- */

/* PoP: _positive_int @ tools/computer_use/browser_route.py:_positive_int */
long browser_route_positive_int(const json_t *value) {
    if (!value) return -1;
    if (value->type == JSON_BOOL) return -1;  /* bool is not int */
    if (value->type == JSON_NUMBER) {
        long v = (long)value->num_val;
        return v > 0 ? v : -1;
    }
    if (value->type == JSON_STRING) {
        /* try parse */
        char *end; long v = strtol(value->str_val, &end, 10);
        if (*end == '\0' && end != value->str_val && v > 0) return v;
        return -1;
    }
    return -1;
}

/* PoP: _tool_payload @ tools/computer_use/browser_route.py:_tool_payload */
json_t *browser_route_tool_payload(const json_t *out) {
    json_t *payload = json_object();
    if (!out || out->type != JSON_OBJECT) return payload;
    const json_t *data = br_obj_get(out, "data");
    if (data && data->type == JSON_OBJECT) {
        for (size_t i = 0; i < data->c.count; i++)
            json_set(payload, data->c.keys[i], br_deep_copy(data->c.items[i]));
    } else if (data && data->type == JSON_STRING && data->str_val && *data->str_val) {
        json_set(payload, "message", json_string(data->str_val));
    }
    const json_t *structured = br_obj_get(out, "structuredContent");
    if (structured && structured->type == JSON_OBJECT) {
        for (size_t i = 0; i < structured->c.count; i++)
            json_set(payload, structured->c.keys[i], br_deep_copy(structured->c.items[i]));
    }
    const json_t *is_err = br_obj_get(out, "isError");
    if (is_err && is_err->type == JSON_BOOL && is_err->bool_val &&
        br_obj_get(payload, "isError") == NULL)
        json_set(payload, "isError", json_bool(true));
    return payload;
}

/* object key lookup (libjson's json_get is array-by-index only) */
static json_t *br_obj_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->c.count; i++)
        if (strcmp(obj->c.keys[i], key) == 0) return (json_t*)obj->c.items[i];
    return NULL;
}
/* deep copy helper (libjson has no built-in) */
static json_t *br_deep_copy(const json_t *v) {
    if (!v) return NULL;
    switch (v->type) {
    case JSON_NULL: return json_null();
    case JSON_BOOL: return json_bool(v->bool_val);
    case JSON_NUMBER: return json_number(v->num_val);
    case JSON_STRING: return json_string(v->str_val);
    case JSON_ARRAY: {
        json_t *a = json_array();
        for (size_t i = 0; i < v->c.count; i++) json_append(a, br_deep_copy(v->c.items[i]));
        return a;
    }
    case JSON_OBJECT: {
        json_t *o = json_object();
        for (size_t i = 0; i < v->c.count; i++)
            json_set(o, v->c.keys[i], br_deep_copy(v->c.items[i]));
        return o;
    }
    }
    return json_null();
}

/* PoP: _ref_map @ tools/computer_use/browser_route.py:_ref_map */
json_t *browser_route_ref_map(const json_t *payload) {
    /* returns json_t* object: ref -> actions(array) */
    json_t *normalized = json_object();
    if (!payload || payload->type != JSON_OBJECT) return normalized;
    const json_t *snapshot = br_obj_get(payload, "snapshot");
    const json_t *raw = br_obj_get(payload, "content_refs");
    if (!raw || raw->type == JSON_NULL) raw = br_obj_get(payload, "refs");
    if ((!raw || raw->type == JSON_NULL) && snapshot && snapshot->type == JSON_OBJECT)
        raw = br_obj_get(snapshot, "refs");

    if (raw && raw->type == JSON_OBJECT) {
        for (size_t i = 0; i < raw->c.count; i++) {
            char *key = raw->c.keys[i];
            const json_t *value = raw->c.items[i];
            char *ref = NULL; json_t *actions = NULL;
            if (value && value->type == JSON_OBJECT) {
                const json_t *rv = br_obj_get(value, "ref");
                ref = (rv && rv->type == JSON_STRING && rv->str_val && *rv->str_val)
                    ? strdup(rv->str_val) : strdup(key);
                const json_t *acts = br_obj_get(value, "actions");
                if (acts && acts->type == JSON_ARRAY) {
                    actions = json_array();
                    for (size_t j = 0; j < acts->c.count; j++)
                        if (acts->c.items[j]->type == JSON_STRING)
                            json_append(actions, json_string(acts->c.items[j]->str_val));
                }
            } else {
                ref = strdup(key);
            }
            if (ref && *ref) {
                json_t *arr = actions ? actions : json_array();
                json_set(normalized, ref, arr);
            }
            free(ref);
            if (actions) json_free(actions);
        }
    } else if (raw && raw->type == JSON_ARRAY) {
        for (size_t i = 0; i < raw->c.count; i++) {
            const json_t *item = raw->c.items[i];
            if (item && item->type == JSON_OBJECT) {
                const json_t *rv = br_obj_get(item, "ref");
                if (rv && rv->type == JSON_STRING && rv->str_val && *rv->str_val) {
                    json_t *arr = json_array();
                    const json_t *acts = br_obj_get(item, "actions");
                    if (acts && acts->type == JSON_ARRAY) {
                        for (size_t j = 0; j < acts->c.count; j++)
                            if (acts->c.items[j]->type == JSON_STRING)
                                json_append(arr, json_string(acts->c.items[j]->str_val));
                    }
                    json_set(normalized, rv->str_val, arr);
                }
            }
        }
    }
    return normalized;
}

/* PoP: _continuation @ tools/computer_use/browser_route.py:_continuation */
char *browser_route_continuation(const json_t *payload) {
    if (payload && payload->type == JSON_OBJECT) {
        const json_t *d = br_obj_get(payload, "continuation");
        if (d && d->type == JSON_STRING && *d->str_val)
            return strdup(d->str_val);
        const json_t *snap = br_obj_get(payload, "snapshot");
        if (snap && snap->type == JSON_OBJECT) {
            const json_t *n = br_obj_get(snap, "continuation");
            if (n && n->type == JSON_STRING && *n->str_val)
                return strdup(n->str_val);
        }
    }
    return NULL;
}

/* PoP: _tab_ids @ tools/computer_use/browser_route.py:_tab_ids */
json_t *browser_route_tab_ids(const json_t *payload) {
    json_t *result = json_array();
    if (!payload || payload->type != JSON_OBJECT) return result;
    const json_t *tabs = br_obj_get(payload, "tabs");
    if (!tabs || tabs->type != JSON_ARRAY) return result;
    for (size_t i = 0; i < tabs->c.count; i++) {
        const json_t *tab = tabs->c.items[i];
        if (!tab || tab->type != JSON_OBJECT) continue;
        const json_t *tid = br_obj_get(tab, "tab_id");
        if (!tid || tid->type != JSON_STRING || !*tid->str_val)
            tid = br_obj_get(tab, "id");
        if (tid && tid->type == JSON_STRING && *tid->str_val)
            json_append(result, json_string(tid->str_val));
    }
    return result;
}

/* PoP: _refusal_code @ tools/computer_use/browser_route.py:_refusal_code */
char *browser_route_refusal_code(const json_t *payload) {
    if (payload && payload->type == JSON_OBJECT) {
        const json_t *code = br_obj_get(payload, "code");
        if (code && code->type == JSON_STRING) return strdup(code->str_val);
        const json_t *refusal = br_obj_get(payload, "refusal");
        if (refusal && refusal->type == JSON_OBJECT) {
            const json_t *rc = br_obj_get(refusal, "code");
            if (rc && rc->type == JSON_STRING) return strdup(rc->str_val);
        }
    }
    return NULL;
}

/* PoP: _refusal @ tools/computer_use/browser_route.py:_refusal */
json_t *browser_route_refusal(const char *code, const char *message,
                              bool native_fallback, const json_t *extra) {
    json_t *p = json_object();
    json_set(p, "ok", json_bool(false));
    json_set(p, "status", json_string("refused"));
    json_set(p, "code", json_string(code ? code : ""));
    json_set(p, "message", json_string(message ? message : ""));
    if (native_fallback) json_set(p, "native_fallback_required", json_bool(true));
    if (extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++)
            json_set(p, extra->c.keys[i], br_deep_copy(extra->c.items[i]));
    }
    return p;
}

/* ---- adapter ---- */
struct cua_typed_browser_route {
    char *session_id;
    browser_route_call_fn call_tool; void *call_ctx;
    browser_route_has_fn  has_tool;  void *has_ctx;
    browser_route_state_t state;
};

/* PoP: __init__ @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute.__init__ */
cua_typed_browser_route_t *cua_typed_browser_route_new(
    const char *session_id,
    browser_route_call_fn call_tool, void *call_ctx,
    browser_route_has_fn has_tool, void *has_ctx) {
    cua_typed_browser_route_t *r = calloc(1, sizeof *r);
    r->session_id = strdup(session_id ? session_id : "");
    r->call_tool = call_tool; r->call_ctx = call_ctx;
    r->has_tool = has_tool;     r->has_ctx = has_ctx;
    browser_route_state_init(&r->state);
    return r;
}
void cua_typed_browser_route_free(cua_typed_browser_route_t *r) {
    if (!r) return;
    free(r->session_id);
    browser_route_state_free(&r->state);
    free(r);
}
const browser_route_state_t *cua_typed_browser_route_state(const cua_typed_browser_route_t *r) {
    return &r->state;
}

/* _call: inject session, run call_tool, return tool_payload */
/* PoP: _call @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute._call */
static json_t *route_call(cua_typed_browser_route_t *r, const char *name, const json_t *args) {
    json_t *payload = json_object();
    if (args && args->type == JSON_OBJECT)
        for (size_t i = 0; i < args->c.count; i++)
            json_set(payload, args->c.keys[i], br_deep_copy(args->c.items[i]));
    json_set(payload, "session", json_string(r->session_id));
    json_t *out = r->call_tool(name, payload, r->call_ctx);
    json_t *res = browser_route_tool_payload(out);
    json_free(payload);
    if (out) json_free(out);
    return res;
}

/* PoP: _require_tool @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute._require_tool */
static json_t *route_require_tool(cua_typed_browser_route_t *r, const char *name) {
    if (r->has_tool(name, r->has_ctx)) return NULL;
    return browser_route_refusal(
        "typed_browser_unavailable",
        "The connected cua-driver does not advertise %s; use the native AX/PX/foreground ladder.",
        true, NULL);
}

/* helper: get string arg */
static const char *arg_str(const json_t *args, const char *k) {
    const json_t *v = br_obj_get(args, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* PoP: observe @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute.observe */
json_t *cua_typed_browser_route_observe(cua_typed_browser_route_t *r, const json_t *args) {
    json_t *missing = route_require_tool(r, "get_browser_state");
    if (missing) return missing;

    const json_t *pid_j = br_obj_get(args, "pid");
    const json_t *wid_j = br_obj_get(args, "window_id");
    bool binding_request = (pid_j != NULL) || (wid_j != NULL);

    if (binding_request) {
        long exact_pid = browser_route_positive_int(pid_j);
        long exact_window = browser_route_positive_int(wid_j);
        browser_route_state_clear(&r->state);
        if (exact_pid < 0 || exact_window < 0) {
            return browser_route_refusal(
                "browser_exact_target_required",
                "Typed browser binding requires an exact positive pid and window_id pair.",
                true, NULL);
        }
        json_t *call_args = json_object();
        json_set(call_args, "pid", json_number((double)exact_pid));
        json_set(call_args, "window_id", json_number((double)exact_window));
        json_t *payload = route_call(r, "get_browser_state", call_args);
        json_free(call_args);
        if (strcmp(json_get_str(payload, "status", ""), "ok") != 0) {
            char *code = browser_route_refusal_code(payload);
            json_set(payload, "ok", json_bool(false));
            json_set(payload, "native_fallback_available", json_bool(true));
            if (code && strcmp(code, "browser_requires_setup") == 0)
                json_set(payload, "setup_required", json_bool(true));
            free(code);
            return payload;
        }
        const char *target_id = json_get_str(payload, "target_id", NULL);
        const char *quality = json_get_str(payload, "binding_quality", NULL);
        bool mutation_allowed = json_get_num(payload, "mutation_allowed", 0) != 0;
        if (!target_id || !*target_id) {
            return browser_route_refusal(
                "browser_binding_unproven",
                "Browser bind returned no opaque target capability; use native control.",
                true, NULL);
        }
        r->state.pid = exact_pid;
        r->state.window_id = exact_window;
        set_str(&r->state.target_id, target_id);
        json_t *tabs = browser_route_tab_ids(payload);
        /* flatten tab_ids array into state */
        for (size_t i = 0; i < r->state.n_tab_ids; i++) free(r->state.tab_ids[i]);
        free(r->state.tab_ids);
        r->state.n_tab_ids = json_len(tabs);
        r->state.tab_ids = calloc(r->state.n_tab_ids ? r->state.n_tab_ids : 1, sizeof(char *));
        for (size_t i = 0; i < r->state.n_tab_ids; i++)
            r->state.tab_ids[i] = strdup(json_get_str(json_get(tabs, i), "", ""));
        json_free(tabs);
        set_str(&r->state.binding_quality, quality ? quality : NULL);
        r->state.mutation_allowed = mutation_allowed;
        r->state.verification_required = true;
        json_set(payload, "exact_binding", json_bool(quality && strcmp(quality, "exact") == 0));
        if (!quality || strcmp(quality, "exact") != 0 || !mutation_allowed)
            json_set(payload, "native_fallback_required", json_bool(true));
        return payload;
    }

    const char *target_id = r->state.target_id;
    if (!target_id || r->state.binding_quality == NULL ||
        strcmp(r->state.binding_quality, "exact") != 0) {
        return browser_route_refusal(
            "browser_exact_binding_required",
            "Bind the exact native pid/window_id before reading a browser tab.",
            true, NULL);
    }
    const char *tab_id_arg = arg_str(args, "tab_id");
    const char *selected_tab = tab_id_arg ? tab_id_arg : r->state.tab_id;
    if (!selected_tab || !*selected_tab) {
        return browser_route_refusal(
            "browser_tab_required", "Choose an opaque tab_id returned by the exact bind.", false, NULL);
    }
    bool tab_bound = false;
    for (size_t i = 0; i < r->state.n_tab_ids; i++)
        if (strcmp(r->state.tab_ids[i], selected_tab) == 0) { tab_bound = true; break; }
    if (!tab_bound) {
        return browser_route_refusal(
            "browser_tab_unbound",
            "The requested tab_id was not minted by this session's exact bind.", false, NULL);
    }
    const char *continuation = arg_str(args, "continuation");
    if (continuation && (r->state.continuation == NULL ||
        strcmp(continuation, r->state.continuation) != 0)) {
        return browser_route_refusal(
            "browser_continuation_stale",
            "The continuation is not current for this session/tab; take a fresh snapshot.", false, NULL);
    }
    const char *scope_ref = arg_str(args, "scope_ref");
    if (scope_ref) {
        bool found = false;
        for (size_t i = 0; i < r->state.n_refs; i++)
            if (strcmp(r->state.refs[i].ref, scope_ref) == 0) { found = true; break; }
        if (!found)
            return browser_route_refusal(
                "browser_ref_stale",
                "scope_ref must come from this session's latest browser snapshot.", false, NULL);
    }

    json_t *call_args = json_object();
    json_set(call_args, "target_id", json_string(target_id));
    json_set(call_args, "tab_id", json_string(selected_tab));
    const char *fmt = arg_str(args, "snapshot_format");
    json_set(call_args, "snapshot_format", json_string(fmt ? fmt : "semantic_v2"));
    const char *query = arg_str(args, "query");
    if (query) json_set(call_args, "query", json_string(query));
    if (scope_ref) json_set(call_args, "scope_ref", json_string(scope_ref));
    if (continuation) json_set(call_args, "continuation", json_string(continuation));

    bool continuing = continuation != NULL;
    if (!continuing) browser_route_state_clear_refs(&r->state);
    json_t *payload = route_call(r, "get_browser_state", call_args);
    json_free(call_args);
    const json_t *is_err = br_obj_get(payload, "isError");
    bool bad = (br_obj_get(payload, "status") != NULL &&
                strcmp(json_get_str(payload, "status", ""), "ok") != 0) ||
               (is_err && is_err->type == JSON_BOOL && is_err->bool_val);
    if (bad) {
        browser_route_state_clear_refs(&r->state);
        r->state.verification_required = true;
        json_set(payload, "ok", json_bool(false));
        return payload;
    }

    json_t *discovered = browser_route_ref_map(payload);
    if (continuing) {
        /* merge: existing refs keep their position, discovered refs
         * overwrite same-key entries and append new ones (dict.update). */
        for (size_t i = 0; i < discovered->c.count; i++) {
            const char *ref = discovered->c.keys[i];
            const json_t *acts = discovered->c.items[i];
            size_t j;
            for (j = 0; j < r->state.n_refs; j++)
                if (strcmp(r->state.refs[j].ref, ref) == 0) break;
            if (j < r->state.n_refs) {
                json_free(r->state.refs[j].actions);
                r->state.refs[j].actions = br_deep_copy(acts);
            } else {
                r->state.refs = realloc(r->state.refs,
                    (r->state.n_refs + 1) * sizeof(browser_ref_t));
                r->state.refs[r->state.n_refs].ref = strdup(ref);
                r->state.refs[r->state.n_refs].actions = br_deep_copy(acts);
                r->state.n_refs++;
            }
        }
    } else {
        browser_route_state_clear_refs(&r->state);
        r->state.n_refs = json_len(discovered);
        r->state.refs = calloc(r->state.n_refs ? r->state.n_refs : 1, sizeof(browser_ref_t));
        size_t idx = 0;
        for (size_t i = 0; i < discovered->c.count; i++) {
            const char *ref = discovered->c.keys[i];
            const json_t *acts = discovered->c.items[i];
            r->state.refs[idx].ref = strdup(ref);
            r->state.refs[idx].actions = br_deep_copy(acts);
            idx++;
        }
    }
    json_free(discovered);
    free(r->state.continuation);
    r->state.continuation = browser_route_continuation(payload);
    set_str(&r->state.tab_id, selected_tab);
    r->state.verification_required = false;
    json_set(payload, "fresh_state", json_bool(true));
    json_set(payload, "refs_current", json_number((double)r->state.n_refs));
    return payload;
}

/* PoP: prepare @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute.prepare */
json_t *cua_typed_browser_route_prepare(cua_typed_browser_route_t *r, const json_t *args) {
    json_t *missing = route_require_tool(r, "browser_prepare");
    if (missing) return missing;
    const json_t *pid_j = br_obj_get(args, "pid");
    long exact_pid = browser_route_positive_int(pid_j);
    if (exact_pid < 0)
        return browser_route_refusal("browser_pid_required", "browser_prepare requires a positive pid.", false, NULL);
    const char *profile_mode = arg_str(args, "profile_mode");
    const char *profile_name = arg_str(args, "profile_name");
    bool allow_launch = json_get_num(args, "allow_launch", 0) != 0;

    if (profile_mode && strcmp(profile_mode, "existing_profile") == 0) {
        long exact_window = browser_route_positive_int(br_obj_get(args, "window_id"));
        if (exact_window < 0)
            return browser_route_refusal(
                "browser_exact_target_required",
                "Existing-profile attachment requires an exact positive pid and window_id pair.", false, NULL);
        browser_route_state_clear(&r->state);
        json_t *call_args = json_object();
        json_set(call_args, "pid", json_number((double)exact_pid));
        json_set(call_args, "window_id", json_number((double)exact_window));
        json_t *strat = json_object();
        json_set(strat, "kind", json_string("existing_profile"));
        json_set(call_args, "strategy", strat);
        json_t *payload = route_call(r, "browser_prepare", call_args);
        json_free(call_args); json_free(strat);
        return payload;
    }
    if (!profile_mode || (strcmp(profile_mode, "isolated_new") != 0 &&
                          strcmp(profile_mode, "isolated_named") != 0)) {
        return browser_route_refusal(
            "browser_profile_mode_invalid",
            "Use isolated_new, isolated_named, or existing_profile.", false, NULL);
    }
    if (!allow_launch)
        return browser_route_refusal(
            "browser_launch_not_approved",
            "Driver-owned isolated setup requires explicit allow_launch=true.", false, NULL);
    json_t *profile = json_object();
    json_set(profile, "mode", json_string(profile_mode));
    if (strcmp(profile_mode, "isolated_named") == 0) {
        if (!profile_name || !*profile_name)
            return browser_route_refusal(
                "browser_profile_name_required",
                "isolated_named requires a non-empty profile name.", false, NULL);
        json_set(profile, "name", json_string(profile_name));
    }
    json_t *call_args = json_object();
    json_set(call_args, "pid", json_number((double)exact_pid));
    json_set(call_args, "allow_launch", json_bool(true));
    json_set(call_args, "profile", profile);
    long exact_window = browser_route_positive_int(br_obj_get(args, "window_id"));
    if (exact_window >= 0) json_set(call_args, "window_id", json_number((double)exact_window));
    browser_route_state_clear(&r->state);
    json_t *payload = route_call(r, "browser_prepare", call_args);
    json_free(call_args); json_free(profile);
    return payload;
}

/* PoP: _require_mutation @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute._require_mutation */
static bool route_has_ref(const browser_route_state_t *s, const char *ref) {
    for (size_t i = 0; i < s->n_refs; i++)
        if (strcmp(s->refs[i].ref, ref) == 0) return true;
    return false;
}
/* PoP: _require_ref @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute._require_ref */
static bool ref_declares_action(const browser_route_state_t *s, const char *ref,
                                const json_t *actions) {
    /* actions: json_t* array; return true if declared ∩ actions nonempty */
    for (size_t i = 0; i < s->n_refs; i++) {
        if (strcmp(s->refs[i].ref, ref) != 0) continue;
        json_t *declared = s->refs[i].actions;
        if (!declared || declared->type != JSON_ARRAY) return true; /* readable */
        if (!actions || actions->type != JSON_ARRAY) return true;
        /* intersection */
        for (size_t a = 0; a < declared->c.count; a++) {
            const json_t *di = declared->c.items[a];
            if (di->type != JSON_STRING) continue;
            for (size_t b = 0; b < actions->c.count; b++) {
                const json_t *bi = actions->c.items[b];
                if (bi->type == JSON_STRING && strcmp(di->str_val, bi->str_val) == 0)
                    return true;
            }
        }
        return false;
    }
    return false;
}

/* PoP: mutate @ tools/computer_use/browser_route.py:CuaTypedBrowserRoute.mutate */
json_t *cua_typed_browser_route_mutate(cua_typed_browser_route_t *r, const char *tool,
                                        const json_t *args) {
    json_t *call_args = json_object();
    if (args && args->type == JSON_OBJECT)
        for (size_t i = 0; i < args->c.count; i++)
            json_set(call_args, args->c.keys[i], br_deep_copy(args->c.items[i]));

    bool dialog_inspect = (strcmp(tool, "browser_dialog") == 0) &&
        arg_str(call_args, "action") != NULL &&
        strcmp(arg_str(call_args, "action"), "inspect") == 0;

    /* _require_mutation */
    json_t *missing = route_require_tool(r, tool);
    if (missing) { json_free(call_args); return missing; }
    if (!r->state.target_id || r->state.binding_quality == NULL ||
        strcmp(r->state.binding_quality, "exact") != 0 || !r->state.mutation_allowed) {
        json_free(call_args);
        return browser_route_refusal(
            "browser_mutation_unproven",
            "Typed browser mutation requires status=ok, binding_quality=exact, and mutation_allowed=true; use native control otherwise.",
            true, NULL);
    }
    const char *tab_id_arg = arg_str(call_args, "tab_id");
    const char *selected_tab = tab_id_arg ? tab_id_arg : r->state.tab_id;
    if (!selected_tab || !*selected_tab) {
        json_free(call_args);
        return browser_route_refusal("browser_tab_required", "Choose a bound tab_id first.", false, NULL);
    }
    bool tab_bound = false;
    for (size_t i = 0; i < r->state.n_tab_ids; i++)
        if (strcmp(r->state.tab_ids[i], selected_tab) == 0) { tab_bound = true; break; }
    if (!tab_bound) {
        json_free(call_args);
        return browser_route_refusal(
            "browser_tab_unbound",
            "The requested tab_id was not minted by this session's exact bind.", false, NULL);
    }
    if (r->state.verification_required && !dialog_inspect) {
        json_free(call_args);
        return browser_route_refusal(
            "browser_verification_required",
            "Take a fresh cua_browser_state snapshot before another browser mutation.", false, NULL);
    }

    const char *ref = arg_str(call_args, "ref");
    bool supports_trust_choice = (strcmp(tool, "browser_click") == 0 ||
                                  strcmp(tool, "browser_pointer") == 0);
    const char *requested_route = arg_str(call_args, "input_route");
    if (requested_route && !supports_trust_choice) {
        json_free(call_args);
        char buf[160];
        snprintf(buf, sizeof buf, "%s does not expose a trust-route choice in the live 0.9 schema.", tool);
        return browser_route_refusal("browser_input_route_unsupported", buf, false, NULL);
    }
    const char *route = requested_route ? requested_route : "trusted";
    if (strcmp(route, "trusted") != 0 && strcmp(route, "dom_event") != 0) {
        json_free(call_args);
        return browser_route_refusal(
            "browser_input_route_invalid",
            "Use input_route=trusted or explicitly request dom_event.", false, NULL);
    }
    if (strcmp(route, "dom_event") == 0 && !ref) {
        json_free(call_args);
        return browser_route_refusal(
            "browser_dom_event_ref_required",
            "The dom_event trust class requires a current semantic ref.", false, NULL);
    }

    json_t *required_actions = NULL;
    if (strcmp(tool, "browser_click") == 0 && ref) {
        required_actions = json_array();
        json_append(required_actions, json_string("click"));
        json_append(required_actions, json_string("pointer"));
    } else if (strcmp(tool, "browser_type") == 0) {
        required_actions = json_array();
        json_append(required_actions, json_string("type"));
        json_append(required_actions, json_string("edit"));
        json_append(required_actions, json_string("input"));
    } else if (strcmp(tool, "browser_pointer") == 0 && ref) {
        const char *pa = arg_str(call_args, "action");
        required_actions = json_array();
        if (pa && strcmp(pa, "scroll") == 0) json_append(required_actions, json_string("scroll"));
        json_append(required_actions, json_string("pointer"));
    } else if (strcmp(tool, "browser_set_input_files") == 0) {
        required_actions = json_array();
        json_append(required_actions, json_string("set_input_files"));
        json_append(required_actions, json_string("upload"));
        json_append(required_actions, json_string("files"));
    } else if (strcmp(tool, "browser_download") == 0) {
        required_actions = json_array();
        json_append(required_actions, json_string("download"));
        json_append(required_actions, json_string("click"));
    }

    if (required_actions) {
        if (!ref || !route_has_ref(&r->state, ref) ||
            !ref_declares_action(&r->state, ref, required_actions)) {
            json_free(required_actions); json_free(call_args);
            const char *code = (!ref || !route_has_ref(&r->state, ref))
                ? "browser_ref_stale" : "browser_action_unavailable";
            const char *msg = (!ref || !route_has_ref(&r->state, ref))
                ? "Use a current ref from the latest cua_browser_state snapshot."
                : "The current ref does not declare the requested browser action.";
            return browser_route_refusal(code, msg, false, NULL);
        }
        json_free(required_actions);
    }
    const char *destination_ref = arg_str(call_args, "destination_ref");
    if (destination_ref) {
        json_t *dest_actions = json_array();
        json_append(dest_actions, json_string("pointer"));
        json_append(dest_actions, json_string("drag"));
        json_append(dest_actions, json_string("drop"));
        if (!route_has_ref(&r->state, destination_ref) ||
            !ref_declares_action(&r->state, destination_ref, dest_actions)) {
            json_free(dest_actions); json_free(call_args);
            const char *code = !route_has_ref(&r->state, destination_ref)
                ? "browser_ref_stale" : "browser_action_unavailable";
            const char *msg = !route_has_ref(&r->state, destination_ref)
                ? "Use a current ref from the latest cua_browser_state snapshot."
                : "The current ref does not declare the requested browser action.";
            return browser_route_refusal(code, msg, false, NULL);
        }
        json_free(dest_actions);
    }

    json_set(call_args, "target_id", json_string(r->state.target_id));
    json_set(call_args, "tab_id", json_string(selected_tab));
    if (!dialog_inspect) {
        set_str(&r->state.tab_id, selected_tab);
        browser_route_state_clear_refs(&r->state);
        r->state.verification_required = true;
    }
    json_t *payload = route_call(r, tool, call_args);
    char *code = browser_route_refusal_code(payload);
    bool refused = (br_obj_get(payload, "isError") != NULL &&
                    br_obj_get(payload, "isError")->type == JSON_BOOL &&
                    br_obj_get(payload, "isError")->bool_val) ||
                   (br_obj_get(payload, "status") != NULL &&
                    strcmp(json_get_str(payload, "status", ""), "ok") != 0) ||
                   code != NULL;
    if (supports_trust_choice) {
        json_set(payload, "input_trust", json_string(route));
        if (strcmp(route, "dom_event") == 0)
            json_set(payload, "trust_downgrade_explicit", json_bool(true));
    }
    if (refused) {
        json_set(payload, "native_fallback_available", json_bool(true));
        if (dialog_inspect && code &&
            (strcmp(code, "browser_ref_stale") == 0 || strcmp(code, "browser_binding_ambiguous") == 0)) {
            browser_route_state_clear_refs(&r->state);
            r->state.verification_required = true;
        }
        if (code && strcmp(code, "browser_input_trust_unavailable") == 0) {
            json_set(payload, "trust_change_requires_explicit_choice", json_bool(true));
            json_set(payload, "native_fallback_available", json_bool(true));
        }
        free(code);
        json_free(call_args);
        return payload;
    }
    if (dialog_inspect) {
        json_set(payload, "fresh_dialog_state", json_bool(true));
        json_free(call_args);
        return payload;
    }
    json_set(payload, "verification_required", json_bool(true));
    json_set(payload, "next_step", json_string("fresh_browser_state"));
    json_free(call_args);
    return payload;
}
