/*
 * method_ctx.c -- faithful C11 port of upstream Hermes tui_gateway/method_ctx.py.
 *
 * PoP: w9_methodctx_14d3e0_names @ tui_gateway/method_ctx.py:HandlerRegistry.names
 * PoP: w9_methodctx_14d3e0_skip  @ tui_gateway/method_ctx.py:bind_module
 *
 * The code-object rebinding (types.FunctionType / closures) is Python-only; the
 * registrar's name set and bind_module's publish-skip predicate are portable and
 * are mirrored exactly, including the _PLUMBING constant.
 */
#include "noc_w9_methodctx_14d3e0.h"
#include <stdlib.h>
#include <string.h>

static int cmp_str_ptr(const void *a, const void *b) {
    const char *x = *(const char *const *)a;
    const char *y = *(const char *const *)b;
    return strcmp(x, y);
}

json_t *w9_methodctx_14d3e0_names(const json_t *methods) {
    size_t n = (methods && methods->type == JSON_ARRAY) ? json_len(methods) : 0;
    const char **arr = n ? (const char **)malloc(n * sizeof(*arr)) : NULL;
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(methods, i);
        if (e && e->type == JSON_STRING && e->str_val) arr[m++] = e->str_val;
    }
    if (m > 1) qsort(arr, m, sizeof(*arr), cmp_str_ptr);
    json_t *out = json_array();
    for (size_t i = 0; i < m; i++) {
        if (i == 0 || strcmp(arr[i], arr[i - 1]) != 0)
            json_append(out, json_string(arr[i]));
    }
    free(arr);
    return out;
}

static const char *const PLUMBING[] = {
    "HandlerRegistry", "method", "_profile_scoped", "register", "rebind", "logger"
};
#define NPLUMBING (sizeof(PLUMBING) / sizeof(PLUMBING[0]))

int w9_methodctx_14d3e0_skip(const char *name, const json_t *skip) {
    if (!name) return 1;
    if (name[0] == '_' && name[1] == '_') return 1;   /* startswith("__") */
    for (size_t i = 0; i < NPLUMBING; i++)
        if (!strcmp(name, PLUMBING[i])) return 1;
    if (skip && skip->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(skip); i++) {
            json_t *e = json_get(skip, i);
            if (e && e->type == JSON_STRING && e->str_val && !strcmp(name, e->str_val)) return 1;
        }
    }
    return 0;
}
