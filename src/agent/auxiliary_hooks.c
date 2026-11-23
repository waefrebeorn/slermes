/*
 * auxiliary_hooks.c — C home for upstream Hermes agent/auxiliary_hooks.py.
 * PoP: w7_auxiliaryho_d1a407_constants @ agent/auxiliary_hooks.py:PRE/POST_AUXILIARY_CALL
 * PoP: w7_auxiliaryho_d1a407_parent_turn_identity @ agent/auxiliary_hooks.py:_parent_turn_identity
 * PoP: w7_auxiliaryho_d1a407_system_prompt @ agent/auxiliary_hooks.py:_system_prompt
 * PoP: w7_auxiliaryho_d1a407_first_choice_message @ agent/auxiliary_hooks.py:_first_choice_message
 * PoP: w7_auxiliaryho_d1a407_field @ agent/auxiliary_hooks.py:_field
 * PoP: w7_auxiliaryho_d1a407_message_count @ agent/auxiliary_hooks.py:_AuxCallHooks.__init__
 * PoP: w7_auxiliaryho_d1a407_retry_counts @ agent/auxiliary_hooks.py:_AuxCallHooks.__init__
 *
 * current_turn(), hook registration and time.time() are process state; the
 * turn / kwargs / raw retry value are injected from the fixture instead.
 */
#include "noc_w7_auxiliaryho_d1a407.h"
#include <stdlib.h>
#include <string.h>

static const char *jstr(json_t *v) { return (v && v->type == JSON_STRING) ? v->str_val : NULL; }
static const char *jstr_or(json_t *v, const char *def) {
    const char *s = jstr(v);
    return s ? s : def;
}
static json_t *jcopy_or_null(json_t *v) { return v ? json_copy(v) : json_null(); }
static int truthy(json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    if (v->type == JSON_ARRAY || v->type == JSON_OBJECT) return json_object_size(v) || json_len(v) ? 1 : 0;
    return 1;
}

json_t *w7_auxiliaryho_d1a407_constants(void) {
    json_t *o = json_object();
    json_set(o, "PRE_AUXILIARY_CALL", json_string("pre_auxiliary_call"));
    json_set(o, "POST_AUXILIARY_CALL", json_string("post_auxiliary_call"));
    return o;
}
json_t *w7_auxiliaryho_d1a407_parent_turn_identity(json_t *turn) {
    json_t *out = json_object();
    json_t *lease = NULL;
    if (turn && turn->type == JSON_OBJECT) lease = json_obj_get(turn, "lease");
    json_set(out, "session_id", json_string(jstr_or(lease ? json_obj_get(lease, "session_id") : NULL, "")));
    json_set(out, "platform", json_string(jstr_or(lease ? json_obj_get(lease, "platform") : NULL, "")));
    json_set(out, "task_id", json_string(jstr_or(turn ? json_obj_get(turn, "task_id") : NULL, "")));
    json_set(out, "turn_id", json_string(jstr_or(turn ? json_obj_get(turn, "turn_id") : NULL, "")));
    return out;
}
json_t *w7_auxiliaryho_d1a407_system_prompt(json_t *messages, json_t *kwargs) {
    const char *s;
    json_t *first;
    if (kwargs && kwargs->type == JSON_OBJECT) {
        s = jstr(json_obj_get(kwargs, "system"));
        if (s) return json_string(s);
        s = jstr(json_obj_get(kwargs, "instructions"));
        if (s) return json_string(s);
    }
    if (messages && messages->type == JSON_ARRAY && json_len(messages) > 0) {
        first = json_get(messages, 0);
        if (first && first->type == JSON_OBJECT) {
            const char *role = jstr(json_obj_get(first, "role"));
            const char *content = jstr(json_obj_get(first, "content"));
            if (role && !strcmp(role, "system") && content) return json_string(content);
        }
    }
    return json_string("");
}
json_t *w7_auxiliaryho_d1a407_first_choice_message(json_t *response) {
    json_t *out = json_object();
    json_t *choices = NULL, *choice = NULL;
    if (response && response->type == JSON_OBJECT) choices = json_obj_get(response, "choices");
    if (choices && choices->type == JSON_ARRAY && json_len(choices) > 0) choice = json_get(choices, 0);
    if (choice && choice->type == JSON_OBJECT) {
        json_set(out, "message", jcopy_or_null(json_obj_get(choice, "message")));
        json_set(out, "finish_reason", jcopy_or_null(json_obj_get(choice, "finish_reason")));
    } else {
        json_set(out, "message", json_null());
        json_set(out, "finish_reason", json_null());
    }
    return out;
}
json_t *w7_auxiliaryho_d1a407_field(json_t *obj, const char *name) {
    if (obj && obj->type == JSON_OBJECT) return jcopy_or_null(json_obj_get(obj, name));
    return json_null();
}
json_t *w7_auxiliaryho_d1a407_message_count(json_t *messages, json_t *input_value) {
    json_t *src = truthy(messages) ? messages : (truthy(input_value) ? input_value : NULL);
    size_t n = 0;
    if (src) {
        if (src->type == JSON_ARRAY || src->type == JSON_OBJECT) n = json_object_size(src) ? json_object_size(src) : json_len(src);
        else if (src->type == JSON_STRING) n = strlen(src->str_val ? src->str_val : "");
    }
    return json_number((double)n);
}
json_t *w7_auxiliaryho_d1a407_retry_counts(json_t *raw) {
    long long v = 0;
    json_t *out;
    if (raw) {
        if (raw->type == JSON_STRING && raw->str_val) v = strtoll(raw->str_val, NULL, 10);
        else if (raw->type == JSON_NUMBER) v = (long long)raw->num_val;
        else if (raw->type == JSON_BOOL) v = raw->bool_val ? 1 : 0;
    }
    out = json_object();
    json_set(out, "retry", json_number((double)v));
    json_set(out, "call", json_number((double)(v + 1)));
    return out;
}
