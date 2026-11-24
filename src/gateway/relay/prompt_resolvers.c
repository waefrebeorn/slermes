/*
 * prompt_resolvers.c -- real C11 port of the decisions of upstream Hermes
 *   gateway/relay/prompt_resolvers.py.
 *
 * PoP: w22_promptreso_7bfabf_actor @ gateway/relay/prompt_resolvers.py:prompt_actor
 * PoP: w22_promptreso_7bfabf_refusal_plan @ gateway/relay/prompt_resolvers.py:prompt_refusal
 * PoP: w22_promptreso_7bfabf_exec_ack @ gateway/relay/prompt_resolvers.py:resolve_exec_approval
 * PoP: w22_promptreso_7bfabf_slash_ack @ gateway/relay/prompt_resolvers.py:resolve_slash_confirm
 * PoP: w22_promptreso_7bfabf_clarify @ gateway/relay/prompt_resolvers.py:resolve_clarify
 *
 * The adapter (`_send_lifecycle_ack`, `resume_typing_for_chat`,
 * `_canonicalize`), the pending-prompt stores (`tools.approval`,
 * `tools.slash_confirm`, `tools.clarify_gateway`), the authority core
 * (`gateway.approval_actor`, itself bound to those stores) and the i18n lookup
 * of `refusal_text` stay upstream: they are the live transport. Every pure
 * decision is here.
 */
#include "hermes_json.h"
#include "noc_w22_promptreso_7bfabf.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- shared helpers ------------------------------------------------------- */

static char *pr7_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int pr7_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `type(x) is int` for a decoded JSON number. */
static int pr7_is_py_int(const json_t *v) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    if (d < -9.007199254740992e15 || d > 9.007199254740992e15) return 0;
    return 1;
}

/* str(x) for the scalar shapes a prompt state carries. */
static char *pr7_py_str(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return pr7_dup("None");
    switch (v->type) {
    case JSON_BOOL:  return pr7_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return pr7_dup(v->str_val);
    case JSON_NUMBER:
        if (pr7_is_py_int(v)) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else if (floor(v->num_val) == v->num_val) {
            snprintf(buf, sizeof(buf), "%.1f", v->num_val);   /* str(2.0) == "2.0" */
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return pr7_dup(buf);
    default:
        return pr7_dup("None");
    }
}

/* str(value or "") */
static char *pr7_str_or_empty(const json_t *v) {
    if (!pr7_truthy(v)) return pr7_dup("");
    return pr7_py_str(v);
}

/* --- prompt_actor --------------------------------------------------------- */

json_t *w22_promptreso_7bfabf_actor(const json_t *source, const json_t *admin_ids,
                                    int admitted) {
    json_t *out = json_object();
    json_t *uid = (source && source->type == JSON_OBJECT) ? json_obj_get(source, "user_id") : NULL;
    /* getattr(event.source, "user_id", "") -- only a missing attribute is "". */
    json_set(out, "actor_id", uid ? json_copy(uid) : json_string(""));
    json_set(out, "admin_ids", admin_ids ? json_copy(admin_ids) : json_array());
    json_set(out, "admitted", json_bool(admitted ? 1 : 0));
    return out;
}

/* --- prompt_refusal ------------------------------------------------------- */

static json_t *pr7_append_str(json_t *arr, const char *s) {
    json_append(arr, json_string(s ? s : ""));
    return arr;
}

json_t *w22_promptreso_7bfabf_refusal_plan(const json_t *state, const json_t *actor,
                                           const char *option_id, const char *requester) {
    const char *kind = (state && state->type == JSON_OBJECT)
                           ? json_get_str(state, "kind", "") : "";
    json_t *args = json_array();
    json_t *kwargs = json_object();
    json_t *out = json_object();
    char *session_key = pr7_str_or_empty(state && state->type == JSON_OBJECT
                                             ? json_obj_get(state, "session_key") : NULL);
    json_t *actor_id = (actor && actor->type == JSON_OBJECT) ? json_obj_get(actor, "actor_id") : NULL;
    json_t *admin_ids = (actor && actor->type == JSON_OBJECT) ? json_obj_get(actor, "admin_ids") : NULL;
    json_t *admitted_node = (actor && actor->type == JSON_OBJECT) ? json_obj_get(actor, "admitted") : NULL;
    int admitted = admitted_node ? pr7_truthy(admitted_node) : 1;   /* actor.get("admitted", True) */

    json_set(kwargs, "admin_ids", admin_ids ? json_copy(admin_ids) : json_array());
    json_set(kwargs, "admitted", json_bool(admitted));

    if (!strcmp(kind, "exec_approval")) {
        json_t *request_id = json_obj_get(state, "request_id");
        pr7_append_str(args, session_key);
        json_append(args, actor_id ? json_copy(actor_id) : json_string(""));
        json_set(kwargs, "request_id", request_id ? json_copy(request_id) : json_null());
        json_set(kwargs, "choice", option_id ? json_string(option_id) : json_null());
        json_set(out, "fn", json_string("approval_refusal"));
    } else if (!strcmp(kind, "slash_confirm")) {
        char *confirm_id = pr7_str_or_empty(json_obj_get(state, "confirm_id"));
        pr7_append_str(args, session_key);
        pr7_append_str(args, confirm_id);
        json_append(args, actor_id ? json_copy(actor_id) : json_string(""));
        free(confirm_id);
        json_set(out, "fn", json_string("slash_confirm_refusal"));
    } else {
        char *clarify_id = pr7_str_or_empty(json_obj_get(state, "clarify_id"));
        pr7_append_str(args, clarify_id);
        json_append(args, actor_id ? json_copy(actor_id) : json_string(""));
        free(clarify_id);
        json_set(out, "fn", json_string("clarify_refusal"));
    }
    free(session_key);

    json_set(out, "args", args);
    json_set(out, "kwargs", kwargs);
    /* `approval_actor.refusal_text(requester) if requester else None` */
    json_set(out, "refused", json_bool((requester && requester[0]) ? 1 : 0));
    return out;
}

/* --- resolve_exec_approval ------------------------------------------------ */

static const char *pr7_exec_label(const char *choice) {
    if (!strcmp(choice, "once"))    return "\xe2\x9c\x85" " Approved once";
    if (!strcmp(choice, "session")) return "\xe2\x9c\x85" " Approved for session";
    if (!strcmp(choice, "always"))  return "\xe2\x9c\x85" " Approved permanently";
    if (!strcmp(choice, "deny"))    return "\xe2\x9d\x8c" " Denied";
    return NULL;
}

static int pr7_exec_known(const char *choice) {
    return pr7_exec_label(choice) != NULL;
}

json_t *w22_promptreso_7bfabf_exec_ack(const char *option_id, int count,
                                       const json_t *actor_state) {
    const char *choice = (option_id && pr7_exec_known(option_id)) ? option_id : "deny";
    const char *label = count
        ? pr7_exec_label(choice)
        : "\xe2\x8c\x9b" " Approval expired \xe2\x80\x94 no command was waiting.";
    json_t *out = json_object();
    json_t *acks = json_array();

    json_append(acks, json_string(label));
    json_set(out, "choice", json_string(choice));
    json_set(out, "label", json_string(label));
    json_set(out, "acks", acks);
    json_set(out, "resume", json_bool(count ? 1 : 0));
    json_set(out, "kwargs", (actor_state && actor_state->type == JSON_OBJECT)
                                ? json_copy(actor_state) : json_object());
    return out;
}

/* --- resolve_slash_confirm ------------------------------------------------ */

static const char *pr7_slash_label(const char *choice) {
    if (!strcmp(choice, "once"))   return "\xe2\x9c\x85" " Approved once";
    if (!strcmp(choice, "always")) return "\xf0\x9f\x94\x92" " Always approve";
    if (!strcmp(choice, "cancel")) return "\xe2\x9d\x8c" " Cancelled";
    return NULL;
}

json_t *w22_promptreso_7bfabf_slash_ack(const char *option_id, const char *result_text) {
    const char *choice = (option_id && pr7_slash_label(option_id)) ? option_id : "cancel";
    json_t *out = json_object();
    json_t *acks = json_array();

    json_append(acks, json_string(pr7_slash_label(choice)));
    if (result_text && result_text[0]) json_append(acks, json_string(result_text));
    json_set(out, "choice", json_string(choice));
    json_set(out, "acks", acks);
    return out;
}

/* --- resolve_clarify ------------------------------------------------------ */

/* int(text): optional surrounding whitespace, optional sign, digits with
 * single underscores between them. Returns 0 when the text is not an int. */
static int pr7_parse_int(const char *text, long long *out) {
    const char *p = text;
    int negative = 0, digits = 0, prev_digit = 0, overflow = 0;
    unsigned long long acc = 0;

    if (!text) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p == '+' || *p == '-') {
        negative = (*p == '-');
        p++;
    }
    while (*p) {
        char c = *p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') break;
        if (c == '_') {
            if (!prev_digit) return 0;
            prev_digit = 0;
            p++;
            continue;
        }
        if (c < '0' || c > '9') return 0;
        digits++;
        prev_digit = 1;
        if (acc > 922337203685477580ULL / 10) overflow = 1;
        else acc = acc * 10 + (unsigned long long)(c - '0');
        if (acc > 9223372036854775807ULL) overflow = 1;
        p++;
    }
    if (!digits || !prev_digit) return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    if (*p) return 0;
    if (overflow) {                     /* live int() is unbounded: never in range */
        *out = negative ? -9223372036854775807LL - 1 : 9223372036854775807LL;
        return 2;
    }
    *out = negative ? -(long long)acc : (long long)acc;
    return 1;
}

json_t *w22_promptreso_7bfabf_clarify(const char *option_id, const json_t *choices) {
    json_t *out = json_object();
    json_t *choice;
    size_t n, idx;
    long long parsed = -1;
    int parsed_ok;

    if (!choices || choices->type != JSON_ARRAY) {
        /* `state.get("choices") or []` */
        n = 0;
        choice = NULL;
    } else {
        n = choices->c.count;
        choice = NULL;
    }

    if (option_id && !strcmp(option_id, "other")) {
        json_set(out, "action", json_string("await_text"));
        json_set(out, "ack", json_string("\xe2\x9c\x8f\xef\xb8\x8f" " Type your answer:"));
        json_set(out, "value", json_null());
        return out;
    }

    parsed_ok = option_id && option_id[0] == 'c' && pr7_parse_int(option_id + 1, &parsed);
    if (parsed_ok && parsed >= 0 && (unsigned long long)parsed < (unsigned long long)n) {
        idx = (size_t)parsed;
        choice = choices->c.items[idx];
        {
            char *value = pr7_py_str(choice);
            char *ack = (char *)malloc(strlen(value) + 6);
            json_set(out, "action", json_string("resolve"));
            json_set(out, "value", json_string(value));
            if (ack) {
                sprintf(ack, "\xe2\x9c\x85 %s", value);
                json_set(out, "ack", json_string(ack));
                free(ack);
            } else {
                json_set(out, "ack", json_null());
            }
            free(value);
        }
        return out;
    }
    /* Unmappable option: flip to text capture, and never ack. */
    json_set(out, "action", json_string("fallback_text"));
    json_set(out, "ack", json_null());
    json_set(out, "value", json_null());
    return out;
}
