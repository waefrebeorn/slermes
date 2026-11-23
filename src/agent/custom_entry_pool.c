/*
 * custom_entry_pool.c — C home for upstream Hermes agent/custom_entry_pool.py.
 *
 * Faithful C11 ports of the pure credential-ownership helpers. Registry and
 * OpenRouter values are injected through the args object (no live config or
 * env reads), so differential oracles can drive the exact Python branches.
 *
 * PoP: w9_customentry_7786fd_entry_declares_credential @ agent/custom_entry_pool.py:entry_declares_credential
 * PoP: w9_customentry_7786fd_own_seeded_sources @ agent/custom_entry_pool.py:_own_seeded_sources
 * PoP: w9_customentry_7786fd_builtin_pool_address @ agent/custom_entry_pool.py:builtin_pool_address
 * PoP: w9_customentry_7786fd_recorded_address @ agent/custom_entry_pool.py:recorded_address
 */
#include "noc_w9_customentry_7786fd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CUSTOM_POOL_PREFIX "custom:"

static const char *const DECLARED_CREDENTIAL_KEYS[] = {
    "api_key", "key_env", "api_key_env", "key_cmd"
};

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python (value or "") stripped to a fresh string. Non-strings in fixtures
 * are treated as their empty-string coercion except truthy scalars. */
static char *py_str_stripped(const json_t *v)
{
    if (!v || json_is_null(v)) return strdup("");
    const char *src = NULL;
    char numbuf[64];
    if (json_is_string(v)) src = json_string_value(v);
    else if (json_is_bool(v)) src = json_bool_value(v) ? "True" : "False";
    else if (json_is_number(v)) { snprintf(numbuf, sizeof(numbuf), "%g", json_number_value(v)); src = numbuf; }
    if (!src) return strdup("");
    size_t n = strlen(src), s = 0, e = n;
    while (s < e && is_ws(src[s])) s++;
    while (e > s && is_ws(src[e - 1])) e--;
    char *out = (char *)malloc(e - s + 1);
    if (!out) return strdup("");
    memcpy(out, src + s, e - s);
    out[e - s] = '\0';
    return out;
}

static char *lower_dup(const char *s)
{
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return o;
}

static int py_bool(const json_t *v)
{
    if (!v || json_is_null(v)) return 0;
    if (json_is_bool(v)) return json_bool_value(v) ? 1 : 0;
    if (json_is_number(v)) return json_number_value(v) != 0;
    if (json_is_string(v)) { const char *s = json_string_value(v); return s && *s; }
    if (json_is_array(v) || json_is_object(v)) return json_len(v) != 0;
    return 0;
}

static int str_cmp(const void *x, const void *y)
{
    const char *const *a = (const char *const *)x;
    const char *const *b = (const char *const *)y;
    return strcmp(*a, *b);
}

/* PoP: w9_customentry_7786fd_entry_declares_credential @ agent/custom_entry_pool.py:entry_declares_credential */
json_t *w9_customentry_7786fd_entry_declares_credential(json_t *a)
{
    json_t *entry = json_obj_get(a, "entry");
    if (!json_is_object(entry)) return json_bool(0);
    json_t *flag = json_obj_get(entry, "declares_credential");
    if (flag) return json_bool(py_bool(flag));
    for (size_t i = 0; i < sizeof(DECLARED_CREDENTIAL_KEYS) / sizeof(DECLARED_CREDENTIAL_KEYS[0]); i++) {
        char *s = py_str_stripped(json_obj_get(entry, DECLARED_CREDENTIAL_KEYS[i]));
        int nonempty = *s != '\0';
        free(s);
        if (nonempty) return json_bool(1);
    }
    return json_bool(0);
}

/* PoP: w9_customentry_7786fd_own_seeded_sources @ agent/custom_entry_pool.py:_own_seeded_sources */
json_t *w9_customentry_7786fd_own_seeded_sources(json_t *a)
{
    json_t *entry = json_obj_get(a, "entry");
    char *buf[8];
    size_t n = 0;
    static const char *const ENV_KEYS[] = {"key_env", "api_key_env"};
    for (size_t i = 0; i < 2; i++) {
        char *name = py_str_stripped(json_is_object(entry) ? json_obj_get(entry, ENV_KEYS[i]) : NULL);
        if (*name) {
            size_t sz = strlen(name) + 6;
            buf[n] = (char *)malloc(sz);
            snprintf(buf[n], sz, "env:%s", name);
            n++;
        }
        free(name);
    }
    char *name = py_str_stripped(json_is_object(entry) ? json_obj_get(entry, "name") : NULL);
    size_t sz = strlen(name) + 9;
    buf[n] = (char *)malloc(sz);
    snprintf(buf[n], sz, "config:%s", name);
    n++;
    free(name);
    buf[n] = strdup("model_config");
    n++;

    qsort(buf, n, sizeof(char *), str_cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) { json_append(out, json_string(buf[i])); free(buf[i]); }
    return out;
}

/* builtin_pool_address with injected openrouter_url + registry. */
static json_t *builtin_addr(const char *pool_key, json_t *openrouter_url, json_t *registry)
{
    const char *raw = pool_key ? pool_key : "";
    size_t s = 0, e = strlen(raw);
    while (s < e && is_ws(raw[s])) s++;
    while (e > s && is_ws(raw[e - 1])) e--;
    char *k = (char *)malloc(e - s + 1);
    memcpy(k, raw + s, e - s);
    k[e - s] = '\0';
    for (char *p = k; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    json_t *result;
    if (!*k || strncmp(k, CUSTOM_POOL_PREFIX, strlen(CUSTOM_POOL_PREFIX)) == 0) {
        result = json_null();
    } else if (strcmp(k, "openrouter") == 0) {
        result = json_is_string(openrouter_url) ? json_copy(openrouter_url) : json_null();
    } else {
        json_t *pc = json_is_object(registry) ? json_obj_get(registry, k) : NULL;
        if (!pc) result = json_null();
        else result = json_string(py_str_stripped(pc));
    }
    free(k);
    return result;
}

/* PoP: w9_customentry_7786fd_builtin_pool_address @ agent/custom_entry_pool.py:builtin_pool_address */
json_t *w9_customentry_7786fd_builtin_pool_address(json_t *a)
{
    return builtin_addr(json_get_str(a, "pool_key", ""),
                        json_obj_get(a, "openrouter_url"), json_obj_get(a, "registry"));
}

/* PoP: w9_customentry_7786fd_recorded_address @ agent/custom_entry_pool.py:recorded_address */
json_t *w9_customentry_7786fd_recorded_address(json_t *a)
{
    json_t *row = json_obj_get(a, "row");
    char *addr = py_str_stripped(json_is_object(row) ? json_obj_get(row, "base_url") : NULL);
    if (*addr) { json_t *r = json_string(addr); free(addr); return r; }
    free(addr);
    json_t *b = builtin_addr(json_get_str(a, "pool_key", ""),
                             json_obj_get(a, "openrouter_url"), json_obj_get(a, "registry"));
    if (json_is_string(b)) return b;
    json_free(b);
    return json_string("");
}
