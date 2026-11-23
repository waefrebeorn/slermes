/*
 * update_finish.c — faithful C11 port of hermes_cli/update_finish.py.
 * Gate w11_updatefini_fefa40.
 *
 * PoP: w11_updatefini_fefa40_restore_plan @ hermes_cli/update_finish.py:_restore_plan
 * PoP: w11_updatefini_fefa40_tail_owed @ hermes_cli/update_finish.py:finish_update
 * PoP: w11_updatefini_fefa40_resume_handled @ hermes_cli/update_finish.py:main
 */
#include "hermes_json.h"
#include "noc_w11_updatefini_fefa40.h"
#include <string.h>

static int is_falsy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   return !v->bool_val;
        case JSON_NUMBER: return v->num_val == 0.0;
        case JSON_STRING: return !v->str_val || !v->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count == 0;
        default: return 1;
    }
}

static int in_string_array(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

json_t *w11_updatefini_fefa40_restore_plan(const json_t *data,
                                           const json_t *plan_fields,
                                           const json_t *rt_fields) {
    if (is_falsy(data)) return NULL; /* if not data: return None */
    json_t *values = json_object();
    if (plan_fields && plan_fields->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(plan_fields); i++) {
            json_t *fn = json_get(plan_fields, i);
            if (!fn || fn->type != JSON_STRING || !fn->str_val) continue;
            json_t *dv = json_obj_get(data, fn->str_val);
            if (dv) json_set(values, fn->str_val, json_copy(dv)); /* field.name in data */
        }
    }
    json_t *rts = json_array();
    json_t *rows = json_obj_get(data, "runtimes");
    if (rows && rows->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(rows); i++) {
            json_t *row = json_get(rows, i);
            json_t *filtered = json_object();
            if (row && row->type == JSON_OBJECT) {
                for (size_t j = 0; j < json_object_size(row); j++) {
                    const char *k = json_object_get_key_at(row, j);
                    if (k && in_string_array(rt_fields, k))
                        json_set(filtered, k, json_copy(json_object_get_at(row, j)));
                }
            }
            json_append(rts, filtered);
        }
    }
    json_set(values, "runtimes", rts); /* always set (replaces an earlier copy) */
    return values;
}

int w11_updatefini_fefa40_tail_owed(const json_t *owed,
                                    const json_t *tail_followups) {
    if (!owed || owed->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(owed); i++) {
        json_t *pair = json_get(owed, i);
        if (!pair || pair->type != JSON_ARRAY) continue;
        json_t *step = json_get(pair, 0);
        if (step && step->type == JSON_STRING && in_string_array(tail_followups, step->str_val))
            return 1;
    }
    return 0;
}

int w11_updatefini_fefa40_resume_handled(int attempted, const json_t *token) {
    if (attempted) return 1;
    if (!token || token->type == JSON_NULL) return 1; /* not token */
    json_t *rn = json_obj_get(token, "resume_needed");
    if (!rn || rn->type == JSON_NULL) return 1;
    switch (rn->type) {
        case JSON_BOOL:   return !rn->bool_val;
        case JSON_NUMBER: return rn->num_val == 0.0;
        case JSON_STRING: return !rn->str_val || !rn->str_val[0];
        case JSON_ARRAY:
        case JSON_OBJECT: return rn->c.count == 0;
        default: return 1;
    }
}
