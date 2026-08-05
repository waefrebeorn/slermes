/*
 * omap_test.c — correctness test for lib/libomap (Python `dict` semantics).
 *
 * Every expectation below was derived from a live CPython run, not eyeballed:
 * insertion order, replace-keeps-position, pop/del, setdefault, and churn
 * (which is what exercises tombstone compaction).
 *
 * Build/run:  make omap-test
 */

#include "omap.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strdup is POSIX; the test builds with a bare -std=c11 like the library. */
static char *tdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int g_pass = 0, g_fail = 0;

static void ok(int cond, const char *what)
{
    if (cond) { g_pass++; }
    else { g_fail++; printf("  FAIL: %s\n", what); }
}

static void eq_str(const char *got, const char *want, const char *what)
{
    int c = (got == want) || (got && want && strcmp(got, want) == 0);
    if (!c) printf("  FAIL: %s (got '%s', want '%s')\n",
                   what, got ? got : "(null)", want ? want : "(null)");
    if (c) g_pass++; else g_fail++;
}

/* Join the live keys in order: "a,b,c" — the direct analogue of
 * ",".join(d) in Python. */
static void order_str(const omap_t *m, char *buf, size_t sz)
{
    buf[0] = '\0';
    const char *k;
    for (size_t i = 0; omap_at(m, i, &k, NULL); i++) {
        if (i) strncat(buf, ",", sz - strlen(buf) - 1);
        strncat(buf, k, sz - strlen(buf) - 1);
    }
}

static int g_freed = 0;
static void counting_free(void *v) { g_freed++; free(v); }

int main(void)
{
    char buf[512];

    /* ── 1. insertion order is preserved ──────────────────────────────── */
    omap_t *m = omap_new(NULL);
    ok(m != NULL, "omap_new");
    ok(omap_empty(m), "new map is empty");
    ok(omap_size(m) == 0, "new map size 0");

    omap_set(m, "alpha", (void *)1);
    omap_set(m, "beta",  (void *)2);
    omap_set(m, "gamma", (void *)3);
    ok(omap_size(m) == 3, "size after 3 inserts");
    order_str(m, buf, sizeof buf);
    /* python: list({'alpha':1,'beta':2,'gamma':3}) */
    eq_str(buf, "alpha,beta,gamma", "insertion order");

    /* ── 2. lookup ────────────────────────────────────────────────────── */
    ok(omap_get(m, "beta") == (void *)2, "get beta");
    ok(omap_get(m, "missing") == NULL, "get missing -> NULL");
    ok(omap_contains(m, "alpha"), "contains alpha");
    ok(!omap_contains(m, "missing"), "not contains missing");

    /* ── 3. replace KEEPS the original position (Python semantics) ────── */
    omap_set(m, "alpha", (void *)99);
    ok(omap_size(m) == 3, "size unchanged on replace");
    ok(omap_get(m, "alpha") == (void *)99, "replaced value");
    order_str(m, buf, sizeof buf);
    /* python: d['alpha']=99 -> list(d) is still ['alpha','beta','gamma'] */
    eq_str(buf, "alpha,beta,gamma", "replace keeps position");

    /* ── 4. pop / erase leave the remaining order intact ──────────────── */
    void *popped = omap_pop(m, "beta");
    ok(popped == (void *)2, "pop returns value");
    ok(omap_size(m) == 2, "size after pop");
    ok(!omap_contains(m, "beta"), "popped key gone");
    order_str(m, buf, sizeof buf);
    /* python: del d['beta'] -> ['alpha','gamma'] */
    eq_str(buf, "alpha,gamma", "order after pop");
    ok(omap_pop(m, "beta") == NULL, "pop absent -> NULL");

    /* re-inserting a popped key appends at the END (Python semantics) */
    omap_set(m, "beta", (void *)7);
    order_str(m, buf, sizeof buf);
    /* python: d['beta']=7 after del -> ['alpha','gamma','beta'] */
    eq_str(buf, "alpha,gamma,beta", "re-insert appends at end");

    /* ── 5. setdefault ────────────────────────────────────────────────── */
    void *out = NULL;
    omap_setdefault(m, "alpha", (void *)555, &out);
    ok(out == (void *)99, "setdefault existing returns existing");
    ok(omap_get(m, "alpha") == (void *)99, "setdefault did not overwrite");
    omap_setdefault(m, "delta", (void *)4, &out);
    ok(out == (void *)4, "setdefault absent returns inserted");
    ok(omap_size(m) == 4, "size after setdefault insert");
    order_str(m, buf, sizeof buf);
    eq_str(buf, "alpha,gamma,beta,delta", "setdefault appends");

    /* ── 6. keys snapshot ─────────────────────────────────────────────── */
    size_t n = 0;
    const char **keys = omap_keys(m, &n);
    ok(n == 4, "omap_keys count");
    ok(keys && strcmp(keys[0], "alpha") == 0, "keys[0]");
    ok(keys && strcmp(keys[3], "delta") == 0, "keys[3]");
    free((void *)keys);

    /* ── 7. NULL value is storable and distinct from absent ───────────── */
    omap_set(m, "nullval", NULL);
    ok(omap_get(m, "nullval") == NULL, "stored NULL reads back NULL");
    ok(omap_contains(m, "nullval"), "contains distinguishes NULL from absent");

    omap_clear(m);
    ok(omap_size(m) == 0, "clear -> size 0");
    ok(omap_empty(m), "clear -> empty");
    omap_free(m);

    /* ── 8. value destructor runs on erase/clear, NOT on pop ──────────── */
    g_freed = 0;
    omap_t *o = omap_new(counting_free);
    omap_set(o, "a", tdup("one"));
    omap_set(o, "b", tdup("two"));
    omap_set(o, "c", tdup("three"));
    /* replace frees the displaced value */
    omap_set(o, "a", tdup("uno"));
    ok(g_freed == 1, "replace freed displaced value");
    eq_str((const char *)omap_get(o, "a"), "uno", "replaced string value");
    /* pop hands ownership back to the caller: no destructor call */
    char *taken = (char *)omap_pop(o, "b");
    ok(g_freed == 1, "pop does NOT free");
    eq_str(taken, "two", "popped string");
    free(taken);
    /* erase does free */
    ok(omap_erase(o, "c"), "erase present -> true");
    ok(g_freed == 2, "erase freed value");
    ok(!omap_erase(o, "c"), "erase absent -> false");
    omap_free(o);
    ok(g_freed == 3, "free released remaining value");

    /* ── 9. churn: tombstone compaction keeps order + lookups correct ── */
    omap_t *c = omap_new(NULL);
    char key[32];
    for (int i = 0; i < 2000; i++) {
        snprintf(key, sizeof key, "k%d", i);
        omap_set(c, key, (void *)(intptr_t)(i + 1));
    }
    ok(omap_size(c) == 2000, "churn: 2000 inserted");
    for (int i = 0; i < 2000; i += 2) {          /* erase the evens */
        snprintf(key, sizeof key, "k%d", i);
        omap_pop(c, key);
    }
    ok(omap_size(c) == 1000, "churn: 1000 remain");
    for (int i = 0; i < 2000; i++) {              /* re-add the evens */
        if (i % 2) continue;
        snprintf(key, sizeof key, "k%d", i);
        omap_set(c, key, (void *)(intptr_t)(i + 1));
    }
    ok(omap_size(c) == 2000, "churn: back to 2000");
    int all = 1;
    for (int i = 0; i < 2000; i++) {
        snprintf(key, sizeof key, "k%d", i);
        if (omap_get(c, key) != (void *)(intptr_t)(i + 1)) { all = 0; break; }
    }
    ok(all, "churn: every value still correct after compaction");
    /* surviving odds kept their original order, re-added evens follow */
    const char *k0 = NULL, *k999 = NULL, *k1000 = NULL;
    omap_at(c, 0, &k0, NULL);
    omap_at(c, 999, &k999, NULL);
    omap_at(c, 1000, &k1000, NULL);
    eq_str(k0, "k1", "churn: first key is the oldest survivor");
    eq_str(k999, "k1999", "churn: last survivor before re-adds");
    eq_str(k1000, "k0", "churn: re-added keys follow survivors");
    ok(!omap_at(c, 2000, NULL, NULL), "omap_at past end -> false");
    omap_free(c);

    /* ── 10. NULL-safety (ports call these on optional state) ─────────── */
    ok(omap_size(NULL) == 0, "size(NULL)");
    ok(omap_empty(NULL), "empty(NULL)");
    ok(omap_get(NULL, "x") == NULL, "get(NULL)");
    ok(!omap_contains(NULL, "x"), "contains(NULL)");
    ok(omap_pop(NULL, "x") == NULL, "pop(NULL)");
    ok(!omap_erase(NULL, "x"), "erase(NULL)");
    ok(!omap_at(NULL, 0, NULL, NULL), "at(NULL)");
    omap_free(NULL);   /* must not crash */

    printf("omap_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
