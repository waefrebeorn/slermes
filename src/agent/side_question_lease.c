/*
 * side_question_lease.c — C home for upstream Hermes agent/side_question_lease.py.
 *
 * Faithful C11 ports of the pure lease-eligibility helpers: the unattended
 * turn test and the refusal-reason classifier used before a lease is spent.
 * Lease/clock state is injected through the args object (no live wall clock,
 * no weak references, no captured contextvars).
 *
 * PoP: w11_sidequesti_649034_unattended @ agent/side_question_lease.py:is_unattended_turn
 * PoP: w11_sidequesti_649034_redeemable @ agent/side_question_lease.py:_redeemable
 */
#include "noc_w11_sidequesti_649034.h"

#include <string.h>

#define MAX_PROMPT_CHARS 4096

static int json_equal(json_t *x, json_t *y) {
    if (!x || !y) return x == y;
    if (x->type != y->type) return 0;
    switch (x->type) {
        case JSON_NULL:   return 1;
        case JSON_BOOL:   return x->bool_val == y->bool_val;
        case JSON_NUMBER: return x->num_val == y->num_val;
        case JSON_STRING: return !strcmp(x->str_val ? x->str_val : "", y->str_val ? y->str_val : "");
        default:          return 0;
    }
}

static int blank_prompt(const char *s) {
    if (!s) return 1;
    static const char *ws = " \t\n\r\f\v";
    return strspn(s, ws) == strlen(s);
}

/* PoP: w11_sidequesti_649034_unattended @ agent/side_question_lease.py:is_unattended_turn */
json_t *w11_sidequesti_649034_unattended(json_t *a) {
    if (!a) return json_bool(false);
    json_t *disabled = json_obj_get(a, "persist_disabled");
    json_t *platform = json_obj_get(a, "platform");
    json_t *depth = json_obj_get(a, "delegate_depth");
    int unattended =
        (disabled && json_is_bool(disabled) && disabled->bool_val) ||
        (platform && json_is_string(platform) && platform->str_val && !strcmp(platform->str_val, "cron")) ||
        (depth && json_is_number(depth) && depth->num_val > 0);
    return json_bool(unattended ? true : false);
}

/* PoP: w11_sidequesti_649034_redeemable @ agent/side_question_lease.py:_redeemable */
json_t *w11_sidequesti_649034_redeemable(json_t *a) {
    json_t *lease = a ? json_obj_get(a, "lease") : NULL;
    if (!json_is_object(lease)) return json_string("unknown_token");
    json_t *expired = json_obj_get(lease, "expired");
    if (expired && json_is_bool(expired) && expired->bool_val) return json_string("expired");
    json_t *gone = json_obj_get(lease, "parent_gone");
    if (gone && json_is_bool(gone) && gone->bool_val) return json_string("parent_gone");
    json_t *pturn = json_obj_get(lease, "parent_turn_id");
    json_t *turn = json_obj_get(lease, "turn_id");
    if (!json_equal(pturn, turn)) return json_string("turn_moved_on");
    json_t *prompt = a ? json_obj_get(a, "prompt") : NULL;
    double max_chars = a ? json_get_num(a, "max_prompt_chars", (double)MAX_PROMPT_CHARS) : (double)MAX_PROMPT_CHARS;
    if (!json_is_string(prompt) || !prompt->str_val || blank_prompt(prompt->str_val) ||
        (double)strlen(prompt->str_val) > max_chars)
        return json_string("bad_prompt");
    return json_null();
}
