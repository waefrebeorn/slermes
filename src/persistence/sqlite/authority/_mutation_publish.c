/*
 * _mutation_publish.c - C11 port of upstream
 * persistence/sqlite/authority/_mutation_publish.py (gate w26_mutationpu_fecf98).
 *
 * PoP: w26_mutationpu_fecf98_profile_from_session_key @ persistence/sqlite/authority/_mutation_publish.py:_profile_from_session_key
 * PoP: w26_mutationpu_fecf98_child_policy @ persistence/sqlite/authority/_mutation_publish.py:_child_policy
 * PoP: w26_mutationpu_fecf98_constants @ persistence/sqlite/authority/_mutation_publish.py:TITLE_SOURCE_USER/TITLE_SOURCE_DERIVED
 *
 * What a publish decides from values alone: the profile a session key names, and
 * the policy its branch child starts with.  A branch never shares the parent's
 * editor MCP scope -- the child's prepared selection replaces it, or the publish
 * refuses.  Every connection, row, writer and receipt is a seam in the caller.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_mutationpu_fecf98.h"

static const char *mp_str(const json_t *v) {
    if (!v || v->type != JSON_STRING || !v->str_val) return NULL;
    return v->str_val;
}

static int mp_nullish(const json_t *v) { return !v || v->type == JSON_NULL; }

/* Python truthiness of a JSON value. */
static int mp_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int mp_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python ==: absent == null, True == 1, 1 == 1.0, dicts and lists structurally. */
static int mp_eq(const json_t *a, const json_t *b) {
    double x, y;
    size_t i, n;

    if (mp_nullish(a) || mp_nullish(b)) return mp_nullish(a) && mp_nullish(b);
    if (mp_num(a, &x) && mp_num(b, &y)) return x == y;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case JSON_STRING:
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    case JSON_ARRAY:
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!mp_eq(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    case JSON_OBJECT:
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !mp_eq(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    default:
        return 0;
    }
}

static json_t *mp_refusal(const char *code) {
    json_t *o = json_object();
    json_set(o, "__refusal", json_string(code));
    return o;
}

json_t *w26_mutationpu_fecf98_profile_from_session_key(const json_t *session_key) {
    const char *key = mp_str(session_key), *colon1, *colon2, *ns;
    size_t ns_len;

    if (!key || !*key) return json_null();
    colon1 = strchr(key, ':');
    if (!colon1) return json_null();
    if ((size_t)(colon1 - key) != 5 || strncmp(key, "agent", 5) != 0) return json_null();
    ns = colon1 + 1;
    colon2 = strchr(ns, ':');
    ns_len = colon2 ? (size_t)(colon2 - ns) : strlen(ns);
    if (ns_len == 0) return json_string("default");        /* parts[1] or "main" */
    if (ns_len == 4 && !strncmp(ns, "main", 4)) return json_string("default");
    if (ns_len == 5 && !strncmp(ns, "main~", 5)) return json_string("main");
    {
        char *copy = (char *)malloc(ns_len + 1);
        json_t *out;
        if (!copy) return json_null();
        memcpy(copy, ns, ns_len);
        copy[ns_len] = '\0';
        out = json_string(copy);
        free(copy);
        return out;
    }
}

json_t *w26_mutationpu_fecf98_child_policy(const json_t *policy, const json_t *child,
                                           const json_t *prepared) {
    json_t *out;

    if (!policy || policy->type != JSON_OBJECT) return mp_refusal("invalid_params");
    out = json_copy(policy);
    if (!out) return mp_refusal("storage_unavailable");
    if (json_has(out, "edit_approval_policy"))     /* a branch starts at Default */
        json_set(out, "edit_approval_policy", json_string("ask"));

    if (mp_nullish(prepared)) {
        if (mp_truthy(json_obj_get(out, "editor_mcp_json"))) {
            json_free(out);
            return mp_refusal("runtime_coordination_required");
        }
        return out;
    }
    if (prepared->type != JSON_OBJECT) {
        json_free(out);
        return mp_refusal("invalid_params");
    }
    /* Prepared against the parent's selection and the child id this commit derives. */
    if (!mp_eq(child, json_obj_get(prepared, "child")) ||
        !mp_eq(json_obj_get(out, "editor_mcp_json"),
               json_obj_get(prepared, "parent_editor_mcp_json"))) {
        json_free(out);
        return mp_refusal("revision_conflict");
    }
    {
        const char *keys[3] = {"editor_mcp_json", "toolsets", "config_secret_ref"};
        size_t i;
        for (i = 0; i < 3; i++)
            json_set(out, keys[i], json_copy(json_obj_get(prepared, keys[i])));
    }
    return out;
}

json_t *w26_mutationpu_fecf98_constants(void) {
    json_t *o = json_object();
    json_set(o, "title_source_user", json_string("user"));
    json_set(o, "title_source_derived", json_string("derived"));
    return o;
}
