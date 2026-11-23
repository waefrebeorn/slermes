/*
 * gateway_commands.c — C home for upstream Hermes acp_adapter/gateway_commands.py.
 *
 * PoP: w15_gatewaycom_6e4e92_deferred_pick_text @ acp_adapter/gateway_commands.py:deferred_pick_text
 * PoP: w15_gatewaycom_6e4e92_refused_pick_text @ acp_adapter/gateway_commands.py:refused_pick_text
 * PoP: w15_gatewaycom_6e4e92_summary_text @ acp_adapter/gateway_commands.py:_summary_text
 * PoP: w15_gatewaycom_6e4e92_model_text @ acp_adapter/gateway_commands.py:_model_text
 * PoP: w15_gatewaycom_6e4e92_tools_text @ acp_adapter/gateway_commands.py:_tools_text
 *
 * The module's async RPC handlers need a live gateway; these are its pure reply
 * formatters, ported with Python `or` truthiness and str() coercion preserved.
 */
#include "noc_w15_gatewaycom_6e4e92.h"
#include "hermes_json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

typedef struct { char *p; size_t n, cap; } sb_t;
static void sb_add(sb_t *b, const char *s, size_t l) {
    if (!s) return;
    if (b->n + l + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 128;
        while (nc < b->n + l + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->n, s, l); b->n += l; b->p[b->n] = '\0';
}
static void sb_adds(sb_t *b, const char *s) { sb_add(b, s, strlen(s)); }
static void sb_addf(sb_t *b, const char *fmt, ...) {
    char tmp[1024];
    va_list ap; va_start(ap, fmt);
    int k = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (k > 0) sb_add(b, tmp, (size_t)k < sizeof(tmp) ? (size_t)k : sizeof(tmp) - 1);
}
static char *sb_take(sb_t *b) {
    char *p = b->p ? b->p : dupstr("");
    b->p = NULL; b->n = b->cap = 0;
    return p;
}
static json_t *sb_json(sb_t *b) { char *s = sb_take(b); json_t *j = json_string(s); free(s); return j; }

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}
static const char *py_str(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return buf; }
    switch (v->type) {
        case JSON_STRING: return v->str_val ? v->str_val : "";
        case JSON_BOOL: snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return buf;
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == (double)(long long)d && d >= -9e15 && d <= 9e15)
                snprintf(buf, n, "%lld", (long long)d);
            else
                snprintf(buf, n, "%.17g", d);
            return buf;
        }
        default: buf[0] = '\0'; return buf;
    }
}
static const char *or_str(const json_t *v, const char *def, char *buf, size_t n) {
    if (py_truthy(v)) return py_str(v, buf, n);
    snprintf(buf, n, "%s", def ? def : "");
    return buf;
}

json_t *w15_gatewaycom_6e4e92_deferred_pick_text(const struct json_t *result) {
    const json_t *r = (const json_t *)result;
    char b[1024];
    const char *model = or_str(json_obj_get(r, "model"), "The picked model", b, sizeof(b));
    sb_t s = {0};
    sb_addf(&s, "%s applies from your next message.", model);
    return sb_json(&s);
}

json_t *w15_gatewaycom_6e4e92_refused_pick_text(const struct json_t *update) {
    const json_t *u = (const json_t *)update;
    char b1[1024], b2[1024], b3[1024], b4[1024];
    const char *model = or_str(json_obj_get(u, "model"), "the current model", b1, sizeof(b1));
    const char *provider = or_str(json_obj_get(u, "provider"), "", b2, sizeof(b2));
    const char *target = or_str(json_obj_get(u, "target_model"), "the picked model", b3, sizeof(b3));
    const char *reason = or_str(json_obj_get(u, "reason"), "refused", b4, sizeof(b4));
    sb_t s = {0};
    sb_addf(&s, "Model switch to %s was not applied (%s); still using %s", target, reason, model);
    if (py_truthy(json_obj_get(u, "provider"))) sb_addf(&s, " (%s)", provider);
    sb_adds(&s, ".");
    return sb_json(&s);
}

json_t *w15_gatewaycom_6e4e92_summary_text(const struct json_t *result) {
    const json_t *r = (const json_t *)result;
    const json_t *summary = json_obj_get(r, "summary");
    sb_t s = {0};
    static const char *const keys[] = {"headline", "token_line", "note"};
    int first = 1;
    if (summary && summary->type == JSON_OBJECT) {
        for (int i = 0; i < 3; ++i) {
            const json_t *v = json_obj_get(summary, keys[i]);
            if (!py_truthy(v)) continue;
            char b[1024];
            const char *str = py_str(v, b, sizeof(b));
            if (!first) sb_adds(&s, "\n");
            sb_adds(&s, str);
            first = 0;
        }
    }
    return sb_json(&s);
}

json_t *w15_gatewaycom_6e4e92_model_text(const char *label, const struct json_t *fields) {
    const json_t *f = (const json_t *)fields;
    char b1[1024], b2[1024];
    const char *model = or_str(json_obj_get(f, "model"), "unknown", b1, sizeof(b1));
    const char *provider = or_str(json_obj_get(f, "provider"), "", b2, sizeof(b2));
    sb_t s = {0};
    sb_addf(&s, "%s: %s", label ? label : "", model);
    if (py_truthy(json_obj_get(f, "provider"))) sb_addf(&s, "\nProvider: %s", provider);
    return sb_json(&s);
}

static int cmp_str(const void *a, const void *b) {
    const char *x = *(const char *const *)a, *y = *(const char *const *)b;
    return strcmp(x, y);
}
static int has_str(const char *const *v, size_t n, const char *s) {
    for (size_t i = 0; i < n; ++i) if (!strcmp(v[i], s)) return 1;
    return 0;
}
static const char **collect_names(const json_t *groups, size_t *out_n) {
    const char **names = NULL;
    size_t n = 0, cap = 0;
    if (groups && groups->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(groups); ++i) {
            const json_t *tools = json_object_get_at(groups, i);
            if (!tools || tools->type != JSON_ARRAY) continue;
            for (size_t j = 0; j < json_len(tools); ++j) {
                const json_t *nm = json_get(tools, j);
                if (!nm || nm->type != JSON_STRING || !nm->str_val) continue;
                if (has_str(names, n, nm->str_val)) continue;
                if (n == cap) {
                    cap = cap ? cap * 2 : 16;
                    const char **nn = (const char **)realloc(names, cap * sizeof(*nn));
                    if (!nn) break;
                    names = nn;
                }
                names[n++] = nm->str_val;
            }
        }
    }
    if (n > 1) qsort(names, n, sizeof(*names), cmp_str);
    *out_n = n;
    return names;
}
static char *join_strings(const char *const *v, size_t n, const char *sep) {
    sb_t s = {0};
    for (size_t i = 0; i < n; ++i) {
        if (i) sb_adds(&s, sep);
        sb_adds(&s, v[i]);
    }
    return sb_take(&s);
}

json_t *w15_gatewaycom_6e4e92_tools_text(const struct json_t *info) {
    const json_t *groups = json_obj_get((const json_t *)info, "tools");
    size_t nnames = 0;
    const char **names = collect_names(groups, &nnames);
    sb_t s = {0};
    if (nnames == 0) {
        const json_t *toolsets = json_obj_get((const json_t *)info, "enabled_toolsets");
        if (toolsets && toolsets->type == JSON_ARRAY && json_len(toolsets) > 0) {
            size_t n = json_len(toolsets);
            const char **arr = (const char **)malloc(n * sizeof(*arr));
            size_t k = 0;
            for (size_t i = 0; i < n; ++i) {
                const json_t *v = json_get(toolsets, i);
                arr[k++] = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
            }
            char *j = join_strings(arr, k, ", ");
            sb_addf(&s, "Enabled toolsets: %s", j);
            free(j); free(arr);
        } else {
            sb_adds(&s, "No tools available.");
        }
    } else {
        sb_addf(&s, "Available tools (%zu):", nnames);
        if (groups && groups->type == JSON_OBJECT) {
            size_t ng = json_object_size(groups);
            size_t idx[512];
            if (ng > 512) ng = 512;
            for (size_t i = 0; i < ng; ++i) idx[i] = i;
            for (size_t i = 0; i < ng; ++i) {
                for (size_t j = i + 1; j < ng; ++j) {
                    if (strcmp(json_object_get_key_at(groups, idx[i]),
                               json_object_get_key_at(groups, idx[j])) > 0) {
                        size_t t = idx[i]; idx[i] = idx[j]; idx[j] = t;
                    }
                }
            }
            for (size_t i = 0; i < ng; ++i) {
                const char *key = json_object_get_key_at(groups, idx[i]);
                const json_t *tools = json_object_get_at(groups, idx[i]);
                if (!tools || tools->type != JSON_ARRAY || json_len(tools) == 0) continue;
                size_t nt = json_len(tools);
                const char **arr = (const char **)malloc(nt * sizeof(*arr));
                size_t k = 0;
                for (size_t j = 0; j < nt; ++j) {
                    const json_t *v = json_get(tools, j);
                    arr[k++] = (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
                }
                qsort(arr, k, sizeof(*arr), cmp_str);
                char *j = join_strings(arr, k, ", ");
                sb_addf(&s, "\n  %s: %s", key ? key : "", j);
                free(j); free(arr);
            }
        }
    }
    free(names);
    return sb_json(&s);
}
