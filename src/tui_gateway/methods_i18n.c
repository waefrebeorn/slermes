/*
 * methods_i18n.c — C home for upstream Hermes tui_gateway/methods_i18n.py.
 *
 * PoP: w11_methodsi18_f41712_languages @ tui_gateway/methods_i18n.py:_
 * PoP: w11_methodsi18_f41712_catalog  @ tui_gateway/methods_i18n.py:_
 * PoP: w11_methodsi18_f41712_i18n_err @ tui_gateway/methods_i18n.py:_I18N_ERR
 *
 * The two @method("i18n.*") handlers are ported verbatim: str() coercion of
 * params["surface"], membership check against SURFACES, resolve_language_id
 * or "en" fallback, and the _ok/_err response shapes. External collaborators
 * (agent.i18n, agent.i18n_layers, discover_plugins) are injected by the
 * harness as seams, exactly as the oracle stubs them on the live module.
 */
#include "noc_w11_methodsi18_f41712.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

const char *w11_methodsi18_f41712_i18n_err(void) { return "5400"; }

static char *mi_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static json_t *mi_string(const char *s) { return json_string(s ? s : ""); }

/* Python truthiness for the scalar values a surface parameter can take. */
static int mi_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 1;
}

/* str(v) for scalar v (Python: True -> "True", 5.0 -> "5.0"). */
static char *mi_py_str(const json_t *v) {
    char buf[64];
    if (!v) return mi_dup("");
    switch (v->type) {
        case JSON_STRING: return mi_dup(v->str_val ? v->str_val : "");
        case JSON_BOOL:   return mi_dup(v->bool_val ? "True" : "False");
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(buf, sizeof(buf), "%.0f.0", v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return mi_dup(buf);
        default: return mi_dup("");
    }
}

static int mi_in_surfaces(json_t *surfaces, const char *surface) {
    if (!surfaces || surfaces->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(surfaces); i++) {
        json_t *v = json_get(surfaces, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, surface))
            return 1;
    }
    return 0;
}

/* f"surface must be one of {', '.join(SURFACES)}" */
static char *mi_surface_error(json_t *surfaces) {
    size_t cap = 64, len = 0;
    for (size_t i = 0; i < (surfaces && surfaces->type == JSON_ARRAY ? json_len(surfaces) : 0); i++) {
        json_t *v = json_get(surfaces, i);
        cap += v && v->str_val ? strlen(v->str_val) + 2 : 4;
    }
    char *out = (char *)malloc(cap);
    if (!out) return mi_dup("");
    out[0] = '\0';
    const char *prefix = "surface must be one of ";
    strcpy(out, prefix);
    len = strlen(prefix);
    int first = 1;
    for (size_t i = 0; i < (surfaces && surfaces->type == JSON_ARRAY ? json_len(surfaces) : 0); i++) {
        json_t *v = json_get(surfaces, i);
        if (!first) { out[len++] = ','; out[len++] = ' '; }
        const char *s = (v && v->str_val) ? v->str_val : "";
        size_t n = strlen(s);
        memcpy(out + len, s, n);
        len += n;
        first = 0;
    }
    out[len] = '\0';
    return out;
}

static json_t *mi_ok(const char *rid, json_t *payload) {
    json_t *o = json_object();
    json_set(o, "kind", json_string("ok"));
    json_set(o, "rid", mi_string(rid));
    json_set(o, "payload", payload);
    return o;
}

static json_t *mi_err(const char *rid, int code, const char *msg) {
    json_t *o = json_object();
    json_set(o, "kind", json_string("err"));
    json_set(o, "rid", mi_string(rid));
    json_set(o, "code", json_number((double)code));
    json_set(o, "message", mi_string(msg));
    return o;
}

json_t *w11_methodsi18_f41712_languages(const char *rid, json_t *languages,
                                         const char *fail_message) {
    (void)w11_methodsi18_f41712_i18n_err;
    if (fail_message && *fail_message)
        return mi_err(rid, 5400, fail_message);
    json_t *payload = json_object();
    json_set(payload, "languages", languages ? json_copy(languages) : json_array());
    return mi_ok(rid, payload);
}

json_t *w11_methodsi18_f41712_catalog(const char *rid, json_t *params,
                                       json_t *surfaces, const char *core_surface,
                                       const char *resolve_result, json_t *messages,
                                       const char *fail_message) {
    /* surface = str(params.get("surface") or CORE_SURFACE) */
    json_t *raw_surface = params ? json_obj_get(params, "surface") : NULL;
    char *surface = mi_truthy(raw_surface) ? mi_py_str(raw_surface)
                                          : mi_dup(core_surface ? core_surface : "");
    if (!mi_in_surfaces(surfaces, surface)) {
        char *msg = mi_surface_error(surfaces);
        json_t *r = mi_err(rid, 4002, msg);
        free(msg); free(surface);
        return r;
    }
    if (fail_message && *fail_message) {
        json_t *r = mi_err(rid, 5400, fail_message);
        free(surface);
        return r;
    }
    /* lang = resolve_language_id(params.get("lang", "")) or "en" */
    const char *lang = (resolve_result && *resolve_result) ? resolve_result : "en";
    json_t *payload = json_object();
    json_set(payload, "lang", mi_string(lang));
    json_set(payload, "surface", mi_string(surface));
    json_set(payload, "messages", messages ? json_copy(messages) : json_null());
    free(surface);
    return mi_ok(rid, payload);
}
