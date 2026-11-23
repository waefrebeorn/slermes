/*
 * turn_empty_response.c — C home for upstream Hermes agent/turn_empty_response.py.
 *
 * Faithful C11 ports of the pure empty-response helpers: latest user display
 * kind lookup, machinery-turn quiet-exit detection, and the outgoing-tail
 * retry-hint decoration (returned together with the mutated message list).
 *
 * PoP: w11_turnempty_94bdef_latest_display @ agent/turn_empty_response.py:_latest_user_display_kind
 * PoP: w11_turnempty_94bdef_machinery_quiet @ agent/turn_empty_response.py:_notification_turn_without_tool_round
 * PoP: w11_turnempty_94bdef_retry_hint @ agent/turn_empty_response.py:add_empty_response_retry_hint
 */
#include "noc_w11_turnempty_94bdef.h"

#include <stdlib.h>
#include <string.h>

static const char *const MACHINERY_KINDS[] = {
    "internal_notification", "process_complete", "async_delegation_complete"
};
static int is_machinery_kind(json_t *v) {
    if (!json_is_string(v) || !v->str_val) return 0;
    for (size_t i = 0; i < sizeof(MACHINERY_KINDS) / sizeof(MACHINERY_KINDS[0]); i++)
        if (!strcmp(v->str_val, MACHINERY_KINDS[i])) return 1;
    return 0;
}
static const char *role_of(json_t *msg) {
    if (!json_is_object(msg)) return NULL;
    json_t *r = json_obj_get(msg, "role");
    return (json_is_string(r) && r->str_val) ? r->str_val : NULL;
}
static int py_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* PoP: w11_turnempty_94bdef_latest_display @ agent/turn_empty_response.py:_latest_user_display_kind */
json_t *w11_turnempty_94bdef_latest_display(json_t *a) {
    json_t *msgs = a ? json_obj_get(a, "messages") : NULL;
    if (!json_is_array(msgs)) return json_null();
    for (size_t i = json_len(msgs); i > 0; i--) {
        json_t *m = json_get(msgs, i - 1);
        const char *role = role_of(m);
        if (role && !strcmp(role, "user")) {
            json_t *dk = json_obj_get(m, "display_kind");
            return dk ? json_copy(dk) : json_null();
        }
    }
    return json_null();
}

/* PoP: w11_turnempty_94bdef_machinery_quiet @ agent/turn_empty_response.py:_notification_turn_without_tool_round */
json_t *w11_turnempty_94bdef_machinery_quiet(json_t *a) {
    json_t *msgs = a ? json_obj_get(a, "messages") : NULL;
    if (!json_is_array(msgs)) return json_bool(false);
    for (size_t i = json_len(msgs); i > 0; i--) {
        json_t *m = json_get(msgs, i - 1);
        if (!json_is_object(m)) continue;
        const char *role = role_of(m);
        if (role && !strcmp(role, "tool")) return json_bool(false);
        if (role && !strcmp(role, "user"))
            return json_bool(is_machinery_kind(json_obj_get(m, "display_kind")) ? true : false);
    }
    return json_bool(false);
}

/* PoP: w11_turnempty_94bdef_retry_hint @ agent/turn_empty_response.py:add_empty_response_retry_hint */
json_t *w11_turnempty_94bdef_retry_hint(json_t *a) {
    json_t *out = json_object();
    json_t *msgs = a ? json_obj_get(a, "api_messages") : NULL;
    json_t *hint = a ? json_obj_get(a, "hint") : NULL;
    json_t *msgs_copy = msgs ? json_copy(msgs) : json_array();
    json_set(out, "messages", msgs_copy);
    json_set(out, "suffix", json_string(""));
    if (!py_truthy(hint) || !json_is_array(msgs_copy) || !json_len(msgs_copy)) return out;
    json_t *tail = json_get(msgs_copy, json_len(msgs_copy) - 1);
    const char *role = role_of(tail);
    if (!role || (strcmp(role, "user") && strcmp(role, "tool"))) return out;

    const char *hs = json_string_value(hint);
    size_t hl = hs ? strlen(hs) : 0;
    char *suffix = (char *)malloc(hl + 3);
    if (!suffix) return out;
    suffix[0] = '\n'; suffix[1] = '\n';
    if (hl) memcpy(suffix + 2, hs, hl);
    suffix[hl + 2] = '\0';

    json_t *content = json_obj_get(tail, "content");
    json_t *new_content;
    if (json_is_array(content)) {
        new_content = json_copy(content);
        json_append(new_content, json_object());
        json_t *part = json_get(new_content, json_len(new_content) - 1);
        json_set(part, "type", json_string("text"));
        json_set(part, "text", json_string(suffix));
    } else {
        const char *base = (json_is_string(content) && content->str_val) ? content->str_val : "";
        size_t n = strlen(base) + hl + 2;
        char *joined = (char *)malloc(n + 1);
        if (joined) {
            memcpy(joined, base, strlen(base));
            memcpy(joined + strlen(base), suffix, hl + 2);
            joined[n] = '\0';
            new_content = json_string(joined);
            free(joined);
        } else new_content = json_string(suffix);
    }
    json_t *new_tail = json_copy(tail);
    json_set(new_tail, "content", new_content);
    json_array_set(msgs_copy, json_len(msgs_copy) - 1, new_tail);
    json_set(out, "suffix", json_string(suffix));
    free(suffix);
    return out;
}
