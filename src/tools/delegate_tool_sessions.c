/*
 * delegate_tool_sessions.c - C home for upstream Hermes tools/delegate_tool_sessions.py.
 * PoP: w16_delegateto_87a492_stored_child_session @ tools/delegate_tool_sessions.py:stored_child_session
 *
 * The store is injected as JSON (`candidates` + `rows`): `subagent_session_ids` becomes the
 * candidate list and `sessions.get` becomes a lookup in `rows`. All lineage rules
 * (_lineage_reader / owns / _continues / _delegated_from) are mirrored, including path-cycle
 * detection and verdict memoisation.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_87a492.h"

static const char *SUBAGENT_ID_KEY = "_subagent_id"; /* PoP constant; same key the store is queried with */

typedef struct { char **v; size_t n, cap; } strvec_t;
static void sv_push(strvec_t *s, const char *x) {
    if (s->n == s->cap) { size_t nc = s->cap ? s->cap * 2 : 8; char **nv = (char **)realloc(s->v, nc * sizeof(char *)); if (!nv) return; s->v = nv; s->cap = nc; }
    s->v[s->n++] = (char *)x; /* borrowed: points into the parsed input tree */
}
static int sv_has(const strvec_t *s, const char *x) {
    for (size_t i = 0; i < s->n; i++) if (s->v[i] && x && !strcmp(s->v[i], x)) return 1;
    return 0;
}
static int arr_has_str(const json_t *arr, const char *x) {
    if (!arr || arr->type != JSON_ARRAY || !x || !x[0]) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, x)) return 1;
    }
    return 0;
}

typedef struct {
    const json_t *rows;
    const json_t *ancestors;
    strvec_t vkeys;
    unsigned char *vvals;
    size_t vn, vcap;
} lin_t;

static const json_t *get_row(const lin_t *ctx, const char *sid) {
    if (!ctx->rows || ctx->rows->type != JSON_OBJECT || !sid || !sid[0]) return NULL;
    const json_t *r = json_obj_get(ctx->rows, sid);
    return (r && r->type != JSON_NULL) ? r : NULL;
}
static int verdict_get(const lin_t *ctx, const char *sid, unsigned char *out) {
    for (size_t i = 0; i < ctx->vn; i++) if (!strcmp(ctx->vkeys.v[i], sid)) { *out = ctx->vvals[i]; return 1; }
    return 0;
}
static void verdict_set(lin_t *ctx, const char *sid, unsigned char val) {
    for (size_t i = 0; i < ctx->vn; i++) if (!strcmp(ctx->vkeys.v[i], sid)) { ctx->vvals[i] = val; return; }
    if (ctx->vn == ctx->vcap) {
        size_t nc = ctx->vcap ? ctx->vcap * 2 : 8;
        char **nk = (char **)realloc(ctx->vkeys.v, nc * sizeof(char *));
        unsigned char *nv = (unsigned char *)realloc(ctx->vvals, nc);
        if (!nk || !nv) { free(nk); free(nv); return; }
        ctx->vkeys.v = nk; ctx->vvals = nv; ctx->vcap = nc;
    }
    ctx->vkeys.v[ctx->vn] = (char *)sid; ctx->vvals[ctx->vn] = val; ctx->vn++;
}
static const char *mc_get(const json_t *row, const char *key) {
    const json_t *mc = json_obj_get(row, "model_config");
    if (!mc || mc->type != JSON_OBJECT) return NULL;
    const json_t *v = json_obj_get(mc, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static int delegated_from(const json_t *row, const char *parent) {
    const char *v = mc_get(row, "_delegate_from");
    return v && parent && !strcmp(v, parent);
}
static int continues(lin_t *ctx, const json_t *row) {
    const char *psid = json_get_str(row, "parent_session_id", "");
    if (!psid || !psid[0]) return 0;
    const json_t *parent = get_row(ctx, psid);
    if (!parent) return 0;
    const json_t *source = json_obj_get(row, "source");
    if (source && source->type == JSON_STRING && source->str_val && !strcmp(source->str_val, "tool")) return 0;
    static const char *const MARKERS[] = {"_branched_from", "_delegate_from", "_reset_from"};
    for (size_t i = 0; i < 3; i++) {
        const char *m = mc_get(row, MARKERS[i]);
        if (m && !strcmp(m, psid)) return 0;
    }
    const char *end_reason = json_get_str(parent, "end_reason", "");
    return end_reason && !strcmp(end_reason, "compression");
}
static int owns(lin_t *ctx, const char *sid) {
    const json_t *current = get_row(ctx, sid);
    unsigned char verdict = 0;
    strvec_t path = {0};
    while (current) {
        const char *csid = json_get_str(current, "session_id", "");
        unsigned char cached = 0;
        if (verdict_get(ctx, csid, &cached)) { verdict = cached; break; }
        if (arr_has_str(ctx->ancestors, csid)) { verdict = 1; break; }
        if (sv_has(&path, csid)) break;
        sv_push(&path, csid);
        const char *parent = json_get_str(current, "parent_session_id", "");
        if (!parent || !parent[0] || !(delegated_from(current, parent) || continues(ctx, current))) break;
        current = get_row(ctx, parent);
    }
    for (size_t i = 0; i < path.n; i++) verdict_set(ctx, path.v[i], verdict);
    free(path.v);
    return verdict ? 1 : 0;
}

struct json_t *w16_delegateto_87a492_stored_child_session(const struct json_t *args) {
    const char *subid = json_get_str(args, "subagent_id", "");
    const json_t *ancestors = json_obj_get(args, "conversation_ids");
    const json_t *candidates = json_obj_get(args, "candidates");
    int any_ancestor = 0;
    if (ancestors && ancestors->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(ancestors); i++) {
            const json_t *a = json_get(ancestors, i);
            if (a && a->type == JSON_STRING && a->str_val && a->str_val[0]) { any_ancestor = 1; break; }
        }
    if (!subid[0] || !any_ancestor) return json_null();
    (void)SUBAGENT_ID_KEY;

    lin_t ctx = {0};
    ctx.rows = json_obj_get(args, "rows");
    ctx.ancestors = ancestors;
    strvec_t owned = {0};
    if (candidates && candidates->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(candidates); i++) {
            const json_t *c = json_get(candidates, i);
            if (c && c->type == JSON_STRING && c->str_val && owns(&ctx, c->str_val)) sv_push(&owned, c->str_val);
        }
    strvec_t continued = {0};
    for (size_t i = 0; i < owned.n; i++) {
        const json_t *seg = get_row(&ctx, owned.v[i]);
        if (!seg) continue;
        const char *psid = json_get_str(seg, "parent_session_id", "");
        if (psid && sv_has(&owned, psid) && continues(&ctx, seg)) sv_push(&continued, psid);
    }
    json_t *result = json_null();
    for (size_t i = 0; i < owned.n; i++) {
        if (!sv_has(&continued, owned.v[i])) { result = json_string(owned.v[i]); break; }
    }
    free(owned.v); free(continued.v); free(ctx.vkeys.v); free(ctx.vvals);
    return result;
}

