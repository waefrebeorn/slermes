/*
 * plugin_loader.c — C home for upstream Hermes plugins/plugin_loader.py.
 *
 * PoP: w8_pluginloade_73daa6_plugin_child_dir @ plugins/plugin_loader.py:plugin_child_dir
 * PoP: w8_pluginloade_73daa6_load_would_deadlock @ plugins/plugin_loader.py:_load_would_deadlock
 *
 * The wait graph is injected via the fixture (no real threads/locks), so the
 * deadlock walk is a deterministic port of _waits_for/_load_would_deadlock.
 * All exports are w8_pluginloade_73daa6_ prefixed.
 */
#include "noc_w8_pluginloade_73daa6.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pl_is_first(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
static int pl_is_rest(char c) {
    return pl_is_first(c) || c == '_' || c == '-';
}

char *w8_pluginloade_73daa6_plugin_child_dir(const char *root, const char *name) {
    if (!name || !*name || !pl_is_first(name[0])) return NULL;
    size_t n = strlen(name);
    if (n > 64) return NULL;
    for (size_t i = 1; i < n; ++i) if (!pl_is_rest(name[i])) return NULL;
    const char *r = root ? root : "";
    size_t rn = strlen(r);
    while (rn > 1 && r[rn - 1] == '/') rn--;
    char *o = (char *)malloc(rn + 1 + n + 1);
    if (!o) return NULL;
    if (rn) memcpy(o, r, rn);
    size_t p = rn;
    if (p && o[p - 1] != '/') o[p++] = '/';
    memcpy(o + p, name, n + 1);
    return o;
}

/* --- injected wait-graph deadlock walk --- */
#define PL_MAX 256

static int pl_find_owner(const json_t *owners, const char *mod) {
    json_t *v = json_obj_get(owners, mod);
    return (v && v->type == JSON_NUMBER) ? (int)v->num_val : -1;
}
static const char *pl_waiter_mod(const json_t *waiters, int tid) {
    for (size_t i = 0; i < json_object_size(waiters); ++i) {
        const char *k = json_object_get_key_at(waiters, i);
        if (k && strtol(k, NULL, 10) == tid && k[0] >= '0' && k[0] <= '9')
            return json_get_str(waiters, k, "");
    }
    return NULL;
}
static json_t *pl_blocked(const json_t *blocking, int tid) {
    for (size_t i = 0; i < json_object_size(blocking); ++i) {
        const char *k = json_object_get_key_at(blocking, i);
        if (k && k[0] >= '0' && k[0] <= '9' && strtol(k, NULL, 10) == tid)
            return json_object_get_at(blocking, i);
    }
    return NULL;
}
static void pl_add(int *arr, size_t *n, int v) {
    if (*n >= PL_MAX) return;
    for (size_t i = 0; i < *n; ++i) if (arr[i] == v) return;
    arr[(*n)++] = v;
}
static int pl_seen(const int *arr, size_t n, int v) {
    for (size_t i = 0; i < n; ++i) if (arr[i] == v) return 1;
    return 0;
}
/* Mirrors _waits_for(tid): loader-lock owner + importlib blocking owners. */
static size_t pl_waits_for(int tid, const json_t *owners, const json_t *waiters,
                           const json_t *blocking, int *out) {
    size_t n = 0;
    const char *mod = pl_waiter_mod(waiters, tid);
    if (mod && *mod) {
        int owner = pl_find_owner(owners, mod);
        if (owner >= 0) pl_add(out, &n, owner);
    }
    json_t *b = pl_blocked(blocking, tid);
    if (b) {
        if (b->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(b); ++i) {
                json_t *item = json_get(b, i);
                if (item && item->type == JSON_NUMBER) pl_add(out, &n, (int)item->num_val);
            }
        } else if (b->type == JSON_NUMBER) {
            pl_add(out, &n, (int)b->num_val);
        }
    }
    return n;
}

int w8_pluginloade_73daa6_load_would_deadlock(const struct json_t *graph) {
    if (!graph) return 0;
    int me = (int)json_get_num(graph, "me", 0);
    json_t *owners = json_obj_get(graph, "owners");
    json_t *waiters = json_obj_get(graph, "waiters");
    json_t *blocking = json_obj_get(graph, "blocking");
    int todo[PL_MAX], seen[PL_MAX];
    size_t nt = 0, ns = 0;
    int first[PL_MAX];
    size_t nf = pl_waits_for(me, owners, waiters, blocking, first);
    for (size_t i = 0; i < nf; ++i) todo[nt++] = first[i];
    while (nt) {
        int tid = todo[--nt];
        if (tid == me) return 1;
        if (!pl_seen(seen, ns, tid)) {
            int nxt[PL_MAX];
            seen[ns++] = tid;
            size_t n = pl_waits_for(tid, owners, waiters, blocking, nxt);
            for (size_t i = 0; i < n; ++i) todo[nt++] = nxt[i];
        }
    }
    return 0;
}
