/*
 * moonshot_schema.c — Moonshot/Kimi schema sanitizer.
 *
 * Port of Python agent/moonshot_schema.py (262 lines).
 * 5 Moonshot-specific fixes:
 * 1. Every property must have "type" — add if missing
 * 2. When anyOf is used, strip "type" from parent
 * 3. Strip null and empty-string entries from enum arrays
 * 4. Strip all sibling keys from $ref nodes
 * 5. Collapse tuple-style items arrays to single schema
 *
 * MIT License — WuBu Slermes Project
 */

#include "moonshot_schema.h"
#include "hermes_json.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static json_t *sanitize_node(json_t *node, int depth);

/* Keys whose values contain schema maps (recurse into values) */
static bool is_schema_map_key(const char *key) {
    return (strcmp(key, "properties") == 0 ||
            strcmp(key, "patternProperties") == 0 ||
            strcmp(key, "$defs") == 0 ||
            strcmp(key, "definitions") == 0);
}

/* Keys whose values are lists of schemas */
static bool is_schema_list_key(const char *key) {
    return (strcmp(key, "anyOf") == 0 ||
            strcmp(key, "oneOf") == 0 ||
            strcmp(key, "allOf") == 0 ||
            strcmp(key, "prefixItems") == 0);
}

/* Keys whose values are single nested schemas */
static bool is_schema_node_key(const char *key) {
    return (strcmp(key, "items") == 0 ||
            strcmp(key, "contains") == 0 ||
            strcmp(key, "not") == 0 ||
            strcmp(key, "additionalProperties") == 0 ||
            strcmp(key, "propertyNames") == 0);
}

static json_t *sanitize_object(json_t *obj, int depth) {
    json_t *out = json_object();

    /* Pass 1: handle $ref nodes — strip all siblings */
    json_t *ref_val = json_obj_get(obj, "$ref");
    if (ref_val && depth >= 0) {
        json_set(out, "$ref", json_copy(ref_val));
        return out;
    }

    /* Pass 2: process all keys */
    for (size_t i = 0; i < obj->c.count; i++) {
        const char *key = obj->c.keys[i];
        json_t *val = obj->c.items[i];
        if (!key || !val) continue;

        if (is_schema_map_key(key) && val->type == JSON_OBJECT) {
            json_t *map = json_object();
            for (size_t j = 0; j < val->c.count; j++) {
                const char *sk = val->c.keys[j];
                json_t *sv = val->c.items[j];
                if (sk && sv) {
                    json_t *sanitized = sanitize_node(sv, depth + 1);
                    if (sanitized) {
                        json_set(map, sk, sanitized);
                    }
                }
            }
            json_set(out, key, map);
        } else if (is_schema_list_key(key) && val->type == JSON_ARRAY) {
            json_t *arr = json_array();
            for (size_t j = 0; j < val->c.count; j++) {
                json_t *item = val->c.items[j];
                if (item && item->type == JSON_OBJECT) {
                    json_t *sanitized = sanitize_node(item, depth + 1);
                    if (sanitized) {
                        json_append(arr, sanitized);
                    }
                }
            }
            json_set(out, key, arr);
        } else if (is_schema_node_key(key) && val->type == JSON_OBJECT) {
            json_t *sanitized = sanitize_node(val, depth + 1);
            if (sanitized) {
                json_set(out, key, sanitized);
            }
        } else if (strcmp(key, "enum") == 0 && val->type == JSON_ARRAY) {
            /* Strip null and empty-string entries from enum */
            json_t *enum_arr = json_array();
            for (size_t j = 0; j < val->c.count; j++) {
                json_t *e = val->c.items[j];
                if (e->type == JSON_NULL) continue;
                if (e->type == JSON_STRING && (!e->str_val || !e->str_val[0])) continue;
                json_append(enum_arr, json_copy(e));
            }
            if (json_len(enum_arr) > 0) {
                json_set(out, key, enum_arr);
            }
        } else {
            json_set(out, key, json_copy(val));
        }
    }

    /* Pass 3: add missing "type" to property schemas — Port of Python _fill_missing_type() */
    /* AG26: Port of Python agent/moonshot_schema.py:_fill_missing_type() */
    json_t *type_val = json_obj_get(out, "type");
    if (!type_val) {
        json_set(out, "type", json_string("object"));
    }

    /* Pass 4: when anyOf is present and parent also has type, strip parent type */
    json_t *anyof_val = json_obj_get(out, "anyOf");
    if (anyof_val) {
        json_obj_del(out, "type");
    }

    return out;
}

/* Sanitize a JSON node */
/* Port of Python agent/moonshot_schema.py:_repair_schema() — recursive schema repair. */
static json_t *sanitize_node(json_t *node, int depth) {
    if (!node) return NULL;
    if (node->type == JSON_OBJECT)
        return sanitize_object(node, depth);
    if (node->type == JSON_ARRAY) {
        json_t *arr = json_array();
        for (size_t i = 0; i < node->c.count; i++) {
            json_t *item = sanitize_node(node->c.items[i], depth + 1);
            if (item) json_append(arr, item);
        }
        return arr;
    }
    return json_copy(node);
}

/**/

char *sanitize_moonshot_schema(const char *schema_json) {
    if (!schema_json || !schema_json[0]) {
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }

    char *err = NULL;
    json_t *parsed = json_parse(schema_json, &err);
    if (!parsed) {
        free(err);
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }
    free(err);

    json_t *sanitized = sanitize_node(parsed, 0);
    json_free(parsed);

    if (!sanitized) {
        json_t *empty = json_object();
        char *result = json_serialize(empty);
        json_free(empty);
        return result;
    }

    char *result = json_serialize(sanitized);
    json_free(sanitized);
    return result;
}

/* Port of Python agent/moonshot_schema.py:sanitize_moonshot_tool_parameters(). */
char *sanitize_moonshot_tool_parameters(const char *parameters_json) {
    char *cleaned = sanitize_moonshot_schema(parameters_json);
    if (!cleaned || strcmp(cleaned, "{}") == 0) {
        free(cleaned);
        return strdup("{\"type\":\"object\",\"properties\":{}}");
    }
    return cleaned;
}

/* Port of Python agent/moonshot_schema.py:sanitize_moonshot_tools().
 * Apply sanitize_moonshot_tool_parameters to every tool's parameters field.
 * Returns a new JSON string (caller must free), or strdup(input) if no change. */
char *sanitize_moonshot_tools(const char *tools_json) {
    if (!tools_json || !tools_json[0])
        return strdup("[]");
    json_t *tools = json_parse(tools_json, NULL);
    if (!tools || tools->type != JSON_ARRAY)
        return strdup(tools_json);
    size_t n = json_len(tools);
    json_t *result = json_array();
    bool any_change = false;
    for (size_t i = 0; i < n; i++) {
        json_t *tool = json_get(tools, i);
        if (!tool || tool->type != JSON_OBJECT) {
            json_append(result, json_copy(tool));
            continue;
        }
        json_t *fn = json_obj_get(tool, "function");
        if (!fn || fn->type != JSON_OBJECT) {
            json_append(result, json_copy(tool));
            continue;
        }
        json_t *params = json_obj_get(fn, "parameters");
        char *params_str = params ? json_serialize(params) : NULL;
        char *repaired = sanitize_moonshot_tool_parameters(params_str ? params_str : "{}");
        free(params_str);
        json_t *repaired_json = json_parse(repaired, NULL);
        if (repaired_json) {
            /* Build new_fn = {function: {..., parameters: repaired}, ...} */
            json_t *new_fn = json_object();
            for (size_t k = 0; k < fn->c.count; k++) {
                if (fn->c.keys[k] && strcmp(fn->c.keys[k], "parameters") != 0)
                    json_set(new_fn, fn->c.keys[k], json_copy(fn->c.items[k]));
            }
            json_set(new_fn, "parameters", repaired_json);
            json_t *new_tool = json_object();
            for (size_t k = 0; k < tool->c.count; k++) {
                if (tool->c.keys[k] && strcmp(tool->c.keys[k], "function") != 0)
                    json_set(new_tool, tool->c.keys[k], json_copy(tool->c.items[k]));
            }
            json_set(new_tool, "function", new_fn);
            json_append(result, new_tool);
            any_change = true;
        }
        free(repaired);
    }
    json_free(tools);
    if (!any_change) {
        json_free(result);
        return strdup(tools_json);
    }
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* Port of Python agent/moonshot_schema.py:is_moonshot_model().
 * True for any Kimi / Moonshot model slug. Checks tail of /-delimited
 * path for "kimi-" prefix, or "moonshot" anywhere in the string. */
bool is_moonshot_model(const char *model) {
    if (!model || !model[0]) return false;
    /* Copy and lowercase for case-insensitive comparison */
    char buf[256];
    size_t len = strlen(model);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)model[i]);
    buf[len] = '\0';
    /* Last path segment */
    const char *tail = strrchr(buf, '/');
    tail = tail ? tail + 1 : buf;
    if (strncmp(tail, "kimi-", 5) == 0 || strcmp(tail, "kimi") == 0)
        return true;
    /* Vendor-prefixed forms */
    if (strstr(buf, "moonshot") || strstr(buf, "/kimi"))
        return true;
    return false;
}
