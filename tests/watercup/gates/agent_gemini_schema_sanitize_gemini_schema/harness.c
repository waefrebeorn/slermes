/* gate: agent_gemini_schema_sanitize_gemini_schema — traced copy of
 * src/agent/gemini_schema.c::sanitize_gemini_schema (+ sanitize_node /
 * sanitize_object / enum_has_non_string), linked against the REAL libjson
 * (lib/libjson/json.c) that the port itself uses.
 * LIVE twin: agent/gemini_schema.py::sanitize_gemini_schema.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: top-level schema is a dict/object
 *   per node (recursive, R/r around recursion):
 *     LOOP over keys: L tick
 *       B2 key in allowed set
 *       B3 key == "properties"   B4 value is object -> recurse props
 *       B5 key == "items"        B6 value is object -> recurse
 *       B7 key == "anyOf"        B8 value is list
 *            LOOP items: L tick  B9 item is object -> recurse
 *     B10 enum present and list
 *     B11 type in {integer,number,boolean}
 *     B12 enum has non-string entry      (KNOWN C/Py DIVERGENCE when true —
 *      see gate.json notes; inputs avoid it)
 *     B13 required present and list
 *     B14 properties present and dict
 *       LOOP required names: L tick  B15 name is str and in properties
 *     B16 valid_required empty -> drop required   (C never does this —
 *      divergence avoided by inputs)
 *     B17 len(valid) != len(required)
 *
 * KNOWN DIVERGENCES (documented; inputs stay in the agreed region):
 *  - typed enums with non-string entries: Py stringifies, C drops whole key.
 *  - required names missing from properties: Py filters, C keeps as-is.
 *  - "items" holding a non-dict: Py returns {}, C copies verbatim.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../../../include/hermes_json.h"
#include "watercup.h"

static const char *ALLOWED_KEYS[] = {
    "type", "format", "title", "description", "nullable", "enum",
    "maxItems", "minItems", "properties", "required",
    "minProperties", "maxProperties", "minLength", "maxLength",
    "pattern", "example", "anyOf", "propertyOrdering", "default",
    "items", "minimum", "maximum", NULL
};

static bool is_allowed(const char *key) {
    for (int i = 0; ALLOWED_KEYS[i]; i++)
        if (strcmp(key, ALLOWED_KEYS[i]) == 0) return true;
    return false;
}

static bool enum_has_non_string(json_t *enum_arr) {
    if (!enum_arr || enum_arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < enum_arr->c.count; i++) {
        json_t *item = enum_arr->c.items[i];
        if (item->type != JSON_STRING) return true;
    }
    return false;
}

static json_t *trace_sanitize_node(json_t *node, wcup_t *c);

static json_t *trace_sanitize_object(json_t *obj, wcup_t *c) {
    wcup_rec_in(c);
    json_t *out = json_object();

    for (size_t i = 0; i < obj->c.count; i++) {
        wcup_loop(c);
        const char *key = obj->c.keys[i];
        json_t *val = obj->c.items[i];
        if (!key || !val) continue;
        if (!is_allowed(key)) { wcup_branch(c, 0); continue; }   /* B2 */
        wcup_branch(c, 1);

        if (strcmp(key, "properties") == 0) {                 /* B3 */
            wcup_branch(c, 1);
            if (val->type == JSON_OBJECT) {                   /* B4 */
                wcup_branch(c, 1);
                json_t *props = json_object();
                for (size_t j = 0; j < val->c.count; j++) {
                    wcup_loop(c);
                    const char *pkey = val->c.keys[j];
                    json_t *pval = val->c.items[j];
                    if (!pkey || !pval) continue;
                    wcup_rec_in(c);
                    json_t *sanitized = trace_sanitize_node(pval, c);
                    wcup_rec_out(c);
                    if (sanitized) json_set(props, pkey, sanitized);
                }
                json_set(out, key, props);
            } else {
                wcup_branch(c, 0);   /* divergent path — inputs avoid */
                json_set(out, key, json_copy(val));
            }
        } else { wcup_branch(c, 0); }

        if (strcmp(key, "items") == 0) {                      /* B5 */
            wcup_branch(c, 1);
            if (val->type == JSON_OBJECT) {                   /* B6 */
                wcup_branch(c, 1);
                wcup_rec_in(c);
                json_t *sanitized = trace_sanitize_node(val, c);
                wcup_rec_out(c);
                if (sanitized) json_set(out, key, sanitized);
            } else {
                wcup_branch(c, 0);   /* divergent path — inputs avoid */
                json_set(out, key, json_copy(val));
            }
        } else { wcup_branch(c, 0); }

        if (strcmp(key, "anyOf") == 0) {                      /* B7 */
            wcup_branch(c, 1);
            if (val->type == JSON_ARRAY) {                    /* B8 */
                wcup_branch(c, 1);
                json_t *arr = json_array();
                for (size_t j = 0; j < val->c.count; j++) {
                    wcup_loop(c);
                    json_t *item = val->c.items[j];
                    if (item && item->type == JSON_OBJECT) {  /* B9 */
                        wcup_branch(c, 1);
                        wcup_rec_in(c);
                        json_t *sanitized = trace_sanitize_node(item, c);
                        wcup_rec_out(c);
                        if (sanitized) json_append(arr, sanitized);
                    } else {
                        wcup_branch(c, 0);
                    }
                }
                json_set(out, key, arr);
            } else {
                wcup_branch(c, 0);   /* divergent path — inputs avoid */
                json_set(out, key, json_copy(val));
            }
        } else { wcup_branch(c, 0); }

        if (strcmp(key, "properties") != 0 && strcmp(key, "items") != 0 &&
            strcmp(key, "anyOf") != 0)
            json_set(out, key, json_copy(val));   /* plain leaf copy */
    }

    /* Typed-enum rule. */
    json_t *enum_val = json_obj_get(out, "enum");
    json_t *type_val = json_obj_get(out, "type");
    bool enum_is_list = enum_val && enum_val->type == JSON_ARRAY;   /* B10 */
    wcup_branch(c, enum_is_list);
    bool typed = false;
    if (enum_is_list && type_val && type_val->type == JSON_STRING) {
        const char *t = type_val->str_val;
        typed = (strcmp(t, "integer") == 0 || strcmp(t, "number") == 0 ||
                 strcmp(t, "boolean") == 0);                       /* B11 */
    }
    wcup_branch(c, typed);
    if (typed) {
        bool nonstr = enum_has_non_string(enum_val);               /* B12 */
        wcup_branch(c, nonstr);
        if (nonstr)
            json_obj_del(out, "enum");   /* C drops whole key on non-strings */
    }

    /* Required-vs-properties filter (Py-only behavior; C keeps required). */
    json_t *required_val = json_obj_get(out, "required");
    bool req_is_list = required_val && required_val->type == JSON_ARRAY; /* B13 */
    wcup_branch(c, req_is_list);
    if (req_is_list) {
        json_t *props_val = json_obj_get(out, "properties");
        bool props_is_dict = props_val && props_val->type == JSON_OBJECT; /* B14 */
        wcup_branch(c, props_is_dict);
        size_t nvalid = 0, ntotal = 0;
        for (size_t j = 0; j < required_val->c.count; j++) {
            wcup_loop(c);
            json_t *nm = required_val->c.items[j];
            bool ok = nm && nm->type == JSON_STRING && props_is_dict &&
                      json_obj_get(props_val, nm->str_val) != NULL;  /* B15 */
            wcup_branch(c, ok);
            if (ok) nvalid++;
            ntotal++;
        }
        wcup_branch(c, nvalid == 0);                     /* B16 (Py drops) */
        wcup_branch(c, nvalid != ntotal);                /* B17 (Py replaces) */
        (void)nvalid; (void)ntotal;   /* C keeps required unfiltered */
    }

    wcup_rec_out(c);
    return out;
}

static json_t *trace_sanitize_node(json_t *node, wcup_t *c) {
    if (!node) return NULL;
    switch (node->type) {
        case JSON_OBJECT:
            return trace_sanitize_object(node, c);
        case JSON_ARRAY: {
            json_t *arr = json_array();
            for (size_t i = 0; i < node->c.count; i++) {
                wcup_loop(c);
                wcup_rec_in(c);
                json_t *sanitized = trace_sanitize_node(node->c.items[i], c);
                wcup_rec_out(c);
                if (sanitized) json_append(arr, sanitized);
            }
            return arr;
        }
        default:
            return json_copy(node);
    }
}

static void wc_sanitize_gemini_schema(const char *schema_json) {
    WCUP_BEGIN("sanitize_gemini_schema");
    json_t *parsed = NULL;
    if (schema_json && schema_json[0])
        parsed = json_parse(schema_json, NULL);
    bool is_obj = parsed && parsed->type == JSON_OBJECT;          /* B1 */
    WCUP_BRANCH(is_obj);
    if (is_obj) {
        json_t *sanitized = trace_sanitize_node(parsed, &_wc);
        json_free(sanitized);   /* result not printed — one cup line per input only */
    }
    if (parsed) json_free(parsed);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("sanitize_gemini_schema");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s sanitize_gemini_schema <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_sanitize_gemini_schema(argv[i]);
    return 0;
}
