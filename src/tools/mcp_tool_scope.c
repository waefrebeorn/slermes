/*
 * mcp_tool_scope.c — C home for upstream Hermes tools/mcp_tool_scope.py.
 * PoP: w7_mcptoolsco_6bf440_key_name @ tools/mcp_tool_scope.py:_key_name
 * PoP: w7_mcptoolsco_6bf440_key_scope @ tools/mcp_tool_scope.py:_key_scope
 * PoP: w7_mcptoolsco_6bf440_key_visible_in_scope @ tools/mcp_tool_scope.py:_key_visible_in_scope
 * PoP: w7_mcptoolsco_6bf440_server_key @ tools/mcp_tool_scope.py:_server_key
 * PoP: w7_mcptoolsco_6bf440_resolve_server_key @ tools/mcp_tool_scope.py:_resolve_server_key
 *
 * The live Python reads process-global registry state (_mcp_registry_scope,
 * _servers, _lazy_server_configs, _server_tool_scopes). Each exported seam takes
 * that state explicitly from the fixture instead of reading C globals/env, so
 * the differential oracle can drive it deterministically.
 */
#include "noc_w7_mcptoolsco_6bf440.h"
#include <string.h>

static int key_is_tuple(const json_t *k) {
    return k && k->type == JSON_ARRAY && json_len(k) >= 2;
}
static const char *key_name_of(const json_t *k) {
    if (!k) return NULL;
    if (key_is_tuple(k)) {
        json_t *n = json_get(k, 1);
        return (n && n->type == JSON_STRING) ? n->str_val : NULL;
    }
    return k->type == JSON_STRING ? k->str_val : NULL;
}
static const char *key_scope_of(const json_t *k) {
    json_t *s;
    if (!key_is_tuple(k)) return NULL;
    s = json_get(k, 0);
    return (s && s->type == JSON_STRING) ? s->str_val : NULL;
}
static json_t *make_key(const char *scope, const char *name) {
    json_t *a;
    if (!scope) return json_string(name ? name : "");
    a = json_array();
    json_append(a, json_string(scope));
    json_append(a, json_string(name ? name : ""));
    return a;
}
static int key_eq(const json_t *a, const json_t *b) {
    const char *an, *bn, *as, *bs;
    if (!a || !b || key_is_tuple(a) != key_is_tuple(b)) return 0;
    an = key_name_of(a); bn = key_name_of(b);
    as = key_scope_of(a); bs = key_scope_of(b);
    if ((an == NULL) != (bn == NULL)) return 0;
    if (an && strcmp(an, bn)) return 0;
    if ((as == NULL) != (bs == NULL)) return 0;
    if (as && strcmp(as, bs)) return 0;
    return 1;
}
static int arr_contains_key(json_t *arr, const json_t *key) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (i = 0; i < json_len(arr); i++)
        if (key_eq(json_get(arr, i), key)) return 1;
    return 0;
}
static int str_in_arr(json_t *arr, const char *s) {
    size_t i;
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}
static const char *jscope(json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

json_t *w7_mcptoolsco_6bf440_key_name(json_t *key) {
    const char *n = key_name_of(key);
    return json_string(n ? n : "");
}
json_t *w7_mcptoolsco_6bf440_key_scope(json_t *key) {
    const char *s = key_scope_of(key);
    return s ? json_string(s) : json_null();
}
json_t *w7_mcptoolsco_6bf440_key_visible_in_scope(json_t *key, const char *scope, json_t *adopted) {
    const char *ks;
    if (!scope) return json_bool(true);
    ks = key_scope_of(key);
    if (ks && !strcmp(ks, scope)) return json_bool(true);
    return json_bool(str_in_arr(adopted, scope) ? true : false);
}
json_t *w7_mcptoolsco_6bf440_server_key(const char *name, json_t *scope, int current, json_t *current_scope) {
    const char *sc = jscope(scope);
    if (!sc && current) sc = jscope(current_scope);
    return make_key(sc, name ? name : "");
}
json_t *w7_mcptoolsco_6bf440_resolve_server_key(const char *name, json_t *scope, int current,
        json_t *current_scope, json_t *servers, json_t *lazy_configs, json_t *shared) {
    const char *sc = jscope(scope);
    json_t *own;
    if (!sc && current) sc = jscope(current_scope);
    own = make_key(sc, name ? name : "");
    if (!sc || arr_contains_key(servers, own) || arr_contains_key(lazy_configs, own)) return own;
    if (shared && shared->type == JSON_ARRAY) {
        size_t i;
        for (i = 0; i < json_len(shared); i++) {
            json_t *e = json_get(shared, i);
            json_t *k, *scopes;
            const char *kn;
            if (!e || e->type != JSON_OBJECT) continue;
            k = json_obj_get(e, "key");
            scopes = json_obj_get(e, "scopes");
            kn = key_name_of(k);
            if (!str_in_arr(scopes, sc)) continue;
            if (!kn || strcmp(kn, name ? name : "")) continue;
            if (arr_contains_key(servers, k)) { json_free(own); return json_copy(k); }
        }
    }
    return own;
}
