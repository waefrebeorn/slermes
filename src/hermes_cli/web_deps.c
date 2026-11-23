/*
 * web_deps.c — faithful C11 port of hermes_cli/web_deps.py
 * (gate w7_webdeps_8ac7d6).
 *
 * PoP: w7_webdeps_late_name @ hermes_cli/web_deps.py:late.<locals>._proxy
 * PoP: w7_webdeps_state_op @ hermes_cli/web_deps.py:LateState
 *
 * WEB_SERVER = "hermes_cli.web_server" is mirrored as a module-level literal;
 * the injected target stands in for getattr(module, name) at operation time,
 * exactly where the Python LateState resolves it through _server().
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const W7_WEB_SERVER = "hermes_cli.web_server";

/* ---------------- growable buffer ---------------- */
typedef struct { char *p; size_t n, cap; } w7_buf;
static void w7_reserve(w7_buf *b, size_t extra) {
    if (b->n + extra + 1 <= b->cap) return;
    size_t nc = (b->n + extra + 1) * 2;
    char *np = (char *)realloc(b->p, nc);
    if (!np) return;
    b->p = np; b->cap = nc;
}
static void w7_add(w7_buf *b, const char *s) {
    if (!s) return;
    size_t m = strlen(s);
    w7_reserve(b, m);
    if (!b->p || b->n + m + 1 > b->cap) return;
    memcpy(b->p + b->n, s, m);
    b->n += m; b->p[b->n] = '\0';
}
static void w7_ch(w7_buf *b, char c) {
    w7_reserve(b, 1);
    if (!b->p || b->n + 2 > b->cap) return;
    b->p[b->n++] = c; b->p[b->n] = '\0';
}

/* Python repr() of a JSON value — used by LateState.__repr__. */
static void w7_repr(const json_t *v, w7_buf *b) {
    if (!v || v->type == JSON_NULL) { w7_add(b, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: w7_add(b, v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: {
            char tmp[64];
            if (v->num_val == (double)(long long)v->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", v->num_val);
            else snprintf(tmp, sizeof(tmp), "%.17g", v->num_val);
            w7_add(b, tmp); return;
        }
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            int hs = strchr(s, '\'') != NULL, hd = strchr(s, '"') != NULL;
            char q = (hs && !hd) ? '"' : '\'';
            w7_ch(b, q);
            for (const char *p = s; *p; p++) {
                if (*p == '\\' || *p == q) w7_ch(b, '\\');
                if (*p == '\n') { w7_add(b, "\\n"); continue; }
                if (*p == '\r') { w7_add(b, "\\r"); continue; }
                if (*p == '\t') { w7_add(b, "\\t"); continue; }
                w7_ch(b, *p);
            }
            w7_ch(b, q); return;
        }
        case JSON_ARRAY:
            w7_ch(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w7_add(b, ", ");
                w7_repr(v->c.items[i], b);
            }
            w7_ch(b, ']'); return;
        case JSON_OBJECT:
            w7_ch(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) w7_add(b, ", ");
                w7_ch(b, '\'');
                for (const char *p = v->c.keys ? v->c.keys[i] : ""; p && *p; p++) {
                    if (*p == '\\' || *p == '\'') w7_ch(b, '\\');
                    w7_ch(b, *p);
                }
                w7_add(b, "': ");
                w7_repr(v->c.items[i], b);
            }
            w7_ch(b, '}'); return;
        default: return;
    }
}

static int w7_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return v->c.count != 0;
    return 0;
}

/* Python == for JSON values (same-type comparisons used by the fixtures). */
static int w7_equal(const json_t *a, const json_t *b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) {
        if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
            return a->num_val == b->num_val;
        return 0;
    }
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !strcmp(a->str_val ? a->str_val : "",
                                          b->str_val ? b->str_val : "");
        case JSON_ARRAY: {
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++)
                if (!w7_equal(a->c.items[i], b->c.items[i])) return 0;
            return 1;
        }
        case JSON_OBJECT: {
            if (a->c.count != b->c.count) return 0;
            for (size_t i = 0; i < a->c.count; i++) {
                const char *k = a->c.keys ? a->c.keys[i] : "";
                json_t *other = json_obj_get(b, k);
                if (!other || !w7_equal(a->c.items[i], other)) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

static int w7_compare(const json_t *a, const json_t *b) {
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER)
        return a->num_val < b->num_val ? -1 : (a->num_val > b->num_val ? 1 : 0);
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "");
    return 0;
}

static int w7_contains(const json_t *target, const json_t *arg) {
    if (!target) return 0;
    if (target->type == JSON_STRING) {
        const char *needle = (arg && arg->type == JSON_STRING && arg->str_val) ? arg->str_val : "";
        return *needle ? strstr(target->str_val ? target->str_val : "", needle) != NULL : 1;
    }
    if (target->type == JSON_ARRAY) {
        for (size_t i = 0; i < target->c.count; i++)
            if (w7_equal(target->c.items[i], arg)) return 1;
        return 0;
    }
    if (target->type == JSON_OBJECT) {
        const char *key = (arg && arg->type == JSON_STRING && arg->str_val) ? arg->str_val : "";
        return json_obj_get(target, key) != NULL;
    }
    return 0;
}

static json_t *w7_getitem(const json_t *target, const json_t *arg) {
    if (!target) return json_null();
    if (target->type == JSON_OBJECT) {
        const char *key = (arg && arg->type == JSON_STRING && arg->str_val) ? arg->str_val : "";
        json_t *v = json_obj_get(target, key);
        return v ? json_copy(v) : json_null();
    }
    long idx = 0;
    if (arg && arg->type == JSON_NUMBER) idx = (long)arg->num_val;
    else if (arg && arg->type == JSON_STRING && arg->str_val) idx = strtol(arg->str_val, NULL, 10);
    if (idx < 0) idx = 0;
    if (target->type == JSON_ARRAY) {
        json_t *v = json_get(target, (size_t)idx);
        return v ? json_copy(v) : json_null();
    }
    if (target->type == JSON_STRING && target->str_val && target->str_val[idx]) {
        char one[2] = {target->str_val[idx], '\0'};
        return json_string(one);
    }
    return json_null();
}

json_t *w7_webdeps_late_name(const char *name, const char *module) {
    (void)module;  /* module selection is reflected in the injected target */
    json_t *out = json_array();
    const char *n = name ? name : "";
    json_append(out, json_string(n));
    json_append(out, json_string(n));
    return out;
}

json_t *w7_webdeps_state_op(const char *op, const char *name,
                            const json_t *target, const json_t *arg) {
    if (!strcmp(op, "getitem")) return w7_getitem(target, arg);
    if (!strcmp(op, "contains")) return json_bool(w7_contains(target, arg) ? true : false);
    if (!strcmp(op, "len")) {
        size_t n = 0;
        if (target && (target->type == JSON_ARRAY || target->type == JSON_OBJECT)) n = target->c.count;
        else if (target && target->type == JSON_STRING && target->str_val) n = strlen(target->str_val);
        return json_number((double)n);
    }
    if (!strcmp(op, "bool")) return json_bool(w7_truthy(target) ? true : false);
    if (!strcmp(op, "repr")) {
        w7_buf b = {0}; b.p = (char *)malloc(1); if (!b.p) return json_null();
        b.p[0] = '\0'; b.cap = 1;
        w7_add(&b, "<LateState "); w7_add(&b, name ? name : "");
        w7_add(&b, " -> "); w7_repr(target, &b); w7_add(&b, ">");
        json_t *out = json_string(b.p); free(b.p); return out;
    }
    if (!strcmp(op, "attr")) return w7_getitem(target, arg);
    if (!strcmp(op, "eq")) return json_bool(w7_equal(target, arg) ? true : false);
    if (!strcmp(op, "ne")) return json_bool(!w7_equal(target, arg) ? true : false);
    int c = w7_compare(target, arg);
    if (!strcmp(op, "lt")) return json_bool(c < 0 ? true : false);
    if (!strcmp(op, "le")) return json_bool(c <= 0 ? true : false);
    if (!strcmp(op, "gt")) return json_bool(c > 0 ? true : false);
    if (!strcmp(op, "ge")) return json_bool(c >= 0 ? true : false);
    return json_string("UNKNOWN_OP");
}
