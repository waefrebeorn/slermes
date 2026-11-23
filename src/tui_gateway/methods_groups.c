/*
 * methods_groups.c — C port of upstream Hermes tui_gateway/methods_groups.py.
 *
 * PoP: w14_methodsgro_0e3f84_grant_expiry @ tui_gateway/methods_groups.py:_grant_expiry
 * PoP: w14_methodsgro_0e3f84_include_disbanded @ tui_gateway/methods_groups.py:_include_disbanded
 * PoP: w14_methodsgro_0e3f84_methods @ tui_gateway/methods_groups.py:_METHODS
 * PoP: w14_methodsgro_0e3f84_long_handlers @ tui_gateway/methods_groups.py:LONG_HANDLERS
 */
#include "noc_w14_methodsgro_0e3f84.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const _METHODS[] = {
    "groups.capabilities",
    "groups.list",
    "groups.create",
    "groups.state",
    "groups.send",
    "groups.rename",
    "groups.context",
    "groups.log",
    "groups.disband",
    "groups.replicate",
    "groups.replica_state",
    "groups.promote",
    "groups.demote",
    "groups.stop",
    "groups.continue",
    "groups.retry",
    "groups.approve",
    "groups.peer.invite",
    "groups.peer.revoke",
    "groups.peer.register",
    "groups.attachment.upload",
    "groups.attachment.download"
};

/* Python float(v): numeric JSON scalars (int/number/bool/numeric string). */
static int py_float(const struct json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end && *end == '\0') { *out = d; return 1; }
    }
    return 0;
}

/* float(claims.get("status_expires_at", claims["expires_at"])) */
struct json_t *w14_methodsgro_0e3f84_grant_expiry(const struct json_t *claims) {
    if (!claims || claims->type != JSON_OBJECT) return json_null();
    const struct json_t *v = json_obj_get(claims, "status_expires_at");
    if (!v) v = json_obj_get(claims, "expires_at");
    double d = 0.0;
    if (!py_float(v, &d)) return json_null();
    return json_number(d);
}

/* params.get("include_disbanded") is True -- identity check on JSON bool true. */
struct json_t *w14_methodsgro_0e3f84_include_disbanded(const struct json_t *params) {
    const struct json_t *v = (params && params->type == JSON_OBJECT)
                             ? json_obj_get(params, "include_disbanded") : NULL;
    return json_bool(v && v->type == JSON_BOOL && v->bool_val);
}

struct json_t *w14_methodsgro_0e3f84_methods(void) {
    struct json_t *a = json_array();
    for (size_t i = 0; i < sizeof(_METHODS)/sizeof(_METHODS[0]); ++i)
        json_append(a, json_string(_METHODS[i]));
    return a;
}

/* LONG_HANDLERS = frozenset(_METHODS) | {"groups.discard"}; sorted for determinism. */
struct json_t *w14_methodsgro_0e3f84_long_handlers(void) {
    const char *all[24];
    size_t n = 0;
    for (size_t i = 0; i < sizeof(_METHODS)/sizeof(_METHODS[0]); ++i) all[n++] = _METHODS[i];
    all[n++] = "groups.discard";
    int used[24] = {0};
    struct json_t *a = json_array();
    for (size_t k = 0; k < n; ++k) {
        size_t best = n;
        for (size_t i = 0; i < n; ++i)
            if (!used[i] && (best == n || strcmp(all[i], all[best]) < 0)) best = i;
        used[best] = 1;
        json_append(a, json_string(all[best]));
    }
    return a;
}
