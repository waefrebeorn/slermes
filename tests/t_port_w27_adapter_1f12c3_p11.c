/* JSON-lines harness for the w27_adapter_1f12c3_p11 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p11.h"

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
        if (!strcmp(func, "record_debounce")) {
            out = w27_adapter_1f12c3_p11_record_bot_tag_debounce(json_obj_get(in, "delay"), json_obj_get(in, "automated"), json_obj_get(in, "mentioned"), json_obj_get(in, "now"), json_obj_get(in, "split_delay"));
        }
        else if (!strcmp(func, "debounce_continuation")) {
            out = w27_adapter_1f12c3_p11_is_bot_tag_debounce_continuation(json_obj_get(in, "delay"), json_obj_get(in, "automated"), json_obj_get(in, "until"), json_obj_get(in, "now"), json_obj_get(in, "window"));
        }
        else if (!strcmp(func, "free_response_channels")) {
            out = w27_adapter_1f12c3_p11_discord_free_response_channels(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "raw_mentions")) {
            out = w27_adapter_1f12c3_p11_raw_mentioned_user_ids(json_obj_get(in, "content"));
        }
        else if (!strcmp(func, "self_raw_mentioned")) {
            out = w27_adapter_1f12c3_p11_self_is_raw_mentioned(json_obj_get(in, "client_user_id"), json_obj_get(in, "content"));
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
