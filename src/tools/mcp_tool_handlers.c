/*
 * mcp_tool_handlers.c — C home for upstream Hermes tools/mcp_tool_handlers.py.
 * PoP: w10_mcptoolha_1fdc12_trust_approval_key @ tools/mcp_tool_handlers.py:_trust_approval_key
 * PoP: w10_mcptoolha_1fdc12_result_is_error @ tools/mcp_tool_handlers.py:_result_is_error
 * PoP: w10_mcptoolha_1fdc12_json_type_stable_equal @ tools/mcp_tool_handlers.py:_json_type_stable_equal
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_mcptoolha_1fdc12.h"

char *w10_mcptoolha_1fdc12_trust_approval_key(const char *server_name, const char *tool_name) {
    char out[2048];
    snprintf(out, sizeof(out), "mcp:%s/%s", server_name ? server_name : "",
             tool_name ? tool_name : "");
    return strdup(out);
}

int w10_mcptoolha_1fdc12_result_is_error(const char *result_text) {
    if (!result_text) return 0;
    json_t *v = json_parse(result_text, NULL);
    if (!v) return 0;
    int rc = 0;
    if (v->type == JSON_OBJECT) {
        rc = json_obj_get(v, "error") ? 1 : 0;
    } else if (v->type == JSON_ARRAY) {
        for (size_t i = 0; i < v->c.count; i++) {
            json_t *it = json_array_get(v, i);
            if (it && it->type == JSON_STRING && it->str_val && !strcmp(it->str_val, "error")) {
                rc = 1; break;
            }
        }
    } else if (v->type == JSON_STRING) {
        rc = v->str_val && strstr(v->str_val, "error") ? 1 : 0;
    }
    /* number/bool/null: `"error" in <non-container>` raises TypeError -> False */
    json_free(v);
    return rc;
}

static int steq(json_t *a, json_t *b) {
    if (!a || !b) return a == b;
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        size_t na = json_object_size(a), nb = json_object_size(b);
        if (na != nb) return 0;
        for (size_t i = 0; i < na; i++) {
            const char *k = json_object_get_key_at(a, i);
            json_t *vb = json_obj_get(b, k);
            if (!vb || !steq(json_object_get_at(a, i), vb)) return 0;
        }
        return 1;
    }
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        size_t na = json_array_size(a), nb = json_array_size(b);
        if (na != nb) return 0;
        for (size_t i = 0; i < na; i++)
            if (!steq(json_array_get(a, i), json_array_get(b, i))) return 0;
        return 1;
    }
    if (a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && !strcmp(a->str_val, b->str_val);
        default: return 0;
    }
}

int w10_mcptoolha_1fdc12_json_type_stable_equal(json_t *left, json_t *right) {
    return steq(left, right);
}
