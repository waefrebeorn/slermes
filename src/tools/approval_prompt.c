/*
 * approval_prompt.c — faithful C11 port of upstream Hermes tools/approval_prompt.py.
 * PoP: w8_approvalpro_21df2f_consent @ tools/approval_prompt.py:_consent
 * PoP: w8_approvalpro_21df2f_attempt @ tools/approval_prompt.py:_attempt
 * PoP: w8_approvalpro_21df2f_unanswered @ tools/approval_prompt.py:Unanswered
 * PoP: w8_approvalpro_21df2f_cli_decision @ tools/approval_prompt.py:_ask_human
 * PoP: w8_approvalpro_21df2f_i18n_key @ tools/approval_prompt.py:_CLI_CHOICE_I18N
 */
#include "hermes_json.h"
#include "noc_w8_approvalpro_21df2f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *w8_approvalpro_21df2f_consent(const char *choice, const char *unresolved) {
    if (choice && (!strcmp(choice, "once") || !strcmp(choice, "session") || !strcmp(choice, "always")))
        return "accept";
    if (choice && (!strcmp(choice, "timeout") || !strcmp(choice, "cancelled")))
        return unresolved ? unresolved : "decline";
    return "decline";
}
static struct json_t *dup_or_null(const struct json_t *v) {
    struct json_t *c = json_copy(v);
    return c ? c : json_null();
}
struct json_t *w8_approvalpro_21df2f_attempt(const char *name, const struct json_t *choice,
                              const struct json_t *failure, const struct json_t *fallback) {
    struct json_t *o = json_object();
    json_set(o, "selected", json_bool(true));
    json_set(o, "choice", dup_or_null(choice));
    json_set(o, "failure", dup_or_null(failure));
    json_set(o, "fallback", dup_or_null(fallback));
    json_set(o, "name", json_string(name ? name : ""));
    return o;
}
struct json_t *w8_approvalpro_21df2f_unanswered(const char *cause) {
    struct json_t *o = json_object();
    json_set(o, "choice", json_string("cancelled"));
    json_set(o, "cause", json_string(cause ? cause : ""));
    return o;
}
static int in_array(const struct json_t *a, const char *s) {
    if (!a || a->type != JSON_ARRAY || !s) return 0;
    for (size_t i = 0; i < json_len(a); i++) {
        const json_t *v = json_get(a, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}
const char *w8_approvalpro_21df2f_cli_decision(const char *choice, int once_only, int allow_permanent,
                                const struct json_t *once_inputs, const struct json_t *deny_inputs) {
    if (!choice) choice = "";
    if (once_only) {
        if (in_array(deny_inputs, choice)) return "deny"; /* deny overrides once, like dict union */
        if (in_array(once_inputs, choice)) return "once";
        return "deny";
    }
    const char *d = "deny";
    if (!strcmp(choice, "o") || !strcmp(choice, "once")) d = "once";
    else if (!strcmp(choice, "s") || !strcmp(choice, "session")) d = "session";
    else if (!strcmp(choice, "a") || !strcmp(choice, "always")) d = "always";
    if (!strcmp(d, "always") && !allow_permanent) d = "session";
    return d;
}
const char *w8_approvalpro_21df2f_i18n_key(const char *decision) {
    if (!decision) return NULL;
    if (!strcmp(decision, "once")) return "approval.allowed_once";
    if (!strcmp(decision, "session")) return "approval.allowed_session";
    if (!strcmp(decision, "always")) return "approval.allowed_always";
    if (!strcmp(decision, "deny")) return "approval.denied";
    return NULL;
}