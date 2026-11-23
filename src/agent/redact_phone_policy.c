/*
 * redact_phone_policy.c — C home for upstream Hermes agent/redact_phone_policy.py.
 *
 * Faithful C11 port of the chat-egress phone-number policy.  The real loader
 * (`load_config_readonly`) and the two downstream redactors are injected by
 * the oracle; what is ported here is the policy module's own contract:
 *   - unreadable config -> masked (fails closed)
 *   - only a bool false under security.redact_phone_numbers opts out
 *   - any other value, missing key or empty security -> masked
 *   - the decided keep flag is passed to both egress scrubs.
 *
 * PoP: w11_redactphon_b15391_chat_keeps_phone_numbers @ agent/redact_phone_policy.py:chat_keeps_phone_numbers
 * PoP: w11_redactphon_b15391_redact_for_chat_egress @ agent/redact_phone_policy.py:redact_for_chat_egress
 * PoP: w11_redactphon_b15391_redact_assistant_reply @ agent/redact_phone_policy.py:redact_assistant_reply
 */
#include "noc_w11_redactphon_b15391.h"

/* (load_config_readonly().get("security") or {}).get("redact_phone_numbers", True)
 * then: bool -> not raw; anything else (or a read failure) -> False. */
static int keeps_phone_numbers(json_t *a) {
    if (json_get_bool(a, "config_error", false)) return 0;
    json_t *sec = json_obj_get(a, "security");
    json_t *raw = NULL;
    if (json_is_object(sec) && json_object_size(sec) > 0)
        raw = json_obj_get(sec, "redact_phone_numbers");
    if (!raw) return 0;                       /* default True -> not True = False */
    if (raw->type == JSON_BOOL) return raw->bool_val ? 0 : 1;
    return 0;                                  /* non-bool keeps numbers masked */
}

static json_t *mirror(json_t *a) {
    json_t *o = json_object();
    json_set(o, "keep", json_bool(keeps_phone_numbers(a) ? true : false));
    json_t *t = json_obj_get(a, "text");
    json_set(o, "text", t ? json_copy(t) : json_string(""));
    return o;
}

/* PoP: w11_redactphon_b15391_chat_keeps_phone_numbers @ agent/redact_phone_policy.py:chat_keeps_phone_numbers */
json_t *w11_redactphon_b15391_chat_keeps_phone_numbers(json_t *a) {
    return json_bool(keeps_phone_numbers(a) ? true : false);
}

/* PoP: w11_redactphon_b15391_redact_for_chat_egress @ agent/redact_phone_policy.py:redact_for_chat_egress */
json_t *w11_redactphon_b15391_redact_for_chat_egress(json_t *a) {
    return mirror(a);
}

/* PoP: w11_redactphon_b15391_redact_assistant_reply @ agent/redact_phone_policy.py:redact_assistant_reply */
json_t *w11_redactphon_b15391_redact_assistant_reply(json_t *a) {
    return mirror(a);
}
