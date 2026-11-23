/*
 * update.c — C port of upstream Hermes pm/update.py (gate w5_update_6d82c1).
 *
 * PoP: w5_update_6d82c1_version_key @ pm/update.py:version_key
 * PoP: w5_update_6d82c1_minor_of @ pm/update.py:minor_of
 * PoP: w5_update_6d82c1_best_in_minor @ pm/update.py:best_in_minor
 * PoP: w5_update_6d82c1_resolve_best @ pm/update.py:resolve_best
 *
 * The four helpers explicitly documented as pure in the Python module are
 * mirrored here, including tuple ordering and set intersection. Index fetches,
 * artifact pinning and venv refresh stay in Python; candidates are injected.
 */
#include "noc_w5_update_6d82c1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TARGETS 64
#define MAX_PARTS 256

typedef struct { int kind; long long num; char *str; } vpart_t;
typedef struct { vpart_t *items; size_t n, cap; } vkey_t;

static void key_add(vkey_t *k, int kind, long long num, const char *str, size_t len) {
    if (k->n == k->cap) {
        size_t nc = k->cap ? k->cap * 2 : 8;
        vpart_t *ni = (vpart_t *)realloc(k->items, nc * sizeof(vpart_t));
        if (!ni) return;
        k->items = ni; k->cap = nc;
    }
    vpart_t *p = &k->items[k->n++];
    p->kind = kind; p->num = num; p->str = NULL;
    if (kind == 1) {
        p->str = (char *)malloc(len + 1);
        if (p->str) { memcpy(p->str, str, len); p->str[len] = '\0'; }
    }
}

/* _VERSION_PART_RE = re.compile(r"\d+|[A-Za-z]+") over version.replace("-", ".").split(".")
 * Non-alphanumeric characters are skipped by findall, which is the same as
 * scanning the original string for maximal digit / ASCII-letter runs. */
static void key_parse(const char *v, vkey_t *k) {
    memset(k, 0, sizeof(*k));
    if (!v) return;
    for (const char *p = v; *p;) {
        if (isdigit((unsigned char)*p)) {
            const char *s = p;
            while (isdigit((unsigned char)*p)) p++;
            key_add(k, 0, strtoll(s, NULL, 10), NULL, 0);
        } else if (isalpha((unsigned char)*p)) {
            const char *s = p;
            while (isalpha((unsigned char)*p)) p++;
            key_add(k, 1, 0, s, (size_t)(p - s));
        } else {
            p++;
        }
    }
}

static void key_free(vkey_t *k) {
    for (size_t i = 0; i < k->n; i++) free(k->items[i].str);
    free(k->items);
    k->items = NULL; k->n = k->cap = 0;
}

static int key_cmp(const vkey_t *a, const vkey_t *b) {
    size_t n = a->n < b->n ? a->n : b->n;
    for (size_t i = 0; i < n; i++) {
        const vpart_t *x = &a->items[i], *y = &b->items[i];
        if (x->kind != y->kind) return x->kind < y->kind ? -1 : 1;
        if (x->kind == 0) {
            if (x->num != y->num) return x->num < y->num ? -1 : 1;
        } else {
            int c = strcmp(x->str ? x->str : "", y->str ? y->str : "");
            if (c) return c < 0 ? -1 : 1;
        }
    }
    if (a->n == b->n) return 0;
    return a->n < b->n ? -1 : 1;
}

static int versions_cmp(const char *a, const char *b) {
    vkey_t ka, kb;
    key_parse(a, &ka); key_parse(b, &kb);
    int c = key_cmp(&ka, &kb);
    key_free(&ka); key_free(&kb);
    return c;
}

/* PoP: w5_update_6d82c1_version_key @ pm/update.py:version_key */
json_t *w5_update_6d82c1_version_key(const char *version) {
    vkey_t k;
    key_parse(version, &k);
    json_t *out = json_array();
    for (size_t i = 0; i < k.n && i < MAX_PARTS; i++) {
        json_t *pair = json_array();
        json_append(pair, json_number((double)k.items[i].kind));
        if (k.items[i].kind == 0) json_append(pair, json_number((double)k.items[i].num));
        else json_append(pair, json_string(k.items[i].str ? k.items[i].str : ""));
        json_append(out, pair);
    }
    key_free(&k);
    return out;
}

static int minor_of_raw(const char *v, long long *maj, long long *min) {
    long long nums[MAX_PARTS];
    size_t n = 0;
    if (!v) return 0;
    for (const char *p = v; *p && n < MAX_PARTS;) {
        if (isdigit((unsigned char)*p)) {
            const char *s = p;
            while (isdigit((unsigned char)*p)) p++;
            nums[n++] = strtoll(s, NULL, 10);
        } else p++;
    }
    if (n < 2) return 0;
    *maj = nums[0]; *min = nums[1];
    return 1;
}

/* PoP: w5_update_6d82c1_minor_of @ pm/update.py:minor_of */
json_t *w5_update_6d82c1_minor_of(const char *version) {
    long long a = 0, b = 0;
    if (!minor_of_raw(version, &a, &b)) return json_null();
    json_t *out = json_array();
    json_append(out, json_number((double)a));
    json_append(out, json_number((double)b));
    return out;
}

/* PoP: w5_update_6d82c1_best_in_minor @ pm/update.py:best_in_minor */
json_t *w5_update_6d82c1_best_in_minor(json_t *versions, int major, int minor) {
    const char *best = NULL;
    size_t n = json_array_size(versions);
    for (size_t i = 0; i < n; i++) {
        json_t *v = json_get(versions, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (!s) continue;
        long long a = 0, b = 0;
        if (!minor_of_raw(s, &a, &b) || a != major || b != minor) continue;
        if (!best || versions_cmp(s, best) > 0) best = s;
    }
    return best ? json_string(best) : json_null();
}

typedef struct { long long a, b; } minor_t;

static int minor_has(const minor_t *set, size_t n, long long a, long long b) {
    for (size_t i = 0; i < n; i++) if (set[i].a == a && set[i].b == b) return 1;
    return 0;
}
static void minor_add(minor_t *set, size_t *n, long long a, long long b) {
    if (!minor_has(set, *n, a, b)) { set[*n].a = a; set[*n].b = b; (*n)++; }
}
static int str_has(const char *const *set, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (!strcmp(set[i], s)) return 1;
    return 0;
}

/* PoP: w5_update_6d82c1_resolve_best @ pm/update.py:resolve_best */
json_t *w5_update_6d82c1_resolve_best(const char *name, json_t *latest_by_target,
                                      const char *locked, int locked_present,
                                      const char *style)
{
    const char *targets[MAX_TARGETS];
    json_t *lists[MAX_TARGETS];
    size_t nt = 0;
    size_t nobj = json_object_size(latest_by_target);
    for (size_t i = 0; i < nobj && nt < MAX_TARGETS; i++) {
        json_t *v = json_object_get_at(latest_by_target, i);
        if (v && json_array_size(v) > 0) {
            targets[nt] = json_object_get_key_at(latest_by_target, i);
            lists[nt] = v;
            nt++;
        }
    }

    const char *version = NULL;
    char version_buf[64];
    const char *reason = "";
    json_t *per_target = json_object();
    int changed = 0;

    if (nt == 0) {
        reason = "no source";
    } else if (style && !strcmp(style, "minor")) {
        minor_t common[MAX_PARTS];
        size_t nc = 0;
        for (size_t t = 0; t < nt; t++) {
            minor_t cur[MAX_PARTS];
            size_t cc = 0;
            size_t nv = json_array_size(lists[t]);
            for (size_t i = 0; i < nv; i++) {
                json_t *e = json_get(lists[t], i);
                const char *s = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                long long a = 0, b = 0;
                if (s && minor_of_raw(s, &a, &b)) minor_add(cur, &cc, a, b);
            }
            if (t == 0) { for (size_t i = 0; i < cc; i++) common[nc++] = cur[i]; }
            else {
                size_t w = 0;
                for (size_t i = 0; i < nc; i++)
                    if (minor_has(cur, cc, common[i].a, common[i].b)) common[w++] = common[i];
                nc = w;
            }
        }
        if (nc == 0) {
            reason = "no shared minor";
        } else {
            size_t best = 0;
            for (size_t i = 1; i < nc; i++)
                if (common[i].a > common[best].a ||
                    (common[i].a == common[best].a && common[i].b > common[best].b))
                    best = i;
            snprintf(version_buf, sizeof(version_buf), "%lld.%lld",
                     common[best].a, common[best].b);
            version = version_buf;
            for (size_t t = 0; t < nt; t++) {
                json_t *b = w5_update_6d82c1_best_in_minor(
                    lists[t], (int)common[best].a, (int)common[best].b);
                json_set(per_target, targets[t], b);
            }
        }
    } else {
        const char *common[MAX_PARTS];
        size_t nc = 0;
        for (size_t t = 0; t < nt; t++) {
            size_t nv = json_array_size(lists[t]);
            if (t == 0) {
                for (size_t i = 0; i < nv; i++) {
                    json_t *e = json_get(lists[t], i);
                    const char *s = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                    if (s && nc < MAX_PARTS && !str_has(common, nc, s)) common[nc++] = s;
                }
            } else {
                const char *tmp[MAX_PARTS];
                size_t w = 0;
                for (size_t i = 0; i < nc; i++) {
                    int found = 0;
                    for (size_t j = 0; j < nv; j++) {
                        json_t *e = json_get(lists[t], j);
                        const char *s = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                        if (s && !strcmp(s, common[i])) { found = 1; break; }
                    }
                    if (found) tmp[w++] = common[i];
                }
                for (size_t i = 0; i < w; i++) common[i] = tmp[i];
                nc = w;
            }
        }
        if (nc == 0) {
            reason = "no shared version";
        } else {
            size_t best = 0;
            for (size_t i = 1; i < nc; i++)
                if (versions_cmp(common[i], common[best]) > 0) best = i;
            version = common[best];
            for (size_t t = 0; t < nt; t++)
                json_set(per_target, targets[t], json_string(version));
        }
    }

    if (version) {
        if (!locked_present || !locked) {
            changed = 1;
        } else if (style && !strcmp(style, "minor")) {
            long long va = 0, vb = 0, la = 0, lb = 0;
            int vok = minor_of_raw(version, &va, &vb);
            int lok = minor_of_raw(locked, &la, &lb);
            changed = (!vok || !lok || va != la || vb != lb) ? 1 : 0;
        } else {
            changed = versions_cmp(version, locked) > 0;
        }
    }

    json_t *out = json_object();
    json_set(out, "name", json_string(name ? name : ""));
    json_set(out, "locked", locked_present && locked ? json_string(locked) : json_null());
    json_set(out, "style", json_string(style ? style : ""));
    json_set(out, "version", version ? json_string(version) : json_null());
    json_set(out, "per_target", per_target);
    json_set(out, "reason", json_string(reason));
    json_set(out, "artifact_updates", json_object());
    json_set(out, "changed", json_bool(changed));
    return out;
}
