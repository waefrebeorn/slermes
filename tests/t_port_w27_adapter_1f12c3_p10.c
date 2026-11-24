/* JSON-lines harness for the w27_adapter_1f12c3_p10 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p10.h"

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
        if (!strcmp(func, "resolved_ids")) {
            out = w27_adapter_1f12c3_p10_resolved_allowlist_user_ids(json_obj_get(in, "ids"));
        }
        else if (!strcmp(func, "allow_all_users")) {
            out = w27_adapter_1f12c3_p10_discord_allow_all_users(json_obj_get(in, "env_val"), json_obj_get(in, "extra"));
        }
        else if (!strcmp(func, "gateway_allow_all")) {
            out = w27_adapter_1f12c3_p10_gateway_allow_all_users(json_obj_get(in, "env_val"));
        }
        else if (!strcmp(func, "allow_bots")) {
            out = w27_adapter_1f12c3_p10_get_allow_bots(json_obj_get(in, "env_val"), json_obj_get(in, "extra"));
        }
        else if (!strcmp(func, "debounce_key")) {
            out = w27_adapter_1f12c3_p10_bot_tag_debounce_key(json_obj_get(in, "channel_id"), json_obj_get(in, "author_id"));
        }
        else if (!strcmp(func, "window_seconds")) {
            out = w27_adapter_1f12c3_p10_bot_tag_window_seconds(json_obj_get(in, "delay"), json_obj_get(in, "split_delay"));
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
