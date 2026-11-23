/*
 * local_create.c — C port of upstream Hermes session_rules/local_create.py.
 *
 * PoP: w14_localcreat_5396fc_is_secret_env_name @ session_rules/local_create.py:is_secret_env_name
 * PoP: w14_localcreat_5396fc_travels @ session_rules/local_create.py:_travels
 * PoP: w14_localcreat_5396fc_kept @ session_rules/local_create.py:_kept
 * PoP: w14_localcreat_5396fc_normalise_client_env @ session_rules/local_create.py:normalise_client_env
 * PoP: w14_localcreat_5396fc_client_request @ session_rules/local_create.py:client_request
 * PoP: w14_localcreat_5396fc_same_create_request @ session_rules/local_create.py:same_create_request
 * PoP: w14_localcreat_5396fc_retry_matches @ session_rules/local_create.py:retry_matches
 *
 * Create-retry rules. Python ValueError from normalise_client_env is surfaced
 * as {"error": "<message>"}; bool-returning helpers stay bool.
 */
#include "noc_w14_localcreat_5396fc.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAMES 256
#define MAX_BYTES (256 * 1024)

static const char *const DEFAULT_SECRET[] = {
    "KEY","TOKEN","SECRET","PASSWORD","CREDENTIAL","PASSWD","AUTH","DSN",
    "WEBHOOK","CREDS","BEARER","APIKEY","COOKIE",NULL};

static const char *lc_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static json_t *lc_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "error", json_string(msg));
    return o;
}
static json_t *lc_b(int v) { return json_bool(v ? 1 : 0); }
static int lc_isnull(const json_t *v) { return !v || v->type == JSON_NULL; }
static char *lc_upper(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (size_t i = 0; i < n; i++) p[i] = (char)toupper((unsigned char)s[i]);
    p[n] = '\0';
    return p;
}
static size_t lc_cplen(const char *s) {
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}
static int lc_in_strings(const json_t *arr, const char *s) {
    if (!s || !json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = lc_jstr(json_get(arr, i));
        if (v && !strcmp(v, s)) return 1;
    }
    return 0;
}
static int lc_hidden(const char *name, const json_t *secret) {
    char *up = lc_upper(name);
    if (!up) return 0;
    int found = 0;
    if (secret && secret->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(secret); i++) {
            const char *s = lc_jstr(json_get(secret, i));
            if (s && *s && strstr(up, s)) { found = 1; break; }
        }
    } else {
        for (int i = 0; DEFAULT_SECRET[i]; i++)
            if (strstr(up, DEFAULT_SECRET[i])) { found = 1; break; }
    }
    free(up);
    return found;
}

json_t *w14_localcreat_5396fc_is_secret_env_name(const struct json_t *name) {
    const char *n = lc_jstr(name);
    return lc_b(n && lc_hidden(n, NULL));
}
json_t *w14_localcreat_5396fc_travels(const struct json_t *name, const struct json_t *secret) {
    const char *n = lc_jstr(name);
    if (!n) return lc_b(0);
    int hidden = lc_hidden(n, (secret && secret->type == JSON_ARRAY) ? secret : NULL);
    return lc_b(!hidden && strncmp(n, "HERMES_", 7) != 0);
}
json_t *w14_localcreat_5396fc_kept(const struct json_t *name, const struct json_t *rules) {
    const char *n = lc_jstr(name);
    if (!n || !json_is_object(rules)) return lc_b(0);
    if (lc_in_strings(json_obj_get(rules, "behaviour"), n)) return lc_b(1);
    const json_t *secret = json_obj_get(rules, "secret");
    if (lc_hidden(n, (secret && secret->type == JSON_ARRAY) ? secret : NULL)) return lc_b(0);
    if (lc_in_strings(json_obj_get(rules, "names"), n)) return lc_b(1);
    const json_t *pref = json_obj_get(rules, "prefixes");
    if (json_is_array(pref)) {
        for (size_t i = 0; i < json_len(pref); i++) {
            const char *p = lc_jstr(json_get(pref, i));
            if (p && !strncmp(n, p, strlen(p))) return lc_b(1);
        }
    }
    return lc_b(0);
}

static int lc_cmp_key(const void *a, const void *b) {
    const char *const *ka = (const char *const *)a;
    const char *const *kb = (const char *const *)b;
    return strcmp(*ka, *kb);
}
json_t *w14_localcreat_5396fc_normalise_client_env(const struct json_t *value, const struct json_t *rules) {
    if (!json_is_object(value)) return lc_err("client_env must be a map of variable names to string values");
    size_t n = json_object_size(value);
    if (n > MAX_NAMES) return lc_err("client_env must be a map of variable names to string values");
    size_t total = 0;
    for (size_t i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(value, i);
        const json_t *v = json_object_get_at(value, i);
        if (!k || !v || v->type != JSON_STRING)
            return lc_err("client_env must be a map of variable names to string values");
        total += lc_cplen(k) + lc_cplen(v->str_val);
    }
    if (total > MAX_BYTES) return lc_err("client_env must be a map of variable names to string values");
    const char **keys = (const char **)malloc((n ? n : 1) * sizeof(char *));
    if (!keys) return lc_err("out of memory");
    for (size_t i = 0; i < n; i++) keys[i] = json_object_get_key_at(value, i);
    qsort(keys, n, sizeof(char *), lc_cmp_key);
    int unfiltered = lc_isnull(rules);
    json_t *out = json_object();
    for (size_t i = 0; i < n; i++) {
        if (unfiltered || w14_localcreat_5396fc_kept(json_string(keys[i]), rules)->bool_val) {
            json_set(out, keys[i], json_copy(json_obj_get(value, keys[i])));
        }
    }
    free(keys);
    return out;
}

/* deep JSON equality, order-independent for objects */
static int lc_eq(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL: return 1;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); i++) if (!lc_eq(json_get(a,i), json_get(b,i))) return 0;
            return 1;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (size_t i = 0; i < json_object_size(a); i++) {
                const char *k = json_object_get_key_at(a, i);
                const json_t *vb = json_obj_get(b, k);
                if (!vb || !lc_eq(json_object_get_at(a,i), vb)) return 0;
            }
            return 1;
    }
    return 0;
}
static json_t *lc_client_request(const char *request_json, const json_t *derived) {
    json_t *req = json_parse(request_json ? request_json : "", NULL);
    if (!json_is_object(req)) { json_free(req); return json_null(); }
    json_obj_del(req, "desktop_local");
    int dnull = lc_isnull(derived);
    if (dnull) json_obj_del(req, "route_derived");
    if (json_obj_get(req, "model_requested")) {
        json_obj_del(req, "model");
        if (dnull) {
            json_obj_del(req, "base_url");
        } else if (derived->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(derived); i++) {
                const char *k = lc_jstr(json_get(derived, i));
                if (k) json_obj_del(req, k);
            }
        }
    }
    return req;
}
json_t *w14_localcreat_5396fc_client_request(const struct json_t *request_json, const struct json_t *derived) {
    return lc_client_request(lc_jstr(request_json), derived);
}
json_t *w14_localcreat_5396fc_same_create_request(const struct json_t *saved_json, const struct json_t *retry_json) {
    const char *sj = lc_jstr(saved_json), *rj = lc_jstr(retry_json);
    json_t *saved = json_parse(sj ? sj : "", NULL);
    json_t *derived = json_is_object(saved) ? json_obj_get(saved, "route_derived") : NULL;
    json_t *a = lc_client_request(sj ? sj : "", derived);
    json_t *b = lc_client_request(rj ? rj : "", derived);
    if (lc_eq(a, b)) { json_free(a); json_free(b); json_free(saved); return lc_b(1); }
    json_free(a); json_free(b);
    json_t *retry = json_parse(rj ? rj : "", NULL);
    if (lc_eq(json_obj_get(retry, "model_requested"), json_obj_get(retry, "model"))) {
        json_obj_del(retry, "model_requested");
        json_obj_del(retry, "route_derived");
    }
    int res = lc_eq(saved, retry);
    json_free(retry); json_free(saved);
    return lc_b(res);
}
json_t *w14_localcreat_5396fc_retry_matches(const struct json_t *committed, const struct json_t *request) {
    const json_t *stored = json_is_object(committed) ? json_obj_get(committed, "client_env_json") : NULL;
    int has_ce = json_is_object(request) && json_obj_get(request, "client_env") != NULL;
    if (lc_isnull(stored) || !has_ce) return lc_b(lc_isnull(stored) && !has_ce);
    const json_t *rulesv = json_obj_get(committed, "client_env_rules_json");
    json_t *rules = NULL;
    if (!lc_isnull(rulesv)) rules = json_parse(lc_jstr(rulesv) ? lc_jstr(rulesv) : "", NULL);
    json_t *sent = w14_localcreat_5396fc_normalise_client_env(json_obj_get(request, "client_env"), rules);
    json_free(rules);
    if (sent->type == JSON_OBJECT && json_object_size(sent) == 1 && json_obj_get(sent, "error")) {
        json_free(sent);
        return lc_b(0);
    }
    json_t *stored_obj = json_parse(lc_jstr(stored), NULL);
    int res = lc_eq(sent, stored_obj);
    json_free(sent); json_free(stored_obj);
    return lc_b(res);
}
