/* JSON-lines harness for the w27_adapter_1f12c3_p5 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p5.h"

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
        if (!strcmp(func, "sync_policy")) {
            out = w27_adapter_1f12c3_p5_get_discord_command_sync_policy(json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "normalize_permissions")) {
            out = w27_adapter_1f12c3_p5_normalize_permissions(json_obj_get(in, "value"));
        }
        else if (!strcmp(func, "canon_option")) {
            out = w27_adapter_1f12c3_p5_canonicalize_app_command_option(json_obj_get(in, "payload"));
        }
        else if (!strcmp(func, "canon_payload")) {
            out = w27_adapter_1f12c3_p5_canonicalize_app_command_payload(json_obj_get(in, "payload"));
        }
        else if (!strcmp(func, "patchable")) {
            out = w27_adapter_1f12c3_p5_patchable_app_command_payload(json_obj_get(in, "payload"));
        }
        else if (!strcmp(func, "existing_payload")) {
            out = w27_adapter_1f12c3_p5_existing_command_to_payload(json_obj_get(in, "command"));
        }
        else if (!strcmp(func, "reply_rejected")) {
            out = w27_adapter_1f12c3_p5_is_reply_reference_rejected(json_obj_get(in, "text"));
        }
        else if (!strcmp(func, "length_overflow")) {
            out = w27_adapter_1f12c3_p5_is_length_overflow_error(json_obj_get(in, "text"));
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
