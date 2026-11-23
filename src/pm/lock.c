/*
 * lock.c — real C11 port of pm/lock.py (gate w5_lock_32c99e).
 *
 * PoP: pmlock_templatize @ pm/lock.py:_templatize
 * PoP: pmlock_resolve @ pm/lock.py:_resolve
 * PoP: pmlock_artifacts @ pm/lock.py:Lockfile.artifacts
 * PoP: pmlock_tool_rows @ pm/lock.py:_tool_rows
 *
 * Durable-write / filesystem paths stay out; the state-shape transforms and
 * the malformed-pin validation are ported exactly.
 */
#include "noc_w5_lock_32c99e.h"
#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PMLOCK_STORE_TOKEN "{{store}}"

static char *py_str(const json_t *v) {
    char buf[80];
    if (!v) return strdup("None");
    switch (v->type) {
        case JSON_NULL:   return strdup("None");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER:
            if (v->num_val == floor(v->num_val) && fabs(v->num_val) < 9.0e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else
                snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            return strdup(buf);
        default: return strdup("");
    }
}

static char *replace_all(const char *s, const char *from, const char *to) {
    if (!s) s = "";
    if (!from || !*from) return strdup(s);
    size_t fl = strlen(from), tl = strlen(to);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, from)) != NULL) { count++; p += fl; }
    size_t extra = tl > fl ? count * (tl - fl) : 0;
    char *out = (char *)malloc(strlen(s) + extra + 1);
    if (!out) return NULL;
    char *o = out;
    const char *cur = s;
    while ((p = strstr(cur, from)) != NULL) {
        memcpy(o, cur, (size_t)(p - cur)); o += p - cur;
        memcpy(o, to, tl); o += tl;
        cur = p + fl;
    }
    strcpy(o, cur);
    return out;
}

static void transform_env(const char *env_json, const char *root,
                           const char *from, const char *to, char **out) {
    json_t *env = json_parse(env_json ? env_json : "{}", NULL);
    if (!env || env->type != JSON_OBJECT) { json_free(env); *out = NULL; return; }
    json_t *res = json_object();
    for (size_t i = 0; i < json_object_size(env); i++) {
        const char *k = json_object_get_key_at(env, i);
        json_t *v = json_object_get_at(env, i);
        if (!k) continue;
        if (v && v->type == JSON_ARRAY) {
            json_t *arr = json_array();
            for (size_t j = 0; j < json_len(v); j++) {
                char *sv = py_str(v->c.items ? v->c.items[j] : NULL);
                char *r = replace_all(sv, from, to);
                json_append(arr, json_string(r ? r : ""));
                free(sv); free(r);
            }
            json_set(res, k, arr);
        } else {
            char *sv = py_str(v);
            char *r = replace_all(sv, from, to);
            json_set(res, k, json_string(r ? r : ""));
            free(sv); free(r);
        }
    }
    *out = json_dumps(res, 0);
    json_free(res); json_free(env);
}

char *pmlock_templatize(const char *env_json, const char *store_root) {
    char *out = NULL;
    transform_env(env_json, store_root, store_root, PMLOCK_STORE_TOKEN, &out);
    return out;
}

char *pmlock_resolve(const char *env_json, const char *store_root) {
    char *out = NULL;
    transform_env(env_json, store_root, PMLOCK_STORE_TOKEN, store_root, &out);
    return out;
}

static int is_hex64(const char *s) {
    if (!s || strlen(s) != 64) return 0;
    for (int i = 0; i < 64; i++)
        if (!((s[i] >= 'a' && s[i] <= 'f') || (s[i] >= '0' && s[i] <= '9'))) return 0;
    return 1;
}

int pmlock_artifacts(const char *packages_json, const char *name,
                     const char *target, char **out_json) {
    json_t *pkgs = json_parse(packages_json ? packages_json : "{}", NULL);
    json_t *entries = json_array();
    int rc = 0;
    if (!pkgs || pkgs->type != JSON_OBJECT) goto done;

    json_t *pkg = json_obj_get(pkgs, name ? name : "");
    json_t *arts = NULL;
    if (pkg && pkg->type == JSON_OBJECT) arts = json_obj_get(pkg, "artifacts");
    json_t *found = NULL;
    if (arts && arts->type == JSON_OBJECT) {
        found = json_obj_get(arts, target ? target : "");
        if (!found || found->type == JSON_NULL) found = json_obj_get(arts, "any");
    }
    if (!found || found->type == JSON_NULL) {
        *out_json = json_dumps(entries, 0);
        rc = 1;
        goto done;
    }
    if (found->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(found); i++) json_append(entries, json_copy(found->c.items[i]));
    } else {
        json_append(entries, json_copy(found));
    }

    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *a = json_get(entries, i);
        int ok = (a && a->type == JSON_OBJECT);
        json_t *url = ok ? json_obj_get(a, "url") : NULL;
        if (ok && (!url || url->type != JSON_STRING || !url->str_val || !*url->str_val)) ok = 0;
        int has_sha = ok && json_has(a, "sha256");
        if (ok && !has_sha) {
            int docker_exc = !strcmp(name ? name : "", "termux-docker")
                && url && !strncmp(url->str_val, "docker://", 9);
            if (!docker_exc) ok = 0;
        }
        if (ok && has_sha) {
            json_t *sha = json_obj_get(a, "sha256");
            if (!sha || sha->type != JSON_STRING || !is_hex64(sha->str_val)) ok = 0;
        }
        if (!ok) goto done; /* ValueError -> harness emits null */
    }
    *out_json = json_dumps(entries, 0);
    rc = 1;

done:
    json_free(entries); json_free(pkgs);
    return rc;
}

char *pmlock_tool_rows(const char *fact_json) {
    json_t *fact = json_parse(fact_json ? fact_json : "null", NULL);
    json_t *rows = json_array();
    if (fact && fact->type == JSON_OBJECT && json_has(fact, "entry")) {
        json_append(rows, json_copy(fact));
        json_t *prev = json_obj_get(fact, "previous");
        if (prev && prev->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(prev); i++) {
                json_t *row = prev->c.items[i];
                if (row && row->type == JSON_OBJECT && json_has(row, "entry"))
                    json_append(rows, json_copy(row));
            }
        }
    }
    char *out = json_dumps(rows, 0);
    json_free(rows); json_free(fact);
    return out;
}
