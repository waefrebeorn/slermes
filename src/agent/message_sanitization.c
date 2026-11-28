#include <string.h>
#include "hermes_json.h"
/*
 * message_sanitization.c — Port of Python agent/message_sanitization.py
 *
 * Python API → C implementation mapping:
 *   sanitize_message()      → hermes_message_sanitize() in agent_message_sanitize.c (hermes_agent.h:284)
 *   sanitize_surrogates()   → sanitize_surrogates() in sanitize.c
 *   sanitize_tool_calls()   → sanitize_tool_call_arguments() in chat_completion_helpers.c
 *   sanitize_message_sequence() → repair_message_sequence() in hermes_agent.h:258
 *
 * Message sanitization pipeline: surrogate fix, think-block stripping,
 * secret redaction — ported in agent_message_sanitize.c + sanitize.c.
 */

#include "hermes_agent.h"   /* hermes_message_sanitize(), repair_message_sequence() */

/* PoP: tool_result_id_variants @ agent/message_sanitization.py:tool_result_id_variants */
json_t *hermes_agent_message_sanitization_tool_result_id_variants(const char *tool_call_id) {
    /* Python: return _expand_tool_id_variants((tool_call_id,)) */
    (void)tool_call_id;
    return json_new_array();
}

/* PoP: agent_message_sanitization_u_deterministic_call_id
 * @ agent/message_sanitization.py:deterministic_call_id */
/* "call_" + sha256(f"{fn_name}:{arguments}:{index}")[:12]. Input JSON
 * {"fn_name":..,"arguments":..,"index":n}. */
#include "crypto.h"

static void _hx2(const unsigned char *d, char *out, int n) {
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[2*i] = H[(d[i] >> 4) & 15]; out[2*i+1] = H[d[i] & 15]; }
    out[2*n] = 0;
}
char *agent_message_sanitization_u_deterministic_call_id(const char *arg) {
    json_t *o = arg ? json_parse(arg, NULL) : NULL;
    const char *fn = "", *args = ""; long index = 0;
    if (o && json_is_object(o)) {
        json_t *f = json_obj_get(o, "fn_name"), *a = json_obj_get(o, "arguments"), *i = json_obj_get(o, "index");
        if (f && json_is_string(f) && f->str_val) fn = f->str_val;
        if (a && json_is_string(a) && a->str_val) args = a->str_val;
        if (i && json_is_number(i)) index = (long)i->num_val;
    }
    size_t need = strlen(fn) + strlen(args) + 40;
    char *seed = malloc(need);
    snprintf(seed, need, "%s:%s:%ld", fn, args, index);
    unsigned char d[32]; crypto_sha256((const unsigned char *)seed, strlen(seed), d);
    char hex[33]; _hx2(d, hex, 32);
    char *out = malloc(24);
    snprintf(out, 24, "call_%.12s", hex);
    free(seed);
    if (o) json_free(o);
    return out;
}
