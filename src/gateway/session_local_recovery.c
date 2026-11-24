/*
 * session_local_recovery.c — C home for upstream Hermes
 * gateway/session_local_recovery.py.
 *
 * PoP: w18_sessionloc_e14f46_merge_live_in_stored_order @ gateway/session_local_recovery.py:_merge_live_in_stored_order
 */
#include "noc_w18_sessionloc_e14f46.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

typedef struct { json_t **v; size_t n, cap; } jvec;

static void jpush(jvec *a, json_t *x) {
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 8;
        json_t **nv = (json_t **)realloc(a->v, nc * sizeof(*nv));
        if (!nv) return;
        a->v = nv; a->cap = nc;
    }
    a->v[a->n++] = x;
}
static void jfree_all(jvec *a) {
    size_t i;
    for (i = 0; i < a->n; ++i) json_free(a->v[i]);
    free(a->v); a->v = NULL; a->n = a->cap = 0;
}

static const char *row_id(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return "";
    return json_get_str(row, "_row_id", "");
}

struct json_t *w18_sessionloc_e14f46_merge_live_in_stored_order(
    const struct json_t *display, const struct json_t *live) {
    json_t *out = json_array();
    size_t nd, nl, i, j;
    const char **dids, **lids;
    jvec *groups;
    size_t ngroups = 0;
    jvec pending;
    int *shownflag;
    size_t *order;
    size_t seen = 0;

    if (!display || display->type != JSON_ARRAY ||
        !live || live->type != JSON_ARRAY) return out;
    nd = json_len(display); nl = json_len(live);

    dids = (const char **)malloc((nd ? nd : 1) * sizeof(char *));
    lids = (const char **)malloc((nl ? nl : 1) * sizeof(char *));
    groups = (jvec *)malloc((nd ? nd : 1) * sizeof(jvec));
    shownflag = (int *)calloc(nd ? nd : 1, sizeof(int));
    order = (size_t *)malloc((nl ? nl : 1) * sizeof(size_t));
    if (!dids || !lids || !groups || !shownflag || !order) {
        free(dids); free(lids); free(groups); free(shownflag); free(order);
        return out;
    }
    memset(groups, 0, (nd ? nd : 1) * sizeof(jvec));
    pending.v = NULL; pending.n = pending.cap = 0;

    for (i = 0; i < nl; ++i) lids[i] = row_id(json_get(live, i));
    for (i = 0; i < nd; ++i) dids[i] = row_id(json_get(display, i));

    /* Partition display rows into groups of archived rows before each live row. */
    for (i = 0; i < nd; ++i) {
        int inlive = 0;
        for (j = 0; j < nl; ++j) if (!strcmp(dids[i], lids[j])) { inlive = 1; break; }
        if (inlive) {
            groups[ngroups++] = pending;
            pending.v = NULL; pending.n = pending.cap = 0;
        } else {
            json_t *r = json_get(display, i);
            if (r && r->type == JSON_OBJECT) {
                json_t *c = json_copy(r);
                json_set(c, "archived", json_bool(1));
                jpush(&pending, c);
            }
        }
    }

    /* shown_ids = {display ids} & {live ids}. */
    for (i = 0; i < nd; ++i) {
        int inlive = 0;
        for (j = 0; j < nl; ++j) if (!strcmp(dids[i], lids[j])) { inlive = 1; break; }
        shownflag[i] = inlive;
    }

    /* Stable sort live indices by _row_id. */
    for (i = 0; i < nl; ++i) order[i] = i;
    for (i = 1; i < nl; ++i) {
        size_t cur = order[i];
        j = i;
        while (j > 0 && strcmp(lids[order[j - 1]], lids[cur]) > 0) {
            order[j] = order[j - 1]; --j;
        }
        order[j] = cur;
    }

    for (i = 0; i < nl; ++i) {
        size_t idx = order[i];
        int isshown = 0;
        for (j = 0; j < nd; ++j)
            if (shownflag[j] && !strcmp(dids[j], lids[idx])) { isshown = 1; break; }
        if (isshown && seen < ngroups) {
            jvec *g = &groups[seen++];
            for (j = 0; j < g->n; ++j) json_append(out, json_copy(g->v[j]));
        }
        {
            json_t *r = json_get(live, idx);
            json_append(out, r ? json_copy(r) : json_null());
        }
    }
    for (j = 0; j < pending.n; ++j) json_append(out, json_copy(pending.v[j]));

    for (i = 0; i < ngroups; ++i) jfree_all(&groups[i]);
    jfree_all(&pending);
    free(groups); free(dids); free(lids); free(shownflag); free(order);
    return out;
}
