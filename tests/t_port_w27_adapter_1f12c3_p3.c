/* JSON-lines harness for the w27_adapter_1f12c3_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "classify_connect")) {
            out = w27_adapter_1f12c3_p3_classify_connect_exception(json_obj_get(in, "type_name"), json_obj_get(in, "msg"), json_obj_get(in, "shown"), json_obj_get(in, "allowed_user_ids"), json_obj_get(in, "allowed_role_ids"));
        }
        else if (!strcmp(func, "flush_deadline")) {
            out = w27_adapter_1f12c3_p3_text_batch_flush_deadline_seconds(json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "state_key")) {
            out = w27_adapter_1f12c3_p3_command_sync_state_key(json_obj_get(in, "app_id"));
        }
        else if (!strcmp(func, "skip_reason")) {
            out = w27_adapter_1f12c3_p3_command_sync_skip_reason(json_obj_get(in, "entry"), json_obj_get(in, "fingerprint"), json_obj_get(in, "now"));
        }
        else if (!strcmp(func, "mutation_interval")) {
            out = w27_adapter_1f12c3_p3_command_sync_mutation_interval_seconds();
        }
        else if (!strcmp(func, "extract_retry_after")) {
            out = w27_adapter_1f12c3_p3_extract_discord_retry_after(json_obj_get(in, "retry_after"), json_obj_get(in, "headers"));
        }
        else if (!strcmp(func, "is_rate_limit")) {
            out = w27_adapter_1f12c3_p3_is_discord_rate_limit(json_obj_get(in, "type_name"), json_obj_get(in, "retry_after"));
        }
        else if (!strcmp(func, "unknown_interaction")) {
            out = w27_adapter_1f12c3_p3_is_discord_unknown_interaction(json_obj_get(in, "code"), json_obj_get(in, "data_code"), json_obj_get(in, "status"), json_obj_get(in, "msg"));
        }
        else {
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
