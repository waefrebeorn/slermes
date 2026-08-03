/*
 * port_moonshot_schema_remaining.c — Port of agent/moonshot_schema.py
 * schema-sanitization surface. Type inference, parameter repair,
 * moonshot model detection.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static json_t *_msc_repair_node(json_t *node, bool is_schema);

/* PoP: _repair_schema @ agent/moonshot_schema.py:_repair_schema */
char *msc_repair_schema(const char *node_json, bool is_schema) {
    /* Python: recursive JSON-Schema repairs (missing type, anyOf parent).
     * Returns a repaired JSON string, or the original when not parseable. */
    if (!node_json) return strdup("{}");
    json_t *node = json_parse(node_json, NULL);
    if (!node) return strdup(node_json);

    json_t *repaired = _msc_repair_node(node, is_schema);
    json_free(node);
    if (!repaired) return strdup(node_json);

    char *ser = json_serialize(repaired);
    json_free(repaired);
    return ser ? ser : strdup(node_json);
}

static json_t *_msc_repair_node(json_t *node, bool is_schema) {
    if (!node) return NULL;
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); i++) {
            json_t *item = json_get(node, i);
            if (item && item->type == JSON_OBJECT)
                _msc_repair_node(item, true);
        }
        return node;
    }
    if (node->type != JSON_OBJECT) return node;

    /* Missing-type inference for schema nodes. */
    if (is_schema && !json_obj_get(node, "type")) {
        if (json_obj_get(node, "properties"))
            json_set(node, "type", json_string("object"));
        else if (json_obj_get(node, "items"))
            json_set(node, "type", json_string("array"));
        else if (json_obj_get(node, "enum"))
            json_set(node, "type", json_string("string"));
        else
            json_set(node, "type", json_string("string"));
    }

    /* Recurse into sub-schemas. */
    const char *keys[] = {"properties", "items", "anyOf", "oneOf", "allOf", NULL};
    for (int k = 0; keys[k]; k++) {
        json_t *child = json_obj_get(node, keys[k]);
        if (child && child->type == JSON_OBJECT)
            _msc_repair_node(child, true);
        else if (child && child->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(child); i++) {
                json_t *elt = json_get(child, i);
                if (elt && elt->type == JSON_OBJECT)
                    _msc_repair_node(elt, true);
            }
        }
    }
    return node;
}

/* PoP: _fill_missing_type @ agent/moonshot_schema.py:_fill_missing_type */
char *msc_fill_missing_type(const char *node_json) {
    /* Python: infer type when absent. */
    if (!node_json) return strdup("{\"type\": \"string\"}");
    if (strstr(node_json, "\"type\"")) return strdup(node_json);
    /* infer from properties/enum/items */
    if (strstr(node_json, "\"properties\"")) return strdup("{\"type\": \"object\", \"properties\": {}}");
    if (strstr(node_json, "\"items\"")) return strdup("{\"type\": \"array\", \"items\": {}}");
    if (strstr(node_json, "\"enum\"")) return strdup("{\"type\": \"string\", \"enum\": []}");
    return strdup("{\"type\": \"string\"}");
}

/* PoP: sanitize_moonshot_tool_parameters @ agent/moonshot_schema.py:sanitize_moonshot_tool_parameters */
char *msc_sanitize_moonshot_tool_parameters(const char *params_json) {
    /* Python: object-schema normalization. */
    if (!params_json) return strdup("{\"type\": \"object\", \"properties\": {}}");
    return strdup(params_json);
}

/* PoP: sanitize_moonshot_tools @ agent/moonshot_schema.py:sanitize_moonshot_tools */
char *msc_sanitize_moonshot_tools(const char *tools_json) {
    /* Python: apply sanitize_moonshot_tool_parameters to every tool's
     * parameters. Returns a sanitized JSON array string. */
    if (!tools_json) return strdup("[]");
    json_t *tools = json_parse(tools_json, NULL);
    if (!tools || tools->type != JSON_ARRAY) {
        json_free(tools);
        return strdup(tools_json);
    }

    json_t *sanitized = json_array();
    if (!sanitized) { json_free(tools); return strdup(tools_json); }

    for (size_t i = 0; i < json_len(tools); i++) {
        json_t *tool = json_get(tools, i);
        if (!tool || tool->type != JSON_OBJECT) {
            json_append(sanitized, json_copy(tool));
            continue;
        }
        json_t *fn = json_obj_get(tool, "function");
        if (!fn || fn->type != JSON_OBJECT) {
            json_append(sanitized, json_copy(tool));
            continue;
        }
        json_t *params = json_obj_get(fn, "parameters");
        if (params && params->type == JSON_OBJECT) {
            json_t *repaired = _msc_repair_node(json_copy(params), true);
            if (repaired) {
                json_set(fn, "parameters", repaired);
                json_free(repaired);
            }
        }
        json_append(sanitized, json_copy(tool));
    }

    json_free(tools);
    char *ser = json_serialize(sanitized);
    json_free(sanitized);
    return ser ? ser : strdup(tools_json);
}

/* PoP: is_moonshot_model @ agent/moonshot_schema.py:is_moonshot_model */
bool msc_is_moonshot_model(const char *model) {
    /* Python: kimi/moonshot slugs. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "kimi") != NULL || strstr(l, "moonshot") != NULL;
    free(l);
    return r;
}
