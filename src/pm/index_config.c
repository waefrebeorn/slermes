/*
 * index_config.c — C home for upstream Hermes pm/index_config.py.
 *
 * PoP: w5_indexconfig_5a61c5_is_forwarded @ pm/index_config.py:is_forwarded
 * PoP: w5_indexconfig_5a61c5_is_index_redirect @ pm/index_config.py:is_index_redirect
 * PoP: w5_indexconfig_5a61c5_ini_unsafe @ pm/index_config.py:_ini_unsafe
 * PoP: w5_indexconfig_5a61c5_npmrc_env_replace @ pm/index_config.py:_npmrc_env_replace
 * PoP: w5_indexconfig_5a61c5_npm_registry @ pm/index_config.py:npm_registry
 * PoP: w5_indexconfig_5a61c5_npm_registry_url @ pm/index_config.py:npm_registry_url
 * PoP: w5_indexconfig_5a61c5_bridged_index_settings @ pm/index_config.py:bridged_index_settings
 *
 * Filesystem/env reads are injected as explicit seams from the fixture.
 * Exported symbols are w5_indexconfig_5a61c5_ prefixed.
 */
#include "noc_w5_indexconfig_5a61c5.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IC_NPM_PUBLIC "https://registry.npmjs.org/"

/* ---------------- set membership ---------------- */
static int ic_in_set(const char *key, const char *const *set, size_t n) {
    if (!key) return 0;
    for (size_t i = 0; i < n; ++i) if (!strcmp(key, set[i])) return 1;
    return 0;
}

int w5_indexconfig_5a61c5_is_forwarded(const char *key) {
    static const char *const s[] = {
        "UV_INDEX_URL", "UV_EXTRA_INDEX_URL", "UV_DEFAULT_INDEX", "UV_INDEX", "UV_NO_INDEX",
        "UV_FIND_LINKS", "UV_INDEX_STRATEGY", "UV_KEYRING_PROVIDER",
        "UV_NATIVE_TLS", "UV_INSECURE_HOST", "UV_HTTP_TIMEOUT"
    };
    return ic_in_set(key, s, sizeof(s) / sizeof(s[0])) ||
           (key && !strncmp(key, "UV_INDEX_", 9));
}

int w5_indexconfig_5a61c5_is_index_redirect(const char *key) {
    static const char *const s[] = {
        "UV_INDEX_URL", "UV_EXTRA_INDEX_URL", "UV_DEFAULT_INDEX", "UV_INDEX", "UV_NO_INDEX",
        "UV_FIND_LINKS", "UV_INDEX_STRATEGY", "UV_KEYRING_PROVIDER"
    };
    return ic_in_set(key, s, sizeof(s) / sizeof(s[0])) ||
           (key && !strncmp(key, "UV_INDEX_", 9));
}

/* ---------------- growable buffer ---------------- */
typedef struct { char *p; size_t n, cap; } ic_buf;
static void ic_putn(ic_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (b->n + n + 1 > nc) nc *= 2;
        b->p = (char *)realloc(b->p, nc);
        b->cap = nc;
    }
    if (b->p) { memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = '\0'; }
}
static void ic_puts(ic_buf *b, const char *s) { if (s) ic_putn(b, s, strlen(s)); }
static void ic_putc(ic_buf *b, char c) { ic_putn(b, &c, 1); }

static char *ic_trim_dup(const char *s) {
    if (!s) s = "";
    const char *a = s, *z = s + strlen(s);
    while (a < z && isspace((unsigned char)*a)) ++a;
    while (z > a && isspace((unsigned char)z[-1])) --z;
    size_t n = (size_t)(z - a);
    char *o = (char *)malloc(n + 1);
    if (o) { memcpy(o, a, n); o[n] = '\0'; }
    return o;
}

/* ---------------- _ini_unsafe ---------------- */
char *w5_indexconfig_5a61c5_ini_unsafe(const char *raw) {
    char *val = ic_trim_dup(raw);
    if (!val) return NULL;
    size_t n = strlen(val);
    if (n >= 2 && val[0] == val[n - 1] && (val[0] == '"' || val[0] == '\'')) {
        const char *target = val;
        char *inner = NULL;
        if (val[0] == '\'') { inner = (char *)malloc(n - 1); if (inner) { memcpy(inner, val + 1, n - 2); inner[n - 2] = '\0'; } target = inner; }
        char *e = NULL;
        json_t *parsed = json_parse(target, &e);
        char *res = NULL;
        if (parsed && parsed->type == JSON_STRING) res = ic_trim_dup(parsed->str_val);
        else res = ic_trim_dup(target);
        json_free(parsed);
        free(e); free(inner); free(val);
        return res;
    }
    ic_buf b = {0};
    int escaped = 0;
    for (size_t i = 0; i < n; ++i) {
        char c = val[i];
        if (escaped) {
            if (c == '\\' || c == ';' || c == '#') ic_putc(&b, c);
            else { ic_putc(&b, '\\'); ic_putc(&b, c); }
            escaped = 0;
        } else if (c == ';' || c == '#') break;
        else if (c == '\\') escaped = 1;
        else ic_putc(&b, c);
    }
    if (escaped) ic_putc(&b, '\\');
    free(val);
    if (!b.p) return ic_trim_dup("");
    char *out = ic_trim_dup(b.p);
    free(b.p);
    return out;
}

/* ---------------- _npmrc_env_replace ----------------
 * Match (?<!\\)(\\*)\$\{([^${}?]+)(\?)?\}
 */
static int ic_match(const char *t, size_t i, size_t k,
                    size_t *ns, size_t *nl, int *opt, size_t *end) {
    size_t j = i + k;
    if (t[j] != '$' || t[j + 1] != '{') return 0;
    size_t s = j + 2, e = s;
    while (t[e] && t[e] != '}' && t[e] != '?' && t[e] != '$' && t[e] != '{') ++e;
    if (e == s) return 0;
    int optional = 0;
    size_t after = e;
    if (t[e] == '?') { optional = 1; after = e + 1; }
    if (t[after] != '}') return 0;
    *ns = s; *nl = e - s; *opt = optional; *end = after + 1;
    return 1;
}

char *w5_indexconfig_5a61c5_npmrc_env_replace(const char *text, const struct json_t *env) {
    if (!text) text = "";
    ic_buf b = {0};
    int unset = 0;
    size_t i = 0;
    while (text[i]) {
        if (text[i] == '$' || text[i] == '\\') {
            size_t k = 0;
            while (text[i + k] == '\\') ++k;
            size_t ns = 0, nl = 0, end = 0;
            int opt = 0;
            int matched = ic_match(text, i, k, &ns, &nl, &opt, &end);
            if (matched) {
                if (k % 2) {
                    /* odd escapes: literal, drop (k+1)/2 backslashes */
                    for (size_t z = (k + 1) / 2; z < k; ++z) ic_putc(&b, '\\');
                    ic_puts(&b, "${");
                    ic_putn(&b, text + ns, nl);
                    if (opt) ic_putc(&b, '?');
                    ic_putc(&b, '}');
                } else {
                    for (size_t z = 0; z < k / 2; ++z) ic_putc(&b, '\\');
                    char name[512];
                    size_t cn = nl < sizeof(name) - 1 ? nl : sizeof(name) - 1;
                    memcpy(name, text + ns, cn); name[cn] = '\0';
                    json_t *v = (env && env->type == JSON_OBJECT) ? json_obj_get(env, name) : NULL;
                    if (v && v->type == JSON_STRING) ic_puts(&b, v->str_val);
                    else {
                        if (!opt) unset = 1;
                    }
                }
                i = end;
                continue;
            }
            for (size_t z = 0; z < k; ++z) ic_putc(&b, '\\');
            i += k;
            if (text[i] == '$') { ic_putc(&b, '$'); ++i; }
            else if (k == 0) { ic_putc(&b, text[i]); ++i; }
            continue;
        }
        ic_putc(&b, text[i++]);
    }
    if (unset) { free(b.p); return NULL; }
    if (!b.p) return ic_trim_dup(""); /* empty string, not NULL */
    return b.p;
}

/* ---------------- npm_registry / npm_registry_url ---------------- */
char *w5_indexconfig_5a61c5_npm_registry_ex(const char *env_registry, const char *npmrc_registry) {
    const char *value = (env_registry && *env_registry) ? env_registry
                        : (npmrc_registry ? npmrc_registry : "");
    if (!strncmp(value, "https://", 8) || !strncmp(value, "http://", 7)) {
        size_t n = strlen(value);
        while (n && value[n - 1] == '/') --n;
        char *o = (char *)malloc(n + 2);
        if (o) { memcpy(o, value, n); o[n] = '/'; o[n + 1] = '\0'; }
        return o;
    }
    return ic_trim_dup(IC_NPM_PUBLIC);
}

char *w5_indexconfig_5a61c5_npm_registry_url_ex(const char *url, const char *env_registry,
                                                 const char *npmrc_registry) {
    if (!url) url = "";
    size_t pub = strlen(IC_NPM_PUBLIC);
    if (strncmp(url, IC_NPM_PUBLIC, pub)) return ic_trim_dup(url);
    char *reg = w5_indexconfig_5a61c5_npm_registry_ex(env_registry, npmrc_registry);
    const char *rest = url + pub;
    size_t n = strlen(reg) + strlen(rest) + 1;
    char *o = (char *)malloc(n);
    if (o) { snprintf(o, n, "%s%s", reg, rest); }
    free(reg);
    return o;
}

/* ---------------- bridged_index_settings ---------------- */
static int ic_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val[0] != '\0';
        case JSON_ARRAY: return v->c.count != 0;
        case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}
static const char *ic_ambient_str(const json_t *ambient, const char *key) {
    if (!ambient || ambient->type != JSON_OBJECT) return "";
    json_t *v = json_obj_get(ambient, key);
    if (!v || v->type != JSON_STRING) return "";
    return v->str_val;
}

struct json_t *w5_indexconfig_5a61c5_bridged_index_settings_ex(const struct json_t *ambient,
                                                               const char *pip_conf_index_url) {
    json_t *out = json_object();
    if (!ambient || ambient->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(ambient); ++i) {
        const char *k = json_object_get_key_at(ambient, i);
        if (k && w5_indexconfig_5a61c5_is_forwarded(k))
            json_set(out, k, json_copy(json_object_get_at(ambient, i)));
    }
    static const char *const knobs[] = { "UV_INDEX_URL", "UV_DEFAULT_INDEX", "UV_INDEX" };
    int has_index = 0;
    for (size_t i = 0; i < sizeof(knobs) / sizeof(knobs[0]); ++i)
        if (ic_truthy(json_obj_get(out, knobs[i]))) { has_index = 1; break; }
    if (!has_index) {
        char *idx = ic_trim_dup(ic_ambient_str(ambient, "PIP_INDEX_URL"));
        if (!idx[0]) { free(idx); idx = ic_trim_dup(pip_conf_index_url); }
        if (idx[0]) json_set(out, "UV_INDEX_URL", json_string(idx));
        free(idx);
    }
    static const char *const pairs[][2] = {
        {"PIP_EXTRA_INDEX_URL", "UV_EXTRA_INDEX_URL"},
        {"PIP_TRUSTED_HOST", "UV_INSECURE_HOST"}
    };
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
        char *value = ic_trim_dup(ic_ambient_str(ambient, pairs[i][0]));
        if (value[0] && !ic_truthy(json_obj_get(out, pairs[i][1])))
            json_set(out, pairs[i][1], json_string(value));
        free(value);
    }
    return out;
}

