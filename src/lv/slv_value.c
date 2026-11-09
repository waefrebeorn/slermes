/*
 * slv_value.c -- values, string interning and tables. See slv_internal.h.
 *
 * Tables are open-addressed with no deletion: entries are replaced on hit and
 * the table grows by doubling. `#t` is the array-part border, cached and
 * invalidated on every write.
 */
#include "slv_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- values */

int slv_truthy(slv_value v) {
    if (v.t == SLV_NIL) return 0;
    if (v.t == SLV_BOOL) return v.as.b;
    return 1;
}

int slv_rawequal(slv_value a, slv_value b) {
    if (a.t != b.t) return 0;
    switch (a.t) {
    case SLV_NIL: return 1;
    case SLV_BOOL: return a.as.b == b.as.b;
    case SLV_INT: return a.as.i == b.as.i;
    case SLV_NUM: return a.as.n == b.as.n;
    case SLV_STR: return a.as.s == b.as.s;
    case SLV_TABLE: return a.as.tab == b.as.tab;
    case SLV_FUNCTION: return a.as.fn == b.as.fn;
    case SLV_CFUNCTION: return a.as.cf.f == b.as.cf.f;
    case SLV_MULTI: return 0;
    }
    return 0;
}

/* `==` with `__eq` dispatch for tables/userdata. */
int slv_equal(slv *L, slv_value a, slv_value b) {
    if (a.t == b.t && (a.t == SLV_TABLE)) {
        slv_table *ma = slv_getmetatable(L, a);
        slv_table *mb = slv_getmetatable(L, b);
        if (ma && mb && slv_rawequal(slv_table_get(L, ma, slv_str("__eq")),
                                      slv_table_get(L, mb, slv_str("__eq")))) {
            slv_value f = slv_table_get(L, ma, slv_str("__eq"));
            slv_value args[2]; args[0] = a; args[1] = b;
            slv_value r = slv_call(L, f, args, 2);
            return slv_truthy(r);
        }
    }
    if (a.t == SLV_INT && b.t == SLV_NUM) return (double)a.as.i == b.as.n;
    if (a.t == SLV_NUM && b.t == SLV_INT) return a.as.n == (double)b.as.i;
    return slv_rawequal(a, b);
}

double slv_tonumber(slv_value v, int *ok) {
    if (v.t == SLV_NUM) { if (ok) *ok = 1; return v.as.n; }
    if (v.t == SLV_INT) { if (ok) *ok = 1; return (double)v.as.i; }
    if (ok) *ok = 0;
    return 0.0;
}

int64_t slv_tointeger(slv_value v, int *ok) {
    if (v.t == SLV_INT) { if (ok) *ok = 1; return v.as.i; }
    if (v.t == SLV_NUM) {
        if (v.as.n >= -9.2233720368547758e18 && v.as.n < 9.2233720368547758e18) {
            if (ok) *ok = 1;
            return (int64_t)v.as.n;
        }
    }
    if (ok) *ok = 0;
    return 0;
}

/* Lua 5.4 renders floats with 14 significant digits and integers in full. */
const char *slv_tostring_intern(slv *L, slv_value v) {
    char buf[64];
    switch (v.t) {
    case SLV_NIL:   return slv_intern(L, "nil", 3);
    case SLV_BOOL:  return slv_intern(L, v.as.b ? "true" : "false", v.as.b ? 4 : 5);
    case SLV_INT:   snprintf(buf, sizeof buf, "%lld", (long long)v.as.i); return slv_intern(L, buf, strlen(buf));
    case SLV_NUM:   snprintf(buf, sizeof buf, "%.14g", v.as.n); return slv_intern(L, buf, strlen(buf));
    case SLV_STR:   return v.as.s;
    case SLV_TABLE: snprintf(buf, sizeof buf, "table: %p", (void *)v.as.tab); return slv_intern(L, buf, strlen(buf));
    case SLV_FUNCTION: snprintf(buf, sizeof buf, "function: %p", (void *)v.as.fn); return slv_intern(L, buf, strlen(buf));
    case SLV_CFUNCTION: snprintf(buf, sizeof buf, "function: %p", (void *)v.as.cf.f); return slv_intern(L, buf, strlen(buf));
    case SLV_MULTI: return slv_intern(L, "(multi)", 8);
    }
    return slv_intern(L, "(?)", 3);
}

/* Compatibility form: a shared buffer. Callers that need two values at once
 * must copy the first (slv_concat does). */
const char *slv_tostring(slv_value v) {
    static char fb[64];
    if (v.t == SLV_STR) return v.as.s;
    if (v.t == SLV_INT) { snprintf(fb, sizeof fb, "%lld", (long long)v.as.i); return fb; }
    if (v.t == SLV_NUM) { snprintf(fb, sizeof fb, "%.14g", v.as.n); return fb; }
    switch (v.t) {
    case SLV_NIL:   return "nil";
    case SLV_BOOL:  return v.as.b ? "true" : "false";
    case SLV_TABLE: snprintf(fb, sizeof fb, "table: %p", (void *)v.as.tab); return fb;
    case SLV_FUNCTION: snprintf(fb, sizeof fb, "function: %p", (void *)v.as.fn); return fb;
    case SLV_CFUNCTION: snprintf(fb, sizeof fb, "function: %p", (void *)v.as.cf.f); return fb;
    default: return "(?)";
    }
}

const char *slv_type_name(slv_type t) {
    switch (t) {
    case SLV_NIL: return "nil";
    case SLV_BOOL: return "boolean";
    case SLV_INT:
    case SLV_NUM: return "number";
    case SLV_STR: return "string";
    case SLV_TABLE: return "table";
    case SLV_FUNCTION:
    case SLV_CFUNCTION: return "function";
    case SLV_MULTI: return "multi";
    }
    return "unknown";
}

/* --------------------------------------------------------- string interning */

static unsigned strhash(const char *s, size_t n) {
    unsigned h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 16777619u; }
    return h;
}

const char *slv_intern(slv *L, const char *bytes, size_t len) {
    if (!L || !L->buckets) return "";
    unsigned h = strhash(bytes, len), b = h % L->nbuckets;
    for (istr *it = L->buckets[b]; it; it = it->next)
        if (it->len == len && memcmp(it->s, bytes, len) == 0) return it->s;
    istr *n = (istr *)slv_arena_alloc(L, sizeof(*n) + len + 1);
    if (!n) return "";
    n->len = len; n->hash = h; n->s = (char *)(n + 1);
    memcpy(n->s, bytes, len); n->s[len] = 0;
    n->next = L->buckets[b]; L->buckets[b] = n;
    return n->s;
}

/* ---------------------------------------------------------------- tables */

static unsigned valhash(slv_value k) {
    if (k.t == SLV_STR) return strhash(k.as.s, strlen(k.as.s));
    if (k.t == SLV_NUM) { double d = k.as.n; return (unsigned)((unsigned long long)(d * 2654435761.0)); }
    if (k.t == SLV_INT) return (unsigned)((unsigned long long)k.as.i * 2654435761u);
    return (unsigned)(uintptr_t)k.as.tab | (unsigned)(uintptr_t)k.as.fn;
}

static int keyeq(slv_value a, slv_value b) {
    if (a.t != b.t) {
        /* 1 and 1.0 are the same key in Lua */
        if ((a.t == SLV_INT && b.t == SLV_NUM) || (a.t == SLV_NUM && b.t == SLV_INT)) {
            double x = (a.t == SLV_INT) ? (double)a.as.i : a.as.n;
            double y = (b.t == SLV_INT) ? (double)b.as.i : b.as.n;
            return x == y;
        }
        return 0;
    }
    if (a.t == SLV_NUM) return a.as.n == b.as.n;
    if (a.t == SLV_INT) return a.as.i == b.as.i;
    /* content equality: callers may hand us a non-interned C string */
    if (a.t == SLV_STR) return strcmp(a.as.s, b.as.s) == 0;
    return slv_rawequal(a, b);
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
    if (k.t == SLV_NIL) { slv_error(L, "table index is nil (line %d)", L->curline); return; }
    if (k.t == SLV_NUM && isnan(k.as.n)) { slv_error(L, "table index is NaN (line %d)", L->curline); return; }
    if (t->count * 2 >= t->cap) tgrow(L, t);
    slv_kv *f = tfind(t, k);
    if (f) { f->v = v; t->seq_dirty = 1; return; }
    unsigned hi = valhash(k) % t->cap, j = hi;
    while (t->e[j].used) j = (j + 1) % t->cap;
    t->e[j].k = k; t->e[j].v = v; t->e[j].used = 1; t->count++;
    t->seq_dirty = 1;
}

slv_value slv_table_get(slv *L, const slv_table *tt, slv_value k) {
    (void)L;
    slv_kv *f = tfind((slv_table *)tt, k);
    return f ? f->v : slv_nil();
}

/* `#t`: the array-part border. Lua's rule is any n with t[n] ~= nil and
 * t[n+1] == nil; for a sequence that is the length. */
size_t slv_table_len(const slv_table *tt) {
    slv_table *t = (slv_table *)tt;
    if (!t) return 0;
    if (!t->seq_dirty) return t->seq;
    size_t n = 0;
    if (t->cap) {
        /* linear walk from the current hint; good for the common sequence */
        while (n + 1 < t->cap * 2) {
            slv_value k; k.t = SLV_INT; k.as.i = (int64_t)(n + 1);
            if (tfind(t, k) == NULL) break;
            n++;
            if (n > t->count) break;
        }
    }
    t->seq = n; t->seq_dirty = 0;
    return n;
}

/* `next` support: iterate the backing array. */
int slv_table_next(slv_table *t, slv_value *k, slv_value *v) {
    if (!t || !t->cap) return 0;
    static size_t slot;   /* single-step iterator; fine for non-reentrant use */
    if (k && k->t != SLV_NIL) {
        slv_kv *f = tfind(t, *k);
        if (!f) return 0;
        slot = (size_t)(f - t->e) + 1;
    } else slot = 0;
    for (size_t i = slot; i < t->cap; i++) {
        if (t->e[i].used) { if (k) *k = t->e[i].k; if (v) *v = t->e[i].v; return 1; }
    }
    return 0;
}

/* ------------------------------------------------------------- metatables */

slv_table *slv_getmetatable(slv *L, slv_value v) {
    (void)L;
    if (v.t == SLV_TABLE) return v.as.tab->meta;
    if (v.t == SLV_STR) return NULL;   /* the string metatable is per-state */
    return NULL;
}

void slv_setmetatable(slv *L, slv_table *t, slv_table *meta) {
    (void)L;
    if (t) t->meta = meta;
}

int slv_has_meta(slv *L, slv_value v, const char *event) {
    slv_table *m = slv_getmetatable(L, v);
    if (!m) return 0;
    return slv_table_get(L, m, slv_str(event)).t != SLV_NIL;
}
