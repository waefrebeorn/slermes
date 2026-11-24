/*
 * session_managed_worker.c — C home for upstream Hermes
 * gateway/session_managed_worker.py.
 *
 * PoP: w18_sessionman_98782b_continuation_path @ gateway/session_managed_worker.py:_continuation_path
 * PoP: w18_sessionman_98782b_compression_ancestors @ gateway/session_managed_worker.py:_compression_ancestors
 * PoP: w18_sessionman_98782b_with_stop_cause @ gateway/session_managed_worker.py:_with_stop_cause
 */
#include "noc_w18_sessionman_98782b.h"

#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static long long gateway_session_managed_worker_CONST_HELLO_SECONDS = 60;
static long long gateway_session_managed_worker_CONST_STOP_ACK_SECONDS = 30;
static long long gateway_session_managed_worker_CONST_LOSS_RECORD_WAIT_S = 10;

typedef struct { const char **v; size_t n, cap; } svec;

static void spush(svec *a, const char *s) {
    if (a->n == a->cap) {
        size_t nc = a->cap ? a->cap * 2 : 8;
        const char **nv = (const char **)realloc(a->v, nc * sizeof(*nv));
        if (!nv) return;
        a->v = nv; a->cap = nc;
    }
    a->v[a->n++] = s;
}
static int scontains(const svec *a, const char *s) {
    size_t i;
    for (i = 0; i < a->n; ++i)
        if ((a->v[i] && s && !strcmp(a->v[i], s)) || (!a->v[i] && !s)) return 1;
    return 0;
}
static json_t *sessions_of(const json_t *store) {
    json_t *s = store ? json_obj_get(store, "sessions") : NULL;
    return (s && s->type == JSON_OBJECT) ? s : NULL;
}
static json_t *get_row(const json_t *sessions, const char *sid) {
    json_t *r;
    if (!sessions || !sid) return NULL;
    r = json_obj_get(sessions, sid);
    return (r && r->type == JSON_OBJECT) ? r : NULL;
}
static int is_comp_child(const json_t *row) {
    return json_is_true(json_obj_get(row, "compression_child")) ? 1 : 0;
}
static const char *parent_of(const json_t *row) {
    json_t *p = json_obj_get(row, "parent_session_id");
    return (p && p->type == JSON_STRING) ? p->str_val : NULL;
}

struct json_t *w18_sessionman_98782b_continuation_path(
    const struct json_t *store, const char *start, const char *tip) {
    json_t *sessions = sessions_of(store);
    json_t *out = json_array();
    svec path;
    const char *cur;
    size_t i;
    int ok = 0;

    path.v = NULL; path.n = path.cap = 0;
    if (!start) start = "";
    if (!tip) tip = "";
    cur = tip;
    spush(&path, tip);

    while (strcmp(cur, start) != 0 && path.n <= 200) {
        json_t *row = get_row(sessions, cur);
        const char *p;
        if (!row || !is_comp_child(row)) goto done;
        p = parent_of(row);
        if (!p) goto done; /* current becomes None; next lookup misses -> None */
        cur = p;
        spush(&path, cur);
    }
    if (strcmp(cur, start) == 0) ok = 1;

done:
    if (ok) {
        for (i = path.n; i > 0; --i) {
            const char *id = path.v[i - 1];
            json_append(out, id ? json_string(id) : json_null());
        }
    } else {
        json_free(out);
        out = json_null();
    }
    free(path.v);
    (void)gateway_session_managed_worker_CONST_HELLO_SECONDS;
    (void)gateway_session_managed_worker_CONST_STOP_ACK_SECONDS;
    (void)gateway_session_managed_worker_CONST_LOSS_RECORD_WAIT_S;
    return out;
}

struct json_t *w18_sessionman_98782b_compression_ancestors(
    const struct json_t *store, const char *session_id) {
    json_t *sessions = sessions_of(store);
    json_t *out = json_array();
    svec anc;
    json_t *row;
    size_t i;

    anc.v = NULL; anc.n = anc.cap = 0;
    if (!session_id) session_id = "";
    row = get_row(sessions, session_id);
    while (row && is_comp_child(row) && anc.n < 200) {
        const char *p = parent_of(row);
        if (p && (scontains(&anc, p) || !strcmp(p, session_id))) break;
        spush(&anc, p);
        row = get_row(sessions, p);
    }
    for (i = anc.n; i > 0; --i) {
        const char *id = anc.v[i - 1];
        json_append(out, id ? json_string(id) : json_null());
    }
    free(anc.v);
    return out;
}

struct json_t *w18_sessionman_98782b_with_stop_cause(
    const struct json_t *accepted, const struct json_t *worker) {
    json_t *out = json_object();
    const char *cause;
    int interrupted;

    if (!accepted || accepted->type != JSON_OBJECT) return out;
    /* copy all accepted keys first (echo semantics of {**accepted, ...}) */
    {
        size_t i;
        for (i = 0; i < json_object_size(accepted); ++i) {
            const char *k = json_object_get_key_at(accepted, i);
            if (k) json_set(out, k, json_copy(json_object_get_at(accepted, i)));
        }
    }
    cause = worker ? json_get_str(worker, "stop_cause", NULL) : NULL;
    interrupted = json_is_true(json_obj_get(accepted, "interrupted")) ? 1 : 0;
    if (interrupted && cause && cause[0] != '\0')
        json_set(out, "interrupt_message", json_string(cause));
    return out;
}
