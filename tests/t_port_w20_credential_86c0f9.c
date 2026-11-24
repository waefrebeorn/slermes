/* JSON-lines harness for the w20_credential_86c0f9 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_credential_86c0f9.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "pool_failure_reason")) {
            out = w20_credential_86c0f9_pool_failure_reason(
                json_get_str(in, "reason", ""), json_obj_get(in, "error_context"),
                json_get_bool(in, "billing_unverified", 0));
        } else if (!strcmp(func, "model_cooldown_until")) {
            out = w20_credential_86c0f9_model_cooldown_until(
                json_obj_get(in, "cooldowns"), json_obj_get(in, "model"),
                json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "merge_model_cooldowns")) {
            out = w20_credential_86c0f9_merge_model_cooldowns(json_obj_get(in, "maps"));
        } else if (!strcmp(func, "is_model_scoped_failure")) {
            out = json_bool(w20_credential_86c0f9_is_model_scoped_failure(
                json_get_str(in, "provider", ""), json_obj_get(in, "status_code"),
                json_obj_get(in, "model"), json_obj_get(in, "failure_reason")) ? 1 : 0);
        } else if (!strcmp(func, "token_is_blocked")) {
            out = json_bool(w20_credential_86c0f9_token_is_blocked(
                json_obj_get(in, "entries"), json_get_str(in, "token", ""),
                json_obj_get(in, "model"), json_get_num(in, "now", 0.0)) ? 1 : 0);
        } else if (!strcmp(func, "cool_down_model")) {
            out = w20_credential_86c0f9_cool_down_model(
                json_obj_get(in, "entries"), (int)json_get_num(in, "entry_index", 0.0),
                json_get_str(in, "model", ""), json_obj_get(in, "reset_at"),
                json_get_str(in, "failure_reason", NULL), json_get_num(in, "now", 0.0),
                json_get_bool(in, "sole_credential", 0),
                json_get_num(in, "exhausted_ttl", 0.0));
        } else if (!strcmp(func, "limit_state")) {
            out = w20_credential_86c0f9_limit_state(json_obj_get(in, "entries"),
                                                    json_obj_get(in, "models"),
                                                    json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "entitlement_seconds")) {
            out = json_number(w20_credential_86c0f9_model_entitlement_bench_seconds());
        } else if (!strcmp(func, "usage_window")) {
            out = json_string(w20_credential_86c0f9_usage_window_reason());
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
