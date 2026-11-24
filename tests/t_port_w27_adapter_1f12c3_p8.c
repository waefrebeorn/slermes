/* JSON-lines harness for the w27_adapter_1f12c3_p8 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p8.h"

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
        if (!strcmp(func, "backfill_channels")) {
            out = w27_adapter_1f12c3_p8_missed_message_backfill_channels(json_obj_get(in, "extra"), json_obj_get(in, "env"), json_obj_get(in, "allowed_channels"), json_obj_get(in, "free_response"));
        }
        else if (!strcmp(func, "extra_flag")) {
            out = w27_adapter_1f12c3_p8_extra_or_env_flag(json_obj_get(in, "extra"), json_obj_get(in, "key"), json_obj_get(in, "env_default"), json_obj_get(in, "truthy"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "require_mention")) {
            out = w27_adapter_1f12c3_p8_discord_require_mention(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "free_response_auto_thread")) {
            out = w27_adapter_1f12c3_p8_discord_free_response_auto_thread(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "max_attachment_bytes")) {
            out = w27_adapter_1f12c3_p8_discord_max_attachment_bytes(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "voice_attachment")) {
            out = w27_adapter_1f12c3_p8_is_discord_voice_message_attachment(json_obj_get(in, "marker"), json_obj_get(in, "duration"), json_obj_get(in, "waveform"));
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
