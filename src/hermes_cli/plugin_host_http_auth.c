/*
 * plugin_host_http_auth.c -- real C11 port of hermes_cli/plugin_host_http_auth.py
 * (gate w22_pluginhost_948f76).
 *
 * PoP: w22_pluginhost_948f76_admitted @ hermes_cli/plugin_host_http_auth.py:admitted_for_sensitive_route
 * PoP: w22_pluginhost_948f76_scope_state @ hermes_cli/plugin_host_http_auth.py:with_hosted_admission
 *
 * A hosted plugin must see the normal guard's decision on its own IPC request,
 * never a forwarded gateway credential and never a process-wide bypass. The
 * guard is an auth seam (stubbed by the oracle with the quarry's real
 * HTTPException) and the ASGI app is a network seam; the fold and the state
 * merge below are the module's own logic.
 */
#include "hermes_json.h"
#include "noc_w22_pluginhost_948f76.h"

#include <string.h>

/* Python truthiness for a decoded JSON value; absent (NULL) reads as None. */
static int ph_948f76_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static json_t *ph_948f76_raised(const char *type, json_t *extra) {
    json_t *out = json_object();
    json_set(out, "raised", extra ? extra : json_object());
    json_set(json_obj_get(out, "raised"), "type", json_string(type ? type : "Exception"));
    return out;
}

json_t *w22_pluginhost_948f76_admitted(const json_t *guard) {
    const char *kind;
    json_t *out, *status, *detail;

    if (!guard || guard->type != JSON_OBJECT) {
        out = json_object();
        json_set(out, "admitted", json_bool(1));
        return out;
    }
    kind = json_get_str(guard, "kind", "ok");
    if (strcmp(kind, "http") && strcmp(kind, "raise")) kind = "ok";

    if (!strcmp(kind, "raise")) {
        json_t *extra = json_object();
        json_set(extra, "detail",
                 json_string(json_get_str(guard, "detail", "")));
        return ph_948f76_raised(json_get_str(guard, "type", "Exception"), extra);
    }
    if (!strcmp(kind, "http")) {
        status = json_obj_get(guard, "status");
        /* `exc.status_code != 401`: anything that is not the int 401 re-raises. */
        if (status && status->type == JSON_NUMBER && status->num_val == 401.0) {
            out = json_object();
            json_set(out, "admitted", json_bool(0));
            return out;
        }
        detail = json_object();
        json_set(detail, "status", status ? json_copy(status) : json_null());
        json_set(detail, "detail", json_string(json_get_str(guard, "detail", "Unauthorized")));
        return ph_948f76_raised("HTTPException", detail);
    }
    out = json_object();
    json_set(out, "admitted", json_bool(1));
    return out;
}

/* dict(["ab"]) -> {"a": "b"}: a two-character string element is one pair. */
static json_t *ph_948f76_pair_from_string(const json_t *item) {
    const char *s = item->str_val;
    json_t *pair;
    char key[2];
    char val[2];
    if (!s || strlen(s) != 2) return NULL;
    key[0] = s[0];
    key[1] = '\0';
    val[0] = s[1];
    val[1] = '\0';
    pair = json_object();
    json_set(pair, key, json_string(val));
    return pair;
}

/* dict(x) for the JSON shapes an ASGI scope's "state" can carry. NULL plus a
 * `raised` name when CPython would raise. */
static json_t *ph_948f76_dict_from(const json_t *v, const char **err) {
    json_t *out;
    size_t i;

    if (!ph_948f76_truthy(v)) return json_object();   /* `x or {}` */
    switch (v->type) {
    case JSON_OBJECT:
        return json_copy(v);
    case JSON_ARRAY:
        out = json_object();
        for (i = 0; i < v->c.count; i++) {
            json_t *item = v->c.items[i], *pair = NULL;
            if (item && item->type == JSON_ARRAY) {
                if (item->c.count != 2) { *err = "ValueError"; json_free(out); return NULL; }
                if (!item->c.items[0] || item->c.items[0]->type != JSON_STRING) {
                    *err = "TypeError";                   /* unhashable/non-string key */
                    json_free(out);
                    return NULL;
                }
                pair = json_object();
                json_set(pair, item->c.items[0]->str_val,
                         item->c.items[1] ? json_copy(item->c.items[1]) : json_null());
            } else if (item && item->type == JSON_STRING) {
                pair = ph_948f76_pair_from_string(item);
                if (!pair) { *err = "ValueError"; json_free(out); return NULL; }
            } else {
                *err = "TypeError";                       /* not a sequence */
                json_free(out);
                return NULL;
            }
            for (size_t k = 0; k < json_object_size(pair); k++) {
                json_set(out, json_object_get_key_at(pair, k),
                         json_copy(json_object_get_at(pair, k)));
            }
            json_free(pair);
        }
        return out;
    case JSON_STRING:
        /* Every character is a length-1 sequence, so dict("ab") raises. */
        *err = "ValueError";
        return NULL;
    case JSON_NUMBER:
    case JSON_BOOL:
        *err = "TypeError";
        return NULL;
    case JSON_NULL:
        return json_object();
    }
    return json_object();
}

json_t *w22_pluginhost_948f76_scope_state(const json_t *scope, const json_t *authenticated) {
    const char *err = NULL;
    json_t *state, *merged, *out;
    size_t i;

    if (!scope || scope->type != JSON_OBJECT) {
        return ph_948f76_raised("AttributeError", NULL);   /* scope.get on a non-mapping */
    }
    state = ph_948f76_dict_from(json_obj_get(scope, "state"), &err);
    if (!state) return ph_948f76_raised(err ? err : "TypeError", NULL);

    /* `authenticated is True` -- identity, not equality. */
    json_set(state, "hosted_http_authenticated",
             json_bool(authenticated && authenticated->type == JSON_BOOL &&
                       authenticated->bool_val));

    merged = json_object();
    for (i = 0; i < json_object_size(scope); i++) {
        const char *key = json_object_get_key_at(scope, i);
        if (!key || !strcmp(key, "state")) continue;
        json_set(merged, key, json_copy(json_object_get_at(scope, i)));
    }
    json_set(merged, "state", state);

    out = json_object();
    json_set(out, "scope", merged);
    return out;
}
