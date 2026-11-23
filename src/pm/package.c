/*
 * package.c — C port of upstream Hermes pm/package.py (gate w5_package_78a049).
 *
 * PoP: w5_package_78a049_compose_env    @ pm/package.py:compose_env
 * PoP: w5_package_78a049_install_error  @ pm/package.py:InstallError.__init__
 * PoP: w5_package_78a049_store_entry    @ pm/package.py:Package.store_entry
 * PoP: w5_package_78a049_fetch_url      @ pm/package.py:Package.fetch_url
 * PoP: w5_package_78a049_is_optional    @ pm/package.py:Package.is_optional
 * PoP: w5_package_78a049_missing_reason @ pm/package.py:Package.missing_reason
 *
 * compose_env is the substantive port: deps-first diffs, PATH prepend/dedup,
 * and the ordinary (non extended-length) spelling for values leaving PM.
 */
#include "noc_w5_package_78a049.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENV 512
#define MAX_DIRS 512

void w5_package_78a049_free(char *s) { free(s); }

static char *dupn(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}
static char *dupstr(const char *s) { return dupn(s, s ? strlen(s) : 0); }

/* pm.filesystem.native(): strip the extended-length prefix for text that
 * leaves PM's own file calls. */
static const char *native(const char *s) {
    static const char V[] = "\\\\?\\";
    static const char VU[] = "\\\\?\\UNC\\";
    size_t lu = strlen(VU), lv = strlen(V);
    if (strncmp(s, VU, lu) == 0) {
        const char *rest = s + lu;
        char *out = (char *)malloc(3 + strlen(rest) + 1);
        if (!out) return s;
        out[0] = '\\'; out[1] = '\\';
        strcpy(out + 2, rest);
        return out;
    }
    if (strncmp(s, V, lv) == 0) return s + lv;
    return s;
}

/* str.upper()==\"PATH\" comparison (ASCII, enough for the env key domain). */
static int key_is_path(const char *k) {
    size_t n = strlen(k);
    if (n != 4) return 0;
    return (k[0] | 32) == 'p' && (k[1] | 32) == 'a'
        && (k[2] | 32) == 't' && (k[3] | 32) == 'h';
}

json_t *w5_package_78a049_compose_env(const w5_package_78a049_diff_t *diffs,
                                      size_t n_diffs,
                                      const w5_package_78a049_kv_t *base,
                                      size_t n_base) {
    const char *keys[MAX_ENV];
    const char *vals[MAX_ENV];
    size_t ne = 0;

    /* env = dict(base) */
    for (size_t i = 0; i < n_base && ne < MAX_ENV; i++) {
        int found = 0;
        for (size_t j = 0; j < ne; j++)
            if (strcmp(keys[j], base[i].key) == 0) { vals[j] = base[i].value; found = 1; break; }
        if (!found) { keys[ne] = base[i].key; vals[ne] = base[i].value; ne++; }
    }

    /* PATH dirs: reversed(diffs), dedup first occurrence wins. */
    const char *dirs[MAX_DIRS];
    size_t nd = 0;
    for (size_t di = n_diffs; di > 0; di--) {
        const w5_package_78a049_diff_t *d = &diffs[di - 1];
        for (size_t it = 0; it < d->n_items; it++) {
            const w5_package_78a049_diff_item_t *x = &d->items[it];
            if (strcmp(x->key, "PATH") != 0) continue;
            size_t cnt = x->is_list ? x->n_items : 1;
            for (size_t q = 0; q < cnt && nd < MAX_DIRS; q++) {
                const char *raw = x->is_list ? x->items[q] : x->value;
                const char *nv = native(raw ? raw : "");
                int seen = 0;
                for (size_t k = 0; k < nd; k++) if (strcmp(dirs[k], nv) == 0) { seen = 1; break; }
                if (!seen) dirs[nd++] = nv;
            }
        }
    }

    /* Non-PATH keys: deps-first, later wins. */
    for (size_t di = 0; di < n_diffs; di++) {
        const w5_package_78a049_diff_t *d = &diffs[di];
        for (size_t it = 0; it < d->n_items; it++) {
            const w5_package_78a049_diff_item_t *x = &d->items[it];
            if (strcmp(x->key, "PATH") == 0) continue;
            const char *nv = native(strcmp(x->key, "PATH") == 0 ? "" : (x->value ? x->value : ""));
            int found = 0;
            for (size_t j = 0; j < ne; j++)
                if (strcmp(keys[j], x->key) == 0) { vals[j] = nv; found = 1; break; }
            if (!found && ne < MAX_ENV) { keys[ne] = x->key; vals[ne] = nv; ne++; }
        }
    }

    if (nd) {
        size_t target = ne;
        for (size_t j = 0; j < ne; j++) if (key_is_path(keys[j])) { target = j; break; }
        const char *key = (target < ne) ? keys[target] : "PATH";
        const char *existing = (target < ne) ? vals[target] : "";
        size_t prefix_n = 0;
        for (size_t k = 0; k < nd; k++) prefix_n += strlen(dirs[k]);
        prefix_n += (nd - 1); /* separators */
        char *prefix = (char *)malloc(prefix_n + 1);
        if (prefix) {
            size_t p = 0;
            for (size_t k = 0; k < nd; k++) {
                if (k) prefix[p++] = ';';
                size_t l = strlen(dirs[k]);
                memcpy(prefix + p, dirs[k], l); p += l;
            }
            prefix[p] = '\0';
            char *joined;
            if (existing && *existing) {
                joined = (char *)malloc(p + 1 + strlen(existing) + 1);
                if (joined) { memcpy(joined, prefix, p); joined[p] = ';'; strcpy(joined + p + 1, existing); }
            } else joined = dupstr(prefix);
            if (target < ne) vals[target] = joined ? joined : vals[target];
            else if (ne < MAX_ENV) { keys[ne] = key; vals[ne] = joined ? joined : ""; ne++; }
            free(prefix);
        }
    }

    json_t *out = json_object();
    for (size_t j = 0; j < ne; j++)
        json_set(out, keys[j], json_string(vals[j] ? vals[j] : ""));
    return out;
}

char *w5_package_78a049_install_error(const char *package, const char *cause,
                                      const char *remedy) {
    if (!remedy || !*remedy) remedy = "retry, or run `hermes pm doctor`";
    size_t n = strlen(package ? package : "") + strlen(cause ? cause : "")
             + strlen(remedy) + 32;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s: %s \xe2\x80\x94 %s", package ? package : "",
             cause ? cause : "", remedy);
    return out;
}

char *w5_package_78a049_store_entry(const char *name, const char *version,
                                    const char *target) {
    size_t n = strlen(name ? name : "") + strlen(version ? version : "")
             + strlen(target ? target : "") + 3;
    char *out = (char *)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "%s-%s-%s", name ? name : "", version ? version : "", target ? target : "");
    return out;
}

char *w5_package_78a049_fetch_url(const char *url, const char *version,
                                  const char *target) {
    const char *tpl = url ? url : "";
    const char *v = version ? version : "";
    const char *g = target ? target : "";
    size_t cap = strlen(tpl) + strlen(v) * 4 + strlen(g) * 4 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t n = 0;
    for (const char *p = tpl; *p; ) {
        if (!strncmp(p, "{version}", 9)) {
            memcpy(out + n, v, strlen(v)); n += strlen(v); p += 9;
        } else if (!strncmp(p, "{target}", 8)) {
            memcpy(out + n, g, strlen(g)); n += strlen(g); p += 8;
        } else out[n++] = *p++;
    }
    out[n] = '\0';
    return out;
}

bool w5_package_78a049_is_optional(bool optional,
                                   const char *const *on_demand, size_t n_on_demand,
                                   const char *target) {
    if (optional) return true;
    for (size_t i = 0; i < n_on_demand; i++)
        if (on_demand[i] && target && strcmp(on_demand[i], target) == 0) return true;
    return false;
}

char *w5_package_78a049_missing_reason(const char *const *gap_targets,
                                       const char *const *gap_reasons, size_t n,
                                       const char *target) {
    for (size_t i = 0; i < n; i++)
        if (gap_targets[i] && target && strcmp(gap_targets[i], target) == 0)
            return dupstr(gap_reasons[i] ? gap_reasons[i] : "");
    return NULL;
}
