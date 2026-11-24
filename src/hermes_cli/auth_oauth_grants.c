/*
 * auth_oauth_grants.c — C port of upstream Hermes hermes_cli/auth_oauth_grants.py.
 *
 * PoP: w16_authoauth_89a540_is_oauth_pool_payload @ hermes_cli/auth_oauth_grants.py:_is_oauth_pool_payload
 * PoP: w16_authoauth_89a540_is_forkable_pool_row @ hermes_cli/auth_oauth_grants.py:_is_forkable_pool_row
 * PoP: w16_authoauth_89a540_block_tokens @ hermes_cli/auth_oauth_grants.py:_block_tokens
 * PoP: w16_authoauth_89a540_is_pkce_row @ hermes_cli/auth_oauth_grants.py:_is_pkce_row
 * PoP: w16_authoauth_89a540_sign @ hermes_cli/auth_oauth_grants.py:_sign
 * PoP: w16_authoauth_89a540_constants @ hermes_cli/auth_oauth_grants.py:SINGLE_USE_REFRESH_POOL_PROVIDERS/_DEVICE_CODE_BLOCK_PROVIDERS/_OAUTH_TOKEN_FIELDS/SINGLE_USE_OAUTH_SINGLETON_FILES
 */
#include "noc_w16_authoauth_89a540.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* never name a helper `dup` (POSIX collision) */
static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static bool py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        case JSON_ARRAY:  return json_len(v) > 0;
        case JSON_OBJECT: return json_object_size(v) > 0;
        default: return false;
    }
}

/* Python str(value) for scalars (fixtures use scalar fields). */
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return dupstr("");
    if (v->type == JSON_STRING) return dupstr(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return dupstr(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%lld", (long long)v->num_val);
        else
            snprintf(b, sizeof(b), "%.17g", v->num_val);
        return dupstr(b);
    }
    return dupstr("");
}

/* str(obj.get(key) or "") */
static char *field_str(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (!py_truthy(v)) return dupstr("");
    return py_str(v);
}

/* In-place ASCII whitespace strip; memmove keeps the allocation start and
 * the NUL is written at the new length. */
static void strip_inplace(char *s) {
    if (!s) return;
    char *a = s, *z = s + strlen(s);
    while (a < z && isspace((unsigned char)*a)) a++;
    while (z > a && isspace((unsigned char)z[-1])) z--;
    size_t len = (size_t)(z - a);
    memmove(s, a, len);
    s[len] = '\0';
}
static void lower_inplace(char *s) {
    for (; s && *s; s++) *s = (char)tolower((unsigned char)*s);
}

bool w16_authoauth_89a540_is_oauth_pool_payload(const json_t *entry) {
    if (!entry || entry->type != JSON_OBJECT) return false;
    char *at = field_str(entry, "auth_type");
    strip_inplace(at); lower_inplace(at);
    bool c1 = !strcmp(at, "oauth");
    free(at);
    char *rt = field_str(entry, "refresh_token");
    strip_inplace(rt);
    bool c2 = rt[0] != '\0';
    free(rt);
    char *ac = field_str(entry, "access_token");
    bool c3 = !strncmp(ac, "sk-ant-oat", 10);
    free(ac);
    return c1 || c2 || c3;
}

bool w16_authoauth_89a540_is_forkable_pool_row(const char *provider_id, const json_t *entry) {
    if (!w16_authoauth_89a540_is_oauth_pool_payload(entry)) return false;
    if (!provider_id || strcmp(provider_id, "nous")) return true;
    char *rt = field_str(entry, "refresh_token");
    strip_inplace(rt);
    bool r = rt[0] != '\0';
    free(rt);
    return r;
}

json_t *w16_authoauth_89a540_block_tokens(const json_t *block) {
    if (!block || block->type != JSON_OBJECT) return json_copy(block);
    json_t *t = json_obj_get(block, "tokens");
    if (t && t->type == JSON_OBJECT) return json_copy(t);
    return json_copy(block);
}

bool w16_authoauth_89a540_is_pkce_row(const json_t *row) {
    if (!row || row->type != JSON_OBJECT) return false;
    char *s = field_str(row, "source");
    const char *suffix = "hermes_pkce";
    size_t n = strlen(s), m = strlen(suffix);
    bool r = n >= m && !strcmp(s + n - m, suffix);
    free(s);
    return r;
}

json_t *w16_authoauth_89a540_sign(const json_t *x, const json_t *y) {
    if (!x || x->type == JSON_NULL || !y || y->type == JSON_NULL) return json_null();
    double a = (x->type == JSON_NUMBER) ? x->num_val : 0.0;
    double b = (y->type == JSON_NUMBER) ? y->num_val : 0.0;
    return json_number((double)((a > b) - (a < b)));
}

json_t *w16_authoauth_89a540_constants(void) {
    json_t *out = json_object();
    json_t *su = json_array();
    json_append(su, json_string("anthropic"));
    json_append(su, json_string("nous"));
    json_append(su, json_string("openai-codex"));
    json_append(su, json_string("xai-oauth"));
    json_set(out, "single_use", su);
    json_t *dev = json_array();
    json_append(dev, json_string("openai-codex"));
    json_append(dev, json_string("xai-oauth"));
    json_append(dev, json_string("nous"));
    json_set(out, "device_code", dev);
    json_t *tf = json_array();
    json_append(tf, json_string("access_token"));
    json_append(tf, json_string("refresh_token"));
    json_append(tf, json_string("expires_at"));
    json_append(tf, json_string("expires_at_ms"));
    json_append(tf, json_string("last_refresh"));
    json_set(out, "token_fields", tf);
    json_t *sf = json_array();
    json_append(sf, json_string(".anthropic_oauth.json"));
    json_set(out, "singleton_files", sf);
    return out;
}
