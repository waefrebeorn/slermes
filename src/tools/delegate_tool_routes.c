/*
 * delegate_tool_routes.c - C home for upstream Hermes tools/delegate_tool_routes.py.
 * PoP: w14_delegateto_56c3a1_param_error @ tools/delegate_tool_routes.py:_param_error
 * PoP: w14_delegateto_56c3a1_clean_route @ tools/delegate_tool_routes.py:_clean_route
 * PoP: w14_delegateto_56c3a1_with_inherited_fallback @ tools/delegate_tool_routes.py:_with_inherited_fallback
 * PoP: w14_delegateto_56c3a1_route_report @ tools/delegate_tool_routes.py:TaskRoute.report
 * PoP: w14_delegateto_56c3a1_route_refused_entry @ tools/delegate_tool_routes.py:TaskRoute.refused_entry
 *
 * Pure route-shaping helpers from the per-task delegation provider contract.
 * Provider lookup, credential resolution and profile scopes are injected as
 * JSON; only deterministic validation/projection lives here.
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegateto_56c3a1.h"

static char *dupstr(const char *s) { size_t n = strlen(s) + 1; char *p = (char *)malloc(n); if (p) memcpy(p, s, n); return p; }
static char *fmti(long long v) { char b[48]; snprintf(b, sizeof(b), "%lld", v); return dupstr(b); }
static char *fmtg(double v) { char b[64]; snprintf(b, sizeof(b), "%g", v); return dupstr(b); }

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, char c) {
    if (b->n + 1 >= b->cap) { size_t nc = b->cap ? b->cap * 2 : 64; char *np = (char *)realloc(b->p, nc); if (!np) return; b->p = np; b->cap = nc; }
    b->p[b->n++] = c;
}
static void bputs(buf_t *b, const char *s) { for (const char *p = s; *p; p++) bput(b, *p); }
static char *bdone(buf_t *b) { if (!b->p) b->p = (char *)calloc(1, 1); if (b->n) b->p[b->n] = 0; return b->p; }

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count > 0;
    return 0;
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("");
}
/* Python repr() of a str (quote choice + escapes), enough for the error text. */
static char *py_repr_str(const char *s) {
    int has_s = strchr(s, '\'') != NULL, has_d = strchr(s, '"') != NULL;
    char q = (has_s && !has_d) ? '"' : '\'';
    buf_t b = {0}; bput(&b, q);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == '\\' || c == (unsigned char)q) { bput(&b, '\\'); bput(&b, (char)c); }
        else if (c == '\n') { bput(&b, '\\'); bput(&b, 'n'); }
        else if (c == '\r') { bput(&b, '\\'); bput(&b, 'r'); }
        else if (c == '\t') { bput(&b, '\\'); bput(&b, 't'); }
        else if (c < 0x20 || c == 0x7f) {
            static const char hx[] = "0123456789abcdef";
            bput(&b, '\\'); bput(&b, 'x'); bput(&b, hx[c >> 4]); bput(&b, hx[c & 15]);
        } else bput(&b, (char)c);
    }
    bput(&b, q); return bdone(&b);
}
static char *py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("None");
    if (v->type == JSON_STRING) return py_repr_str(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val ? fmti((long long)v->num_val) : fmtg(v->num_val);
    char *s = json_dumps(v, 0); return s ? s : dupstr("None");
}
static int json_eq(const json_t *a, const json_t *b) {
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
        default: { char *x = json_dumps(a, 0), *y = json_dumps(b, 0); int r = x && y && !strcmp(x, y); free(x); free(y); return r; }
    }
}
static int json_in(const json_t *arr, const json_t *v) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) if (json_eq(json_get(arr, i), v)) return 1;
    return 0;
}
static char *join_enum(const json_t *arr) {
    buf_t b = {0};
    for (size_t i = 0; i < json_len(arr); i++) {
        if (i) bputs(&b, ", ");
        char *s = py_str(json_get(arr, i)); bputs(&b, s); free(s);
    }
    return bdone(&b);
}

struct json_t *w14_delegateto_56c3a1_param_error(long long index, const char *name,
                                                 const struct json_t *value, const struct json_t *schema) {
    json_t *en = schema ? json_obj_get(schema, "enum") : NULL;
    if (en && en->type == JSON_ARRAY && !json_in(en, value)) {
        char *joined = join_enum(en), *vr = py_repr(value);
        size_t n = strlen(joined) + strlen(vr) + strlen(name) + 96;
        char *msg = (char *)malloc(n);
        snprintf(msg, n, "Task %lld: '%s' must be one of %s (got %s).", index, name, joined, vr);
        json_t *r = json_string(msg);
        free(msg); free(joined); free(vr);
        return r;
    }
    return json_null();
}

struct json_t *w14_delegateto_56c3a1_clean_route(const char *provider_name, const struct json_t *route) {
    if (!route || route->type != JSON_OBJECT) {
        json_t *e = json_object();
        json_set(e, "__raise__", json_string("DelegationRouteError"));
        size_t n = strlen(provider_name) + 64;
        char *msg = (char *)malloc(n);
        snprintf(msg, n, "delegation backend '%s' returned no route.", provider_name);
        json_set(e, "msg", json_string(msg));
        free(msg);
        return e;
    }
    static const char *KEYS[] = {"model", "provider", "base_url", "api_key", "api_mode", "request_overrides",
                                 "reasoning_effort", "service_tier", "fallback_providers", "profile", "label", NULL};
    json_t *r = json_object();
    for (size_t i = 0; i < json_object_size(route); i++) {
        const char *k = json_object_get_key_at(route, i);
        if (!k) continue;
        for (int j = 0; KEYS[j]; j++) if (!strcmp(k, KEYS[j])) {
            json_set(r, k, json_copy(json_object_get_at(route, i)));
            break;
        }
    }
    return r;
}

struct json_t *w14_delegateto_56c3a1_with_inherited_fallback(const struct json_t *route, const struct json_t *config) {
    json_t *fb = json_obj_get(route, "fallback_providers");
    if (truthy(json_obj_get(route, "profile")) || (fb && fb->type != JSON_NULL)) return json_copy(route);
    json_t *inh = json_obj_get(config, "fallback_providers");
    if (!inh || inh->type == JSON_NULL) return json_copy(route);
    json_t *r = json_copy(route);
    json_set(r, "fallback_providers", json_copy(inh));
    return r;
}

struct json_t *w14_delegateto_56c3a1_route_report(const char *backend, const struct json_t *label) {
    if (!strcmp(backend, "default") && !truthy(label)) return json_null();
    json_t *r = json_object();
    json_set(r, "backend", json_string(backend));
    if (truthy(label)) json_set(r, "label", json_copy(label));
    return r;
}

struct json_t *w14_delegateto_56c3a1_route_refused_entry(long long index, const char *backend,
                                                        const struct json_t *label, const struct json_t *error) {
    json_t *r = json_object();
    json_set(r, "task_index", json_number((double)index));
    json_set(r, "status", json_string("error"));
    json_set(r, "summary", json_null());
    json_set(r, "error", error ? json_copy(error) : json_null());
    json_set(r, "api_calls", json_number(0));
    json_set(r, "duration_seconds", json_number(0));
    json_t *rt = json_object();
    json_t *rep = w14_delegateto_56c3a1_route_report(backend, label);
    if (rep && rep->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(rep); i++) {
            const char *k = json_object_get_key_at(rep, i);
            if (k) json_set(rt, k, json_copy(json_object_get_at(rep, i)));
        }
        json_free(rep);
    } else {
        json_set(rt, "backend", json_string(backend));
    }
    json_set(rt, "refused", json_bool(true));
    json_set(r, "route", rt);
    return r;
}

