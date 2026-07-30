/*
 * port_tools_schema_sanitizer.c — C port of tools/schema_sanitizer.py
 *
 * JSON-Schema sanitization for broad LLM-backend compatibility. Faithful
 * port of the pure-transform functions; no I/O. Recursion is implemented as
 * true tree rebuilds over libjson (c.count / c.keys / c.items).
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA_TYPES[] = {"object","string","number","integer","boolean","array","null",NULL};
static int is_schema_type(const char *s)
{
    for (int i = 0; SCHEMA_TYPES[i]; i++)
        if (strcmp(s, SCHEMA_TYPES[i]) == 0) return 1;
    return 0;
}

/* forward decls for the _node helpers (defined later in this TU) */
json_t *cli_tools_schema_sanitizer_strip_nullable_unions_node(json_t *node);
json_t *cli_tools_schema_sanitizer__strip_top_level_combinators_node(json_t *params);
json_t *cli_tools_schema_sanitizer__strip_ref_siblings_node(json_t *node);

/* ---- _sanitize_node (recursive core) ---- */

static json_t *sanitize_node_walk(json_t *node)
{
    if (!node) return NULL;

    /* Bare-string schema value -> {"type": value} (object gets properties). */
    if (node->type == JSON_STRING && node->str_val) {
        const char *s = node->str_val;
        if (is_schema_type(s)) {
            json_t *o = json_object();
            json_set(o, "type", json_string(s));
            if (strcmp(s, "object") == 0)
                json_set(o, "properties", json_object());
            return o;
        }
        /* non-schema stray string -> permissive object */
        json_t *o = json_object();
        json_set(o, "type", json_string("object"));
        json_set(o, "properties", json_object());
        return o;
    }

    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        size_t n = json_len(node);
        for (size_t i = 0; i < n; i++)
            json_append(out, sanitize_node_walk(json_get(node, i)));
        return out;
    }

    if (node->type != JSON_OBJECT)
        return json_copy(node);

    json_t *out = json_object();
    for (size_t i = 0; i < node->c.count; i++) {
        const char *key = node->c.keys[i];
        json_t *value = node->c.items[i];
        if (!key || !value) continue;

        /* type: [X, "null"] -> type: X (+nullable) */
        if (strcmp(key, "type") == 0 && value->type == JSON_ARRAY) {
            size_t n = json_len(value);
            int has_null = 0;
            const char *first_str = NULL;
            int nn = 0;
            for (size_t k = 0; k < n; k++) {
                json_t *t = json_get(value, k);
                if (t && t->type == JSON_STRING && t->str_val) {
                    if (strcmp(t->str_val, "null") == 0) has_null = 1;
                    else { if (!first_str) first_str = t->str_val; nn++; }
                }
            }
            if (nn == 1 && first_str) {
                json_set(out, "type", json_string(first_str));
                if (has_null) json_set(out, "nullable", json_bool(1));
                continue;
            }
            if (first_str) { json_set(out, "type", json_string(first_str)); continue; }
            json_set(out, "type", json_string("object"));
            continue;
        }

        if ((strcmp(key, "properties") == 0 || strcmp(key, "$defs") == 0 ||
             strcmp(key, "definitions") == 0) && value->type == JSON_OBJECT) {
            json_t *sub = json_object();
            for (size_t k = 0; k < value->c.count; k++) {
                const char *sk = value->c.keys[k];
                json_t *sv = value->c.items[k];
                if (sk) json_set(sub, sk, sanitize_node_walk(sv));
            }
            json_set(out, key, sub);
            continue;
        }

        if ((strcmp(key, "items") == 0 || strcmp(key, "additionalProperties") == 0)) {
            if (value->type == JSON_BOOL) { json_set(out, key, json_copy(value)); continue; }
            json_set(out, key, sanitize_node_walk(value));
            continue;
        }

        if ((strcmp(key, "anyOf") == 0 || strcmp(key, "oneOf") == 0 ||
             strcmp(key, "allOf") == 0) && value->type == JSON_ARRAY) {
            json_t *arr = json_array();
            for (size_t k = 0; k < json_len(value); k++)
                json_append(arr, sanitize_node_walk(json_get(value, k)));
            json_set(out, key, arr);
            continue;
        }

        /* required / enum / examples: literal siblings — pass through. */
        if (strcmp(key, "required") == 0 || strcmp(key, "enum") == 0 ||
            strcmp(key, "examples") == 0) {
            json_set(out, key, json_copy(value));
            continue;
        }

        /* default recursion for everything else */
        if (value->type == JSON_OBJECT || value->type == JSON_ARRAY)
            json_set(out, key, sanitize_node_walk(value));
        else
            json_set(out, key, json_copy(value));
    }

    /* Object nodes without properties: inject empty properties. */
    json_t *t = json_obj_get(out, "type");
    if (t && t->type == JSON_STRING && strcmp(t->str_val, "object") == 0) {
        json_t *props = json_obj_get(out, "properties");
        if (!props || props->type != JSON_OBJECT)
            json_set(out, "properties", json_object());
    }

    /* Prune required entries not present in properties. */
    if (t && t->type == JSON_STRING && strcmp(t->str_val, "object") == 0) {
        json_t *req = json_obj_get(out, "required");
        if (req && req->type == JSON_ARRAY) {
            json_t *props = json_obj_get(out, "properties");
            json_t *valid = json_array();
            size_t n = json_len(req);
            for (size_t k = 0; k < n; k++) {
                json_t *r = json_get(req, k);
                if (r && r->type == JSON_STRING && r->str_val && props) {
                    if (json_obj_get(props, r->str_val))
                        json_append(valid, json_copy(r));
                }
            }
            if (json_len(valid) == 0)
                json_set(out, "required", json_null()); /* remove */
            else if (json_len(valid) != n)
                json_set(out, "required", valid);
            else
                json_free(valid);
        }
    }

    return out;
}

/* PoP: cli_tools_schema_sanitizer__sanitize_single_tool @ tools/schema_sanitizer.py:_sanitize_single_tool */

/* Core: sanitize an OpenAI-format tool object (deep-copied, mutated in place
 * on the returned copy). Caller frees the returned json_t. */
json_t *cli_tools_schema_sanitizer__sanitize_single_tool_node(json_t *tool_in)
{
    if (!tool_in) return json_object();
    json_t *out = json_copy(tool_in);
    if (!out || out->type != JSON_OBJECT) { if (out) json_free(out); return json_object(); }

    json_t *fn = json_obj_get(out, "function");
    if (!fn || fn->type != JSON_OBJECT) return out;

    json_t *params = json_obj_get(fn, "parameters");
    if (!params || params->type != JSON_OBJECT) {
        json_t *np = json_object();
        json_set(np, "type", json_string("object"));
        json_set(np, "properties", json_object());
        json_set(fn, "parameters", np);
        return out;
    }

    json_t *sanitized = sanitize_node_walk(params);
    if (!sanitized || sanitized->type != JSON_OBJECT) {
        sanitized = json_object();
        json_set(sanitized, "type", json_string("object"));
        json_set(sanitized, "properties", json_object());
    } else {
        json_t *tt = json_obj_get(sanitized, "type");
        if (!tt || tt->type != JSON_STRING || strcmp(tt->str_val, "object") != 0)
            json_set(sanitized, "type", json_string("object"));
        json_t *pp = json_obj_get(sanitized, "properties");
        if (!pp || pp->type != JSON_OBJECT)
            json_set(sanitized, "properties", json_object());
    }
    sanitized = cli_tools_schema_sanitizer_strip_nullable_unions_node(sanitized);
    sanitized = cli_tools_schema_sanitizer__strip_top_level_combinators_node(sanitized);
    sanitized = cli_tools_schema_sanitizer__strip_ref_siblings_node(sanitized);
    json_set(fn, "parameters", sanitized);
    return out;
}

char *cli_tools_schema_sanitizer__sanitize_single_tool(const char *tool_json)
{
    if (!tool_json || !tool_json[0]) return strdup("{}");
    char *err = NULL;
    json_t *root = json_parse(tool_json, &err);
    if (!root) { if (err) free(err); return strdup("{}"); }
    json_t *out = cli_tools_schema_sanitizer__sanitize_single_tool_node(root);
    json_free(root);
    char *r = json_serialize(out); json_free(out); return r ? r : strdup("{}");
}

/* PoP: cli_tools_schema_sanitizer__sanitize_tool_schemas @ tools/schema_sanitizer.py:sanitize_tool_schemas */

char *cli_tools_schema_sanitizer__sanitize_tool_schemas(const char *tools_json)
{
    if (!tools_json || !tools_json[0]) return strdup("[]");
    char *err = NULL;
    json_t *root = json_parse(tools_json, &err);
    if (!root) { if (err) free(err); return strdup("[]"); }
    json_t *out = json_array();
    if (root->type == JSON_ARRAY) {
        size_t n = json_len(root);
        for (size_t i = 0; i < n; i++) {
            json_t *tool = json_get(root, i);
            if (!tool || tool->type != JSON_OBJECT) { json_append(out, json_copy(tool)); continue; }
            json_t *st = cli_tools_schema_sanitizer__sanitize_single_tool_node(tool);
            json_append(out, st ? st : json_object());
        }
    }
    json_free(root);
    char *r = json_serialize(out); json_free(out); return r ? r : strdup("[]");
}

/* ---- strip_ref_siblings (recursive) ---- */

/* PoP: cli_tools_schema_sanitizer__strip_ref_siblings @ tools/schema_sanitizer.py:_strip_ref_siblings */

json_t *cli_tools_schema_sanitizer__strip_ref_siblings_node(json_t *node)
{
    if (!node) return NULL;
    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); i++)
            json_append(out, cli_tools_schema_sanitizer__strip_ref_siblings_node(json_get(node, i)));
        return out;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);
    json_t *out = json_object();
    for (size_t i = 0; i < node->c.count; i++) {
        const char *k = node->c.keys[i];
        json_t *v = node->c.items[i];
        if (k) json_set(out, k, cli_tools_schema_sanitizer__strip_ref_siblings_node(v));
    }
    json_t *ref = json_obj_get(out, "$ref");
    if (ref && ref->type == JSON_STRING) {
        json_t *d = json_obj_get(out, "default");
        if (d) {
            json_t *clean = json_object();
            for (size_t i = 0; i < out->c.count; i++) {
                const char *k = out->c.keys[i];
                if (k && strcmp(k, "default") == 0) continue;
                json_set(clean, k, json_copy(out->c.items[i]));
            }
            json_free(out);
            out = clean;
        }
    }
    return out;
}

char *cli_tools_schema_sanitizer__strip_ref_siblings(const char *json_input)
{
    if (!json_input || !json_input[0]) return strdup("{}");
    char *err = NULL;
    json_t *root = json_parse(json_input, &err);
    if (!root) { if (err) free(err); return strdup("{}"); }
    json_t *out = cli_tools_schema_sanitizer__strip_ref_siblings_node(root);
    json_free(root);
    char *r = json_serialize(out); json_free(out); return r ? r : strdup("{}");
}

/* ---- strip_top_level_combinators (top level ONLY) ---- */

/* PoP: cli_tools_schema_sanitizer__strip_top_level_combinators @ tools/schema_sanitizer.py:_strip_top_level_combinators */

static const char *TOP_FORBIDDEN[] = {"allOf","anyOf","oneOf","enum","not",NULL};

json_t *cli_tools_schema_sanitizer__strip_top_level_combinators_node(json_t *params)
{
    if (!params || params->type != JSON_OBJECT) return params;
    json_t *out = json_object();
    for (size_t i = 0; i < params->c.count; i++) {
        const char *k = params->c.keys[i];
        int forbidden = 0;
        for (int f = 0; TOP_FORBIDDEN[f]; f++)
            if (k && strcmp(k, TOP_FORBIDDEN[f]) == 0) { forbidden = 1; break; }
        if (!forbidden && k) json_set(out, k, json_copy(params->c.items[i]));
    }
    return out;
}

char *cli_tools_schema_sanitizer__strip_top_level_combinators(const char *json_input)
{
    if (!json_input || !json_input[0]) return strdup("{}");
    char *err = NULL;
    json_t *root = json_parse(json_input, &err);
    if (!root) { if (err) free(err); return strdup("{}"); }
    if (root->type != JSON_OBJECT) { char *r = json_serialize(root); json_free(root); return r ? r : strdup("{}"); }
    json_t *out = cli_tools_schema_sanitizer__strip_top_level_combinators_node(root);
    json_free(root);
    char *r = json_serialize(out); json_free(out); return r ? r : strdup("{}");
}

/* ---- strip_nullable_unions (recursive) ---- */

/* PoP: cli_tools_schema_sanitizer_strip_nullable_unions @ tools/schema_sanitizer.py:strip_nullable_unions */

static json_t *strip_nullable_walk(json_t *node)
{
    if (!node) return NULL;
    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); i++)
            json_append(out, strip_nullable_walk(json_get(node, i)));
        return out;
    }
    if (node->type != JSON_OBJECT) return json_copy(node);

    json_t *stripped = json_object();
    for (size_t i = 0; i < node->c.count; i++) {
        const char *k = node->c.keys[i];
        if (k) json_set(stripped, k, strip_nullable_walk(node->c.items[i]));
    }
    for (int u = 0; u < 2; u++) {
        const char *key = u == 0 ? "anyOf" : "oneOf";
        json_t *variants = json_obj_get(stripped, key);
        if (variants && variants->type == JSON_ARRAY) {
            size_t n = json_len(variants);
            json_t *non_null = NULL; int nn = 0, has_null = 0;
            for (size_t i = 0; i < n; i++) {
                json_t *item = json_get(variants, i);
                if (item && item->type == JSON_OBJECT) {
                    json_t *it = json_obj_get(item, "type");
                    if (it && it->type == JSON_STRING && strcmp(it->str_val, "null") == 0) has_null = 1;
                    else { non_null = item; nn++; }
                }
            }
            if (has_null && nn == 1 && non_null) {
                json_t *replacement = (non_null->type == JSON_OBJECT) ? json_copy(non_null) : json_object();
                json_set(replacement, "nullable", json_bool(1));
                const char *meta[] = {"title","description","default","examples",NULL};
                for (int m = 0; meta[m]; m++) {
                    json_t *mv = json_obj_get(stripped, meta[m]);
                    if (mv && !json_obj_get(replacement, meta[m])) {
                        if (strcmp(meta[m], "default") == 0 && json_obj_get(replacement, "$ref")) continue;
                        json_set(replacement, meta[m], json_copy(mv));
                    }
                }
                json_free(stripped);
                stripped = strip_nullable_walk(replacement);
                json_free(replacement);
                break;
            }
        }
    }
    return stripped;
}

json_t *cli_tools_schema_sanitizer_strip_nullable_unions_node(json_t *node)
{
    return strip_nullable_walk(node);
}

char *cli_tools_schema_sanitizer_strip_nullable_unions(const char *json_input)
{
    if (!json_input || !json_input[0]) return strdup("{}");
    char *err = NULL;
    json_t *root = json_parse(json_input, &err);
    if (!root) { if (err) free(err); return strdup("{}"); }
    json_t *out = strip_nullable_walk(root);
    json_free(root);
    char *r = json_serialize(out); json_free(out); return r ? r : strdup("{}");
}

/* ---- reactive strip: pattern/format and slash-enum ---- */

/* PoP: cli_tools_schema_sanitizer__strip_pattern_and_format @ tools/schema_sanitizer.py:strip_pattern_and_format */

static json_t *strip_pf_walk(json_t *node, int *stripped)
{
    if (!node) return NULL;
    if (node->type == JSON_OBJECT) {
        int is_schema = 0;
        for (size_t i = 0; i < node->c.count; i++) {
            const char *k = node->c.keys[i];
            if (k && (strcmp(k, "type") == 0 || strcmp(k, "anyOf") == 0 ||
                      strcmp(k, "oneOf") == 0 || strcmp(k, "allOf") == 0)) { is_schema = 1; break; }
        }
        json_t *out = json_object();
        for (size_t i = 0; i < node->c.count; i++) {
            const char *k = node->c.keys[i];
            if (!k) continue;
            if (is_schema && (strcmp(k, "pattern") == 0 || strcmp(k, "format") == 0)) { (*stripped)++; continue; }
            json_t *v = strip_pf_walk(node->c.items[i], stripped);
            json_set(out, k, v ? v : json_null());
        }
        return out;
    }
    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); i++)
            json_append(out, strip_pf_walk(json_get(node, i), stripped));
        return out;
    }
    return json_copy(node);
}

char *cli_tools_schema_sanitizer__strip_pattern_and_format(const char *tools_json, int *out_stripped)
{
    if (out_stripped) *out_stripped = 0;
    if (!tools_json || !tools_json[0]) return strdup("[]");
    char *err = NULL;
    json_t *root = json_parse(tools_json, &err);
    if (!root) { if (err) free(err); return strdup("[]"); }
    int stripped = 0;
    json_t *out = json_array();
    if (root->type == JSON_ARRAY) {
        size_t n = json_len(root);
        for (size_t i = 0; i < n; i++) {
            json_t *tool = json_get(root, i);
            if (!tool || tool->type != JSON_OBJECT) { json_append(out, json_copy(tool)); continue; }
            json_t *params = NULL, *fn = json_obj_get(tool, "function");
            if (fn && fn->type == JSON_OBJECT) params = json_obj_get(fn, "parameters");
            if (!params) params = json_obj_get(tool, "parameters");
            if (params && params->type == JSON_OBJECT) {
                json_t *walked = strip_pf_walk(params, &stripped);
                json_t *new_tool = json_copy(tool);
                if (fn && fn->type == JSON_OBJECT) {
                    json_t *nf = json_obj_get(new_tool, "function");
                    json_set(nf, "parameters", walked ? walked : json_object());
                } else json_set(new_tool, "parameters", walked ? walked : json_object());
                json_append(out, new_tool);
            } else json_append(out, json_copy(tool));
        }
    }
    json_free(root);
    char *r = json_serialize(out); json_free(out);
    if (out_stripped) *out_stripped = stripped;
    return r ? r : strdup("[]");
}

/* PoP: cli_tools_schema_sanitizer__strip_slash_enum @ tools/schema_sanitizer.py:strip_slash_enum */

static int json_array_contains_slash_string(json_t *arr)
{
    if (!arr || arr->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && strchr(v->str_val, '/')) return 1;
    }
    return 0;
}

static json_t *strip_se_walk(json_t *node, int *stripped)
{
    if (!node) return NULL;
    if (node->type == JSON_OBJECT) {
        json_t *enum_node = json_obj_get(node, "enum");
        if (enum_node && json_array_contains_slash_string(enum_node)) {
            (*stripped)++;
            json_t *out = json_object();
            for (size_t i = 0; i < node->c.count; i++) {
                const char *k = node->c.keys[i];
                if (!k || strcmp(k, "enum") == 0) continue;
                json_t *v = strip_se_walk(node->c.items[i], stripped);
                json_set(out, k, v ? v : json_null());
            }
            return out;
        }
        json_t *out = json_object();
        for (size_t i = 0; i < node->c.count; i++) {
            const char *k = node->c.keys[i];
            if (!k) continue;
            json_t *v = strip_se_walk(node->c.items[i], stripped);
            json_set(out, k, v ? v : json_null());
        }
        return out;
    }
    if (node->type == JSON_ARRAY) {
        json_t *out = json_array();
        for (size_t i = 0; i < json_len(node); i++)
            json_append(out, strip_se_walk(json_get(node, i), stripped));
        return out;
    }
    return json_copy(node);
}

char *cli_tools_schema_sanitizer__strip_slash_enum(const char *tools_json, int *out_stripped)
{
    if (out_stripped) *out_stripped = 0;
    if (!tools_json || !tools_json[0]) return strdup("[]");
    char *err = NULL;
    json_t *root = json_parse(tools_json, &err);
    if (!root) { if (err) free(err); return strdup("[]"); }
    int stripped = 0;
    json_t *out = json_array();
    if (root->type == JSON_ARRAY) {
        size_t n = json_len(root);
        for (size_t i = 0; i < n; i++) {
            json_t *tool = json_get(root, i);
            if (!tool || tool->type != JSON_OBJECT) { json_append(out, json_copy(tool)); continue; }
            json_t *params = NULL, *fn = json_obj_get(tool, "function");
            if (fn && fn->type == JSON_OBJECT) params = json_obj_get(fn, "parameters");
            if (!params) params = json_obj_get(tool, "parameters");
            if (params && params->type == JSON_OBJECT) {
                json_t *walked = strip_se_walk(params, &stripped);
                json_t *new_tool = json_copy(tool);
                if (fn && fn->type == JSON_OBJECT) {
                    json_t *nf = json_obj_get(new_tool, "function");
                    json_set(nf, "parameters", walked ? walked : json_object());
                } else json_set(new_tool, "parameters", walked ? walked : json_object());
                json_append(out, new_tool);
            } else json_append(out, json_copy(tool));
        }
    }
    json_free(root);
    char *r = json_serialize(out); json_free(out);
    if (out_stripped) *out_stripped = stripped;
    return r ? r : strdup("[]");
}
