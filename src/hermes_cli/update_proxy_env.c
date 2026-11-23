/*
 * update_proxy_env.c — C port of upstream Hermes hermes_cli/update_proxy_env.py.
 *
 * PoP: w15_updateprox_a557fe_no_proxy_entries @ hermes_cli/update_proxy_env.py:agent.proxy_bypass.no_proxy_entries
 * PoP: w15_updateprox_a557fe_restore_user_proxy_env @ hermes_cli/update_proxy_env.py:restore_user_proxy_env
 * PoP: w15_updateprox_a557fe_prefer_dotenv_proxy_env @ hermes_cli/update_proxy_env.py:prefer_dotenv_proxy_env
 *
 * The two environment dependencies of the live module (dotenv_published_values
 * and no_proxy_entries) are injected explicitly: `published` is a JSON object
 * mapping a variable name (matched case-insensitively, as the live helper does)
 * to the array of values dotenv last published for it. No process environment
 * is read or mutated.
 */
#include "noc_w15_updateprox_a557fe.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MARKER "HERMES_UPDATE_SYSTEM_PROXY"
#define SAVED_PREFIX "HERMES_UPDATE_USER_"

static const char *const ADDED_KEYS[] = {
    "HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy", "NO_PROXY", "no_proxy"
};
static const char *const NO_PROXY_KEYS[] = { "NO_PROXY", "no_proxy" };

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int in_added_keys(const char *k) {
    for (size_t i = 0; i < sizeof(ADDED_KEYS) / sizeof(ADDED_KEYS[0]); i++)
        if (strcmp(k, ADDED_KEYS[i]) == 0) return 1;
    return 0;
}

/* ---- tiny string vector ------------------------------------------------ */
typedef struct { char **v; size_t n, cap; } StrVec;

static void sv_free(StrVec *sv) {
    for (size_t i = 0; i < sv->n; i++) free(sv->v[i]);
    free(sv->v);
    sv->v = NULL; sv->n = sv->cap = 0;
}
static void sv_pushn(StrVec *sv, const char *s, size_t n) {
    if (sv->n == sv->cap) {
        size_t nc = sv->cap ? sv->cap * 2 : 8;
        char **nv = (char **)realloc(sv->v, nc * sizeof(char *));
        if (!nv) return;
        sv->v = nv; sv->cap = nc;
    }
    char *copy = (char *)malloc(n + 1);
    if (!copy) return;
    memcpy(copy, s, n);
    copy[n] = '\0';
    sv->v[sv->n++] = copy;
}
static void sv_push(StrVec *sv, const char *s) { sv_pushn(sv, s, strlen(s)); }
static int sv_has(const StrVec *sv, const char *s) {
    for (size_t i = 0; i < sv->n; i++)
        if (sv->v[i] && strcmp(sv->v[i], s) == 0) return 1;
    return 0;
}

/* Python: [part for part in re.split(r"[\s,]+", value.strip()) if part] */
static void split_entries(const char *value, StrVec *out) {
    if (!value) return;
    const char *p = value;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *e = p + strlen(p);
    while (e > p && isspace((unsigned char)e[-1])) e--;
    while (p < e) {
        while (p < e && (isspace((unsigned char)*p) || *p == ',')) p++;
        const char *start = p;
        while (p < e && !isspace((unsigned char)*p) && *p != ',') p++;
        if (p > start) sv_pushn(out, start, (size_t)(p - start));
    }
}

struct json_t *w15_updateprox_a557fe_no_proxy_entries(const char *value) {
    StrVec sv = {0};
    split_entries(value, &sv);
    struct json_t *arr = json_array();
    for (size_t i = 0; i < sv.n; i++) json_append(arr, json_string(sv.v[i]));
    sv_free(&sv);
    return arr;
}

/* dotenv_published_values(name) seam: union of all published arrays whose key
 * matches name case-insensitively. */
static int published_has_value(const struct json_t *published, const char *name, const char *value) {
    if (!json_is_object(published) || !value) return 0;
    for (size_t i = 0; i < json_object_size(published); i++) {
        const char *k = json_object_get_key_at(published, i);
        if (!k || !ieq(k, name)) continue;
        struct json_t *vals = json_object_get_at(published, i);
        if (!json_is_array(vals)) continue;
        for (size_t j = 0; j < json_len(vals); j++) {
            struct json_t *v = json_get(vals, j);
            if (v && json_is_string(v) && strcmp(v->str_val, value) == 0) return 1;
        }
    }
    return 0;
}
static int published_nonempty(const struct json_t *published, const char *name) {
    if (!json_is_object(published)) return 0;
    for (size_t i = 0; i < json_object_size(published); i++) {
        const char *k = json_object_get_key_at(published, i);
        if (!k || !ieq(k, name)) continue;
        struct json_t *vals = json_object_get_at(published, i);
        if (json_is_array(vals) && json_len(vals) > 0) return 1;
    }
    return 0;
}
static int users_dotenv_value(struct json_t *env, const struct json_t *published, const char *key) {
    struct json_t *v = json_obj_get(env, key);
    if (!v || !json_is_string(v)) return 0;
    return published_has_value(published, key, v->str_val);
}

struct json_t *w15_updateprox_a557fe_restore_user_proxy_env(struct json_t *env,
                                                           const struct json_t *published) {
    if (!json_is_object(env)) return env;
    char *marker = dupstr(json_get_str(env, MARKER, ""));
    if (marker) {
        /* Python: marker.split(",") -- no trimming of the key. */
        const char *p = marker;
        while (1) {
            const char *comma = strchr(p, ',');
            char *key = comma ? (char *)malloc((size_t)(comma - p) + 1) : dupstr(p);
            if (key) {
                if (comma) { memcpy(key, p, (size_t)(comma - p)); key[comma - p] = '\0'; }
                if (in_added_keys(key) && !users_dotenv_value(env, published, key))
                    json_obj_del(env, key);
                free(key);
            }
            if (!comma) break;
            p = comma + 1;
        }
        free(marker);
    }
    for (size_t i = 0; i < 2; i++) {
        const char *nk = NO_PROXY_KEYS[i];
        char saved_key[128];
        snprintf(saved_key, sizeof(saved_key), "%s%s", SAVED_PREFIX, nk);
        struct json_t *saved = json_obj_get(env, saved_key);
        char *saved_copy = (saved && json_is_string(saved)) ? dupstr(saved->str_val) : NULL;
        json_obj_del(env, saved_key); /* frees the node; copy first */
        if (saved_copy && !users_dotenv_value(env, published, nk))
            json_set(env, nk, json_string(saved_copy));
        free(saved_copy);
    }
    json_obj_del(env, MARKER);
    return env;
}

struct json_t *w15_updateprox_a557fe_prefer_dotenv_proxy_env(struct json_t *env,
                                                            const struct json_t *published) {
    if (!json_is_object(env)) return env;
    if (!json_has(env, MARKER)) return env;

    const char *proxy_names[] = {"https_proxy", "http_proxy", "all_proxy"};
    for (int i = 0; i < 3; i++)
        if (published_nonempty(published, proxy_names[i]))
            return w15_updateprox_a557fe_restore_user_proxy_env(env, published);

    char *users = NULL;
    for (size_t i = 0; i < json_object_size(env); i++) {
        const char *k = json_object_get_key_at(env, i);
        if (!k || !ieq(k, "no_proxy")) continue;
        struct json_t *v = json_object_get_at(env, i);
        if (v && json_is_string(v) && published_has_value(published, "no_proxy", v->str_val)) {
            users = dupstr(v->str_val);
            break;
        }
    }
    if (!users) return env;

    StrVec uentries = {0};
    split_entries(users, &uentries);
    int has_star = 0;
    for (size_t i = 0; i < uentries.n; i++)
        if (strcmp(uentries.v[i], "*") == 0) { has_star = 1; break; }
    if (has_star) {
        sv_free(&uentries);
        free(users);
        return w15_updateprox_a557fe_restore_user_proxy_env(env, published);
    }

    StrVec own = {0};
    for (size_t i = 0; i < 2; i++) {
        char saved_key[128];
        snprintf(saved_key, sizeof(saved_key), "%s%s", SAVED_PREFIX, NO_PROXY_KEYS[i]);
        split_entries(json_get_str(env, saved_key, ""), &own);
    }
    StrVec added = {0};
    for (size_t i = 0; i < 2; i++) {
        const char *nk = NO_PROXY_KEYS[i];
        struct json_t *v = json_obj_get(env, nk);
        if (!v || !json_is_string(v)) continue;          /* absent: default users -> equal */
        if (strcmp(v->str_val, users) == 0) continue;     /* env.get(key, users) != users */
        StrVec cur = {0};
        split_entries(v->str_val, &cur);
        for (size_t j = 0; j < cur.n; j++)
            if (!sv_has(&own, cur.v[j])) sv_push(&added, cur.v[j]);
        sv_free(&cur);
    }

    StrVec merged = {0};
    for (size_t i = 0; i < uentries.n; i++) if (!sv_has(&merged, uentries.v[i])) sv_push(&merged, uentries.v[i]);
    for (size_t i = 0; i < added.n; i++)   if (!sv_has(&merged, added.v[i]))   sv_push(&merged, added.v[i]);

    size_t total = 1;
    for (size_t i = 0; i < merged.n; i++) total += strlen(merged.v[i]) + 1;
    char *joined = (char *)malloc(total);
    if (joined) {
        joined[0] = '\0';
        for (size_t i = 0; i < merged.n; i++) {
            if (i) strcat(joined, ",");
            strcat(joined, merged.v[i]);
        }
        for (size_t i = 0; i < 2; i++) {
            const char *nk = NO_PROXY_KEYS[i];
            if (json_has(env, nk)) json_set(env, nk, json_string(joined));
        }
        free(joined);
    }

    sv_free(&merged); sv_free(&added); sv_free(&own); sv_free(&uentries); free(users);
    return env;
}
