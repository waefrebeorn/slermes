/*
 * tool_search_validation.c - C home for upstream Hermes tools/tool_search_validation.py.
 *
 * PoP: w16_toolsearch_240700_schema_for_local_validation @ tools/tool_search_validation.py:_schema_for_local_validation
 * PoP: w16_toolsearch_240700_schema_has_external_ref @ tools/tool_search_validation.py:_schema_has_external_ref
 * PoP: w16_toolsearch_240700_normalize_tool_call_entries @ tools/tool_search_validation.py:normalize_tool_call_entries
 * PoP: w16_toolsearch_240700_local_batch_error @ tools/tool_search_validation.py:local_batch_error
 * PoP: w16_toolsearch_240700_unavailable_in_session_reason @ tools/tool_search_validation.py:unavailable_in_session_reason
 * PoP: w16_toolsearch_240700_session_unavailable_error @ tools/tool_search_validation.py:session_unavailable_error
 * PoP: w16_toolsearch_240700_not_deferrable_error @ tools/tool_search_validation.py:not_deferrable_error
 *
 * Registry-derived sets (bridge names, core names, tool names, out-of-scope
 * reason) are supplied by the fixture, so the port exercises the real logic.
 */
#include "noc_w16_toolsearch_240700.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : "";
}
static int is_literal_key(const char *k) {
    return !strcmp(k, "const") || !strcmp(k, "default") || !strcmp(k, "enum") ||
           !strcmp(k, "example") || !strcmp(k, "examples");
}
static char *compact(const json_t *v) {
    char *s = json_dumps(v, 0);
    return s ? s : (char *)"null";
}

/* ---------- _schema_for_local_validation ---------- */
static json_t *schema_for_local(const json_t *node) {
    if (!node) return json_null();
    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); ++i)
            json_append(out, schema_for_local(json_get(node, i)));
        return out;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);
    json_t *norm = json_object();
    for (size_t i = 0; i < json_object_size(node); ++i) {
        const char *k = json_object_get_key_at(node, i);
        if (!strcmp(k, "nullable")) continue;
        json_t *v = json_object_get_at(node, i);
        json_set(norm, k, is_literal_key(k) ? json_copy(v) : schema_for_local(v));
    }
    json_t *nv = json_obj_get(node, "nullable");
    if (!(nv && nv->type == JSON_BOOL && nv->bool_val)) return norm;
    json_t *t = json_obj_get(norm, "type");
    if (t && t->type == JSON_STRING) {
        json_t *arr = json_array();
        json_append(arr, json_copy(t));
        json_set(norm, "type", arr);
        t = arr;
    }
    if (t && t->type == JSON_ARRAY) {
        int has_null = 0;
        for (size_t i = 0; i < json_len(t); ++i)
            if (!strcmp(jstr(json_get(t, i)), "null")) has_null = 1;
        if (!has_null) json_append(json_obj_get(norm, "type"), json_string("null"));
        return norm;
    }
    json_t *alt = json_array();
    json_append(alt, norm);
    json_t *nul = json_object();
    json_set(nul, "type", json_string("null"));
    json_append(alt, nul);
    json_t *out = json_object();
    json_set(out, "anyOf", alt);
    return out;
}
json_t *w16_toolsearch_240700_schema_for_local_validation(const json_t *node) {
    return schema_for_local(node);
}

/* ---------- _schema_has_external_ref ---------- */
static int schema_has_external(const json_t *node) {
    if (!node) return 0;
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); ++i)
            if (schema_has_external(json_get(node, i))) return 1;
        return 0;
    }
    if (node->type != JSON_OBJECT) return 0;
    json_t *ref = json_obj_get(node, "$ref");
    if (ref && ref->type == JSON_STRING && strncmp(ref->str_val, "#", 1)) return 1;
    for (size_t i = 0; i < json_object_size(node); ++i) {
        const char *k = json_object_get_key_at(node, i);
        if (!is_literal_key(k) && schema_has_external(json_object_get_at(node, i))) return 1;
    }
    return 0;
}
json_t *w16_toolsearch_240700_schema_has_external_ref(const json_t *node) {
    return json_bool(schema_has_external(node) ? true : false);
}

/* ---------- helpers for entries ---------- */
static char *trimdup(const char *s) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' ||
                 s[n-1] == '\r' || s[n-1] == '\f' || s[n-1] == '\v')) n--;
    char *o = (char *)malloc(n + 1);
    if (!o) return (char *)"";
    memcpy(o, s, n); o[n] = '\0';
    return o;
}
static int jarr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); ++i)
        if (!strcmp(jstr(json_get(arr, i)), s)) return 1;
    return 0;
}
static int json_nullish(const json_t *v) { return !v || v->type == JSON_NULL; }

/* Returns entries array; writes an error message into err (capacity cap). */
static json_t *norm_entries(const json_t *args, const json_t *bridge_names,
                            char *err, size_t cap) {
    err[0] = '\0';
    json_t *entries = json_array();
    json_t *raw = json_obj_get(args, "calls");
    if (json_nullish(raw)) {
        char *nm = trimdup(jstr(json_obj_get(args, "name")));
        if (!*nm) {
            snprintf(err, cap, "%s",
                     "tool_call requires 'calls' (an array of {name, arguments})");
            free(nm);
            return entries;
        }
        json_t *one = json_object();
        json_set(one, "name", json_obj_get(args, "name"));
        json_set(one, "arguments", json_obj_get(args, "arguments"));
        raw = json_array();
        json_append(raw, one);
        free(nm);
    } else if (raw->type == JSON_STRING) {
        json_t *parsed = json_parse(raw->str_val, NULL);
        if (!parsed) {
            snprintf(err, cap, "%s",
                     "tool_call 'calls' is not valid JSON: Expecting value: line 1 column 1 (char 0)");
            return entries;
        }
        raw = parsed;
    }
    if (raw->type == JSON_OBJECT) {
        json_t *wrap = json_array();
        json_append(wrap, json_copy(raw));
        raw = wrap;
    }
    if (raw->type != JSON_ARRAY || json_len(raw) == 0) {
        snprintf(err, cap, "%s", "tool_call 'calls' must be a non-empty array of {name, arguments}");
        return entries;
    }
    for (size_t i = 0; i < json_len(raw); ++i) {
        json_t *e = json_get(raw, i);
        if (!e || e->type != JSON_OBJECT) {
            snprintf(err, cap, "tool_call calls[%zu] must be an object with 'name' and 'arguments'", i);
            return entries;
        }
        char *nm = trimdup(jstr(json_obj_get(e, "name")));
        if (!*nm) {
            snprintf(err, cap, "tool_call calls[%zu] requires a 'name'", i);
            free(nm);
            return entries;
        }
        if (jarr_has(bridge_names, nm)) {
            snprintf(err, cap, "tool_call cannot invoke '%s' (it is itself a bridge tool)", nm);
            free(nm);
            return entries;
        }
        json_t *a = json_obj_get(e, "arguments");
        if (json_nullish(a)) {
            a = json_object();
        } else if (a->type == JSON_STRING) {
            char *t = trimdup(a->str_val);
            if (!*t) { a = json_object(); free(t); }
            else {
                json_t *p = json_parse(t, NULL);
                free(t);
                if (!p) {
                    snprintf(err, cap, "tool_call calls[%zu].arguments is not valid JSON: Expecting value", i);
                    free(nm);
                    return entries;
                }
                a = p;
            }
        }
        if (!a || a->type != JSON_OBJECT) {
            snprintf(err, cap, "tool_call calls[%zu].arguments must be an object", i);
            free(nm);
            return entries;
        }
        json_t *out_e = json_object();
        json_set(out_e, "name", json_string(nm));
        json_set(out_e, "arguments", json_copy(a));
        json_append(entries, out_e);
        free(nm);
    }
    return entries;
}
json_t *w16_toolsearch_240700_normalize_tool_call_entries(const json_t *args,
                                                          const json_t *bridge_names) {
    char err[2048];
    json_t *entries = norm_entries(args, bridge_names, err, sizeof(err));
    json_t *out = json_object();
    json_set(out, "entries", entries);
    json_set(out, "error", err[0] ? json_string(err) : json_null());
    return out;
}

/* ---------- local_batch_error ---------- */
json_t *w16_toolsearch_240700_local_batch_error(const json_t *entries) {
    size_t n = (entries && entries->type == JSON_ARRAY) ? json_len(entries) : 0;
    json_t *first = n ? json_get(entries, 0) : NULL;
    json_t *a = first ? json_obj_get(first, "arguments") : NULL;
    if (json_nullish(a)) a = json_object();
    char *as = compact(a);
    const char *use = (strlen(as) > 1500) ? "{...}" : as;
    char *ns = compact(json_obj_get(first, "name"));
    char retry[4096];
    snprintf(retry, sizeof(retry), "{\"calls\":[{\"name\":%s,\"arguments\":%s}]}", ns, use);
    if (as != (char *)"null") free(as);
    if (ns != (char *)"null") free(ns);
    char msg[8192];
    if (n > 1) {
        snprintf(msg, sizeof(msg),
                 "tool_call takes exactly one entry for local tools; you sent %zu. Retry with "
                 "only: %s then issue the remaining %zu call(s) as separate tool_call "
                 "invocations. Only connectors__ names may be batched together.",
                 n, retry, n - 1);
    } else {
        snprintf(msg, sizeof(msg),
                 "tool_call takes exactly one entry for local tools; you sent %zu. Retry with "
                 "only: %s. Only connectors__ names may be batched together.", n, retry);
    }
    return json_string(msg);
}

/* ---------- availability messages ---------- */
json_t *w16_toolsearch_240700_unavailable_in_session_reason(const char *name) {
    char m[1024];
    snprintf(m, sizeof(m),
             "'%s' is not available in this execution context. Don't retry it — state the "
             "assumption and proceed with the work.", name);
    return json_string(m);
}
json_t *w16_toolsearch_240700_session_unavailable_error(const char *name,
                                                        const json_t *session_names,
                                                        int core_and_registered,
                                                        const char *out_of_scope) {
    if (session_names && session_names->type == JSON_NULL) session_names = NULL;
    if (!session_names || jarr_has(session_names, name)) return json_null();
    if (!core_and_registered) return json_null();
    if (out_of_scope && *out_of_scope) return json_string(out_of_scope);
    return w16_toolsearch_240700_unavailable_in_session_reason(name);
}
static int cmpstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
json_t *w16_toolsearch_240700_not_deferrable_error(const char *name,
                                                   const json_t *session_names,
                                                   int core_or_registered,
                                                   const json_t *all_tool_names) {
    if (core_or_registered) {
        if (session_names && session_names->type != JSON_NULL && !jarr_has(session_names, name))
            return w16_toolsearch_240700_unavailable_in_session_reason(name);
        char m[1024];
        snprintf(m, sizeof(m),
                 "'%s' is a directly-listed tool, not a deferred one. Call it directly "
                 "instead of via tool_call.", name);
        return json_string(m);
    }
    char suffix[2048];
    snprintf(suffix, sizeof(suffix), "__%s", name);
    char **cands = NULL; size_t nc = 0;
    if (all_tool_names && all_tool_names->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(all_tool_names); ++i) {
            const char *c = jstr(json_get(all_tool_names, i));
            size_t L = strlen(c), S = strlen(suffix);
            if (L >= S && !strcmp(c + L - S, suffix)) {
                char **tmp = (char **)realloc(cands, (nc + 1) * sizeof(char *));
                if (!tmp) break;
                cands = tmp;
                cands[nc] = (char *)malloc(L + 1);
                memcpy(cands[nc], c, L + 1);
                nc++;
            }
        }
    }
    if (nc > 1) qsort(cands, nc, sizeof(char *), cmpstr);
    char hint[4096] = {0};
    if (nc) {
        size_t pos = 0;
        pos += (size_t)snprintf(hint + pos, sizeof(hint) - pos, " Did you mean ");
        for (size_t i = 0; i < nc; ++i) {
            pos += (size_t)snprintf(hint + pos, sizeof(hint) - pos, "%s'%s'%s",
                                    i ? ", " : "", cands[i], i + 1 == nc ? "?" : "");
        }
    } else {
        snprintf(hint, sizeof(hint), "%s", " Use tool_search to find the exact name.");
    }
    for (size_t i = 0; i < nc; ++i) free(cands[i]);
    free(cands);
    char m[8192];
    snprintf(m, sizeof(m),
             "'%s' is not a known tool name. Deferred tools must be invoked through tool_call "
             "by the exact name tool_search returns (e.g. mcp__<server>__{tool}).%s", name, hint);
    return json_string(m);
}
