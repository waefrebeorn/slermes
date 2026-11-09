/*
 * slv_value.c -- values, string interning and tables. See slv.h.
 *
 * Open addressing, no-delete: replace on hit, grow by doubling. Keys are
 * compared by identity for interned strings and by value for numbers.
 */
#include "slv_internal.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- values */

int slv_truthy(slv_value v) {
    if (v.t == SLV_NIL) return 0;
    if (v.t == SLV_BOOL) return v.as.b;
    return 1;
}

int slv_equal(slv_value a, slv_value b) {
    if (a.t != b.t) return 0;
    switch (a.t) {
    case SLV_NIL: return 1;
    case SLV_BOOL: return a.as.b == b.as.b;
    case SLV_NUM: return a.as.n == b.as.n;
    case SLV_STR: return strcmp(a.as.s, b.as.s) == 0;
    case SLV_TABLE: return a.as.tab == b.as.tab;
    case SLV_FUNCTION: return a.as.fn == b.as.fn;
    case SLV_CFUNCTION: return a.as.cf.f == b.as.cf.f;
    }
    return 0;
}

double slv_tonumber(slv_value v, int *ok) {
    if (v.t == SLV_NUM) { if (ok) *ok = 1; return v.as.n; }
    if (ok) *ok = 0;
    return v.as.n;
}

const char *slv_tostring(slv_value v) {
    static char fb[64];
    if (v.t == SLV_STR) return v.as.s;
    if (v.t == SLV_NUM) { snprintf(fb, sizeof fb, "%g", v.as.n); return fb; }
    return "(value)";
}

/* The single type-name table, shared by the error path and the `type`
 * builtin so the two cannot drift. */
const char *slv_type_name(slv_type t) {
    switch (t) {
    case SLV_NIL: return "nil";
    case SLV_BOOL: return "boolean";
    case SLV_NUM: return "number";
    case SLV_STR: return "string";
    case SLV_TABLE: return "table";
    case SLV_FUNCTION:
    case SLV_CFUNCTION: return "function";
    }
    return "unknown";
}

/* -------------------------------------------------------- string interning */

static unsigned strhash(const char *s, size_t n) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 16777619u; }
    return h;
}

const char *slv_intern(slv *L, const char *bytes, size_t len) {
    unsigned h = strhash(bytes, len), b = h % L->nbuckets;
    for (istr *it = L->buckets[b]; it; it = it->next)
        if (it->len == len && memcmp(it->s, bytes, len) == 0) return it->s;
    istr *n = (istr *)slv_arena_alloc(L, sizeof(*n) + len + 1);
    if (!n) return NULL;
    n->len = len; n->hash = h; n->s = (char *)(n + 1);
    memcpy(n->s, bytes, len); n->s[len] = 0;
    n->next = L->buckets[b]; L->buckets[b] = n;
    return n->s;
}

/* ---------------------------------------------------------------- tables */

static unsigned valhash(slv_value k) {
    if (k.t == SLV_STR) return strhash(k.as.s, strlen(k.as.s));
    if (k.t == SLV_NUM) { double d = k.as.n; return (unsigned)((unsigned long long)(d * 2654435761.0)); }
    return (unsigned)(uintptr_t)k.as.tab | (unsigned)(uintptr_t)k.as.fn;
}

static int keyeq(slv_value a, slv_value b) {
    if (a.t != b.t) return 0;
    if (a.t == SLV_NUM) return a.as.n == b.as.n;
    if (a.t == SLV_STR) return a.as.s == b.as.s;   /* interned + same hash table */
    return slv_equal(a, b);
}

static void tgrow(slv *L, slv_table *t) {
    size_t nc = t->cap ? t->cap * 2 : 16;
    slv_kv *ne = (slv_kv *)slv_arena_alloc(L, nc * sizeof(*ne));
    if (t->e) for (size_t i = 0; i < t->cap; i++)
        if (t->e[i].used) {
            unsigned hi = valhash(t->e[i].k) % nc, j = hi;
            while (ne[j].used) j = (j + 1) % nc;
            ne[j] = t->e[i];
        }
    t->e = ne; t->cap = nc;
}

static slv_kv *tfind(slv_table *t, slv_value k) {
    if (!t->cap) return NULL;
    unsigned hi = valhash(k) % t->cap, j = hi;
    while (t->e[j].used) {
        if (keyeq(t->e[j].k, k)) return &t->e[j];
        j = (j + 1) % t->cap;
    }
    return NULL;
}

slv_table *slv_table_new(slv *L, int narr, int nrec) {
    (void)narr; (void)nrec;
    return SLV_ALLOC(slv_table, L);
}

void slv_table_set(slv *L, slv_table *t, slv_value k, slv_value v) {
    if (t->count * 2 >= t->cap) tgrow(L, t);
    slv_kv *f = tfind(t, k);
    if (f) { f->v = v; return; }
    unsigned hi = valhash(k) % t->cap, j = hi;
    while (t->e[j].used) j = (j + 1) % t->cap;
    t->e[j].k = k; t->e[j].v = v; t->e[j].used = 1; t->count++;
}

slv_value slv_table_get(slv *L, const slv_table *tt, slv_value k) {
    (void)L;
    slv_kv *f = tfind((slv_table *)tt, k);
    return f ? f->v : slv_nil();
}
