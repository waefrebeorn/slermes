/*
 * registry.c — C home for upstream Hermes pm/registry.py (gate w5_registry_8fd451).
 *
 * PoP: w5_registry_8fd451_walk @ pm/registry.py:walk
 *
 * Faithful deps-first topological walk with the same cycle detection and
 * message shape as the Python original.  The package catalogue is injected as
 * a name -> [deps] map, so no import/discovery side effects are needed.
 */
#include "noc_w5_registry_8fd451.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const json_t *deps;   /* object: name -> array of dep-name strings */
    char **seen;          /* names already emitted, in order */
    size_t nseen, cseen;
    char **chain;         /* current recursion stack */
    size_t nchain, cchain;
    char err[1024];       /* cycle message when non-empty */
} w5r_ctx;

static void w5r_push(char ***arr, size_t *n, size_t *cap, const char *s) {
    if (*n + 1 >= *cap) {
        size_t nc = *cap ? *cap * 2 : 16;
        char **nb = (char **)realloc(*arr, nc * sizeof(char *));
        if (!nb) return;
        *arr = nb; *cap = nc;
    }
    (*arr)[(*n)++] = strdup(s);
}
static int w5r_has(char *const *arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++)
        if (arr[i] && strcmp(arr[i], s) == 0) return 1;
    return 0;
}

static int w5r_visit(w5r_ctx *ctx, const char *name) {
    if (w5r_has(ctx->chain, ctx->nchain, name)) {
        /* " -> ".join(chain + (name,)) */
        char msg[1024] = "dependency cycle: ";
        size_t pos = strlen(msg);
        for (size_t i = 0; i < ctx->nchain; i++) {
            int w = snprintf(msg + pos, sizeof(msg) - pos, "%s%s",
                             i ? " -> " : "", ctx->chain[i]);
            if (w < 0 || (size_t)w >= sizeof(msg) - pos) break;
            pos += (size_t)w;
        }
        snprintf(msg + pos, sizeof(msg) - pos, "%s%s", ctx->nchain ? " -> " : "", name);
        snprintf(ctx->err, sizeof(ctx->err), "%s", msg);
        return -1;
    }
    if (w5r_has(ctx->seen, ctx->nseen, name)) return 0;

    w5r_push(&ctx->chain, &ctx->nchain, &ctx->cchain, name);
    json_t *d = json_obj_get(ctx->deps, name);
    if (d && d->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(d); i++) {
            json_t *item = json_get(d, i);
            const char *dep = (item && item->type == JSON_STRING) ? item->str_val : NULL;
            if (!dep) continue;
            if (w5r_visit(ctx, dep) != 0) return -1;
        }
    }
    /* pop chain */
    if (ctx->nchain) ctx->nchain--;
    w5r_push(&ctx->seen, &ctx->nseen, &ctx->cseen, name);
    return 0;
}

json_t *w5_registry_8fd451_walk(const json_t *names, const json_t *deps) {
    w5r_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.deps = deps;

    if (names && names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names); i++) {
            json_t *item = json_get(names, i);
            const char *nm = (item && item->type == JSON_STRING) ? item->str_val : NULL;
            if (!nm) continue;
            if (w5r_visit(&ctx, nm) != 0) break;
        }
    }

    json_t *out;
    if (ctx.err[0]) {
        out = json_object();
        json_set(out, "__raise__", json_string("ValueError"));
        json_set(out, "msg", json_string(ctx.err));
    } else {
        out = json_array();
        for (size_t i = 0; i < ctx.nseen; i++) json_append(out, json_string(ctx.seen[i]));
    }
    for (size_t i = 0; i < ctx.nseen; i++) free(ctx.seen[i]);
    free(ctx.seen);
    for (size_t i = 0; i < ctx.nchain; i++) free(ctx.chain[i]);
    free(ctx.chain);
    return out;
}

