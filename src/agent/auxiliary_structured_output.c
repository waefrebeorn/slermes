/*
 * auxiliary_structured_output.c - C11 port of upstream
 * agent/auxiliary_structured_output.py (gate w26_auxiliarys_818f2b).
 *
 * PoP: w26_auxiliarys_818f2b_route_key @ agent/auxiliary_structured_output.py:_route_key
 * PoP: w26_auxiliarys_818f2b_format_type @ agent/auxiliary_structured_output.py:_response_format_type
 * PoP: w26_auxiliarys_818f2b_capability @ agent/auxiliary_structured_output.py:is_capability_rejection
 * PoP: w26_auxiliarys_818f2b_without_format @ agent/auxiliary_structured_output.py:without_unsupported_response_format
 * PoP: w26_auxiliarys_818f2b_strip_format @ agent/auxiliary_structured_output.py:_without_structured_output_format
 * PoP: w26_auxiliarys_818f2b_remember @ agent/auxiliary_structured_output.py:remember_structured_output_rejection
 * PoP: w26_auxiliarys_818f2b_constants @ agent/auxiliary_structured_output.py:_CAPABILITY_REJECTION_MARKERS
 *
 * The provider-profile registry and the process-level memo are seams: the
 * profile's declared types arrive as the ``unsupported`` input and the memo as
 * the ``rejected`` list.  What is ported is the part that is a pure function of
 * values -- which route an endpoint keys to, which format type a request would
 * carry, whether a rejection is about capability, and what gets dropped.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "noc_w26_auxiliarys_818f2b.h"

/* _CAPABILITY_REJECTION_MARKERS, in source order. */
static const char *const W26_AUX_MARKERS[] = {
    "unavailable",
    "does not support",
    "doesn't support",
    "not supported",
    "unsupported parameter",
    "unsupported_parameter",
    "unknown parameter",
    "unrecognized request argument",
    "unrecognized parameter",
    "extra inputs are not permitted",
};
#define W26_AUX_MARKER_N (sizeof(W26_AUX_MARKERS) / sizeof(*W26_AUX_MARKERS))

static int aux_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* Python '==' over the scalars a format type can be (absent == None,
 * True == 1, 1 == 1.0, strings by bytes). */
static int aux_py_eq(const json_t *a, const json_t *b) {
    int an = !a || a->type == JSON_NULL, bn = !b || b->type == JSON_NULL;
    if (an || bn) return an && bn;
    if (a->type == JSON_STRING || b->type == JSON_STRING) {
        return a->type == JSON_STRING && b->type == JSON_STRING
               && !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    }
    if ((a->type == JSON_NUMBER || a->type == JSON_BOOL)
        && (b->type == JSON_NUMBER || b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    return 0;
}

/* str(v) for the scalars here (provider/model names). */
static char *aux_py_str(const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
    case JSON_BOOL:  return strdup(v->bool_val ? "True" : "False");
    case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
    case JSON_NUMBER: {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e16) snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        else snprintf(tmp, sizeof(tmp), "%.15g", d);
        return strdup(tmp);
    }
    default: {
        char *s = json_dumps(v, 0);
        return s ? s : strdup("None");
    }
    }
}

static int aux_space_at(const char *s, size_t *len) {
    unsigned char c = (unsigned char)s[0];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') { *len = 1; return 1; }
    if (c == 0xC2 && (unsigned char)s[1] == 0xA0) { *len = 2; return 1; }
    return 0;
}

static char *aux_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    size_t l = 0, n;
    char *out;
    while (*a && aux_space_at(a, &l)) a += l;
    b = a + strlen(a);
    while (b > a) {
        const char *p = b - 1;
        while (p > a && ((unsigned char)*p & 0xC0) == 0x80) p--;
        if (aux_space_at(p, &l) && p + l == b) b = p;
        else break;
    }
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

static char *aux_lower(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

/* urlparse(url).netloc: after "://" (or a leading "//") up to "/?#". */
static char *aux_netloc(const char *url) {
    const char *p = url ? url : "";
    const char *q = strstr(p, "://");
    size_t n;
    char *out;
    if (q) p = q + 3;
    else if (p[0] == '/' && p[1] == '/') p += 2;
    else return strdup("");
    q = p;
    while (*q && *q != '/' && *q != '?' && *q != '#') q++;
    n = (size_t)(q - p);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

static const json_t *aux_get(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT ? json_obj_get(obj, key) : NULL;
}

json_t *w26_auxiliarys_818f2b_route_key(const json_t *req) {
    const json_t *base_v = aux_get(req, "base_url");
    const json_t *prov_v = aux_get(req, "provider");
    char *netloc, *lower, *out_s, *prov, *stripped;
    json_t *out;

    netloc = aux_netloc(base_v && base_v->type == JSON_STRING ? base_v->str_val : "");
    lower = aux_lower(netloc);
    out_s = strdup(lower);
    free(lower);
    if (!out_s[0]) {
        /* str(provider or "").strip().lower() */
        prov = prov_v && aux_truthy(prov_v) ? aux_py_str(prov_v) : strdup("");
        stripped = aux_strip(prov);
        free(prov);
        lower = aux_lower(stripped);
        free(stripped);
        free(out_s);
        out_s = lower;
    }
    free(netloc);
    out = json_string(out_s);
    free(out_s);
    return out;
}

/* _response_format_type(request_kwargs) */
static json_t *aux_format_type_of(const json_t *kwargs) {
    const json_t *extra = aux_get(kwargs, "extra_body");
    const json_t *rf = NULL;
    if (extra && extra->type == JSON_OBJECT) rf = json_obj_get(extra, "response_format");
    if (!rf || rf->type == JSON_NULL) rf = aux_get(kwargs, "response_format");
    if (rf && rf->type == JSON_OBJECT) {
        const json_t *t = json_obj_get(rf, "type");
        return t ? json_copy(t) : json_null();
    }
    return json_null();
}

json_t *w26_auxiliarys_818f2b_format_type(const json_t *kwargs) {
    return aux_format_type_of(kwargs);
}

json_t *w26_auxiliarys_818f2b_capability(const json_t *error) {
    char *text, *lower;
    size_t i;
    int found = 0;

    if (error && error->type == JSON_STRING) text = strdup(error->str_val ? error->str_val : "");
    else if (aux_truthy(error)) text = aux_py_str(error);
    else text = strdup("");
    lower = aux_lower(text);
    free(text);
    if (strstr(lower, "invalid schema")) { free(lower); return json_bool(0); }
    for (i = 0; i < W26_AUX_MARKER_N; i++) {
        if (strstr(lower, W26_AUX_MARKERS[i])) { found = 1; break; }
    }
    free(lower);
    return json_bool(found ? 1 : 0);
}

static int aux_type_in(const json_t *format_type, const json_t *unsupported) {
    size_t i, n = unsupported && unsupported->type == JSON_ARRAY ? json_len(unsupported) : 0;
    for (i = 0; i < n; i++)
        if (aux_py_eq(format_type, json_get(unsupported, i))) return 1;
    return 0;
}

static int aux_memo_has(const json_t *rejected, const char *route, const char *model, const json_t *type) {
    size_t i, n = rejected && rejected->type == JSON_ARRAY ? json_len(rejected) : 0;
    for (i = 0; i < n; i++) {
        const json_t *row = json_get(rejected, i);
        const json_t *r, *m, *t;
        if (!row || row->type != JSON_ARRAY || json_len(row) < 3) continue;
        r = json_get(row, 0);
        m = json_get(row, 1);
        t = json_get(row, 2);
        if (!r || r->type != JSON_STRING || strcmp(r->str_val ? r->str_val : "", route)) continue;
        if (!m || m->type != JSON_STRING || strcmp(m->str_val ? m->str_val : "", model)) continue;
        if (aux_py_eq(t, type)) return 1;
    }
    return 0;
}

json_t *w26_auxiliarys_818f2b_without_format(const json_t *req) {
    const json_t *extra = aux_get(req, "extra_body");
    const json_t *unsupported = aux_get(req, "unsupported");
    const json_t *rejected = aux_get(req, "rejected");
    char *route, *model;
    json_t *out, *route_v;

    if (!extra || extra->type != JSON_OBJECT) extra = NULL;
    route_v = w26_auxiliarys_818f2b_route_key(req);
    route = strdup(route_v && route_v->str_val ? route_v->str_val : "");
    json_free(route_v);
    /* str(model or "") */
    model = aux_get(req, "model") && aux_truthy(aux_get(req, "model"))
                ? aux_py_str(aux_get(req, "model")) : strdup("");
    if (!extra) {
        out = json_object();
    } else {
        /* _response_format_type({"extra_body": extra_body}) */
        const json_t *rf = json_obj_get(extra, "response_format");
        json_t *local = NULL;
        int remove;
        if (rf && rf->type == JSON_OBJECT) {
            const json_t *t = json_obj_get(rf, "type");
            local = t ? json_copy(t) : json_null();
        } else {
            local = json_null();
        }
        remove = aux_truthy(local)
                 && (aux_type_in(local, unsupported) || aux_memo_has(rejected, route, model, local));
        if (!remove) {
            out = json_copy(extra);
        } else {
            size_t i, n = json_object_size(extra);
            out = json_object();
            for (i = 0; i < n; i++) {
                const char *k = json_object_get_key_at(extra, i);
                if (!k || !strcmp(k, "response_format")) continue;
                json_set(out, k, json_copy(json_object_get_at(extra, i)));
            }
        }
        json_free(local);
    }
    free(route);
    free(model);
    return out;
}

json_t *w26_auxiliarys_818f2b_strip_format(const json_t *kwargs) {
    json_t *out;
    const json_t *rf, *extra;
    int changed = 0;

    out = kwargs && kwargs->type == JSON_OBJECT ? json_copy(kwargs) : json_object();
    rf = json_obj_get(out, "response_format");
    if (rf && rf->type != JSON_NULL) changed = 1;
    json_obj_del(out, "response_format");
    extra = json_obj_get(out, "extra_body");
    if (extra && extra->type == JSON_OBJECT && json_obj_get(extra, "response_format")) {
        json_t *remaining = json_object();
        size_t i, n = json_object_size(extra);
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (!k || !strcmp(k, "response_format")) continue;
            json_set(remaining, k, json_copy(json_object_get_at(extra, i)));
        }
        if (json_object_size(remaining) > 0) json_set(out, "extra_body", remaining);
        else { json_free(remaining); json_obj_del(out, "extra_body"); }
        changed = 1;
    }
    if (!changed) { json_free(out); return json_null(); }
    return out;
}

json_t *w26_auxiliarys_818f2b_remember(const json_t *req) {
    const json_t *kwargs = aux_get(req, "kwargs");
    json_t *format_type = aux_format_type_of(kwargs);
    json_t *capable = w26_auxiliarys_818f2b_capability(aux_get(req, "error"));
    json_t *out = NULL;

    if (aux_truthy(format_type) && aux_truthy(capable)) {
        json_t *route_v = w26_auxiliarys_818f2b_route_key(req);
        char *model = aux_get(kwargs, "model") && aux_truthy(aux_get(kwargs, "model"))
                          ? aux_py_str(aux_get(kwargs, "model")) : strdup("");
        out = json_array();
        json_append(out, json_copy(route_v));
        json_append(out, json_string(model));
        json_append(out, json_copy(format_type));
        free(model);
        json_free(route_v);
    }
    json_free(format_type);
    json_free(capable);
    return out ? out : json_null();
}

json_t *w26_auxiliarys_818f2b_constants(void) {
    json_t *o = json_object(), *markers = json_array();
    size_t i;
    for (i = 0; i < W26_AUX_MARKER_N; i++) json_append(markers, json_string(W26_AUX_MARKERS[i]));
    json_set(o, "capability_rejection_markers", markers);
    return o;
}
