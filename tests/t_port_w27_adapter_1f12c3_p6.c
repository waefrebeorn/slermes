/* JSON-lines harness for the w27_adapter_1f12c3_p6 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p6.h"

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
        if (!strcmp(func, "voice_fx")) {
            out = w27_adapter_1f12c3_p6_load_voice_fx_config(json_obj_get(in, "fx"));
        }
        else if (!strcmp(func, "int_config")) {
            out = w27_adapter_1f12c3_p6_load_discord_int_config(json_obj_get(in, "cfg"), json_obj_get(in, "key"), json_obj_get(in, "default"), json_obj_get(in, "minimum"));
        }
        else if (!strcmp(func, "voice_timeout")) {
            out = w27_adapter_1f12c3_p6_load_voice_timeout(json_obj_get(in, "cfg"));
        }
        else if (!strcmp(func, "playback_timeout")) {
            out = w27_adapter_1f12c3_p6_load_playback_timeout(json_obj_get(in, "cfg"));
        }
        else if (!strcmp(func, "voice_timeout_limit")) {
            out = w27_adapter_1f12c3_p6_voice_timeout_limit(json_obj_get(in, "stored"));
        }
        else if (!strcmp(func, "playback_timeout_limit")) {
            out = w27_adapter_1f12c3_p6_playback_timeout_limit(json_obj_get(in, "stored"));
        }
        else if (!strcmp(func, "mixer_active")) {
            out = w27_adapter_1f12c3_p6_voice_mixer_active(json_obj_get(in, "mixers"), json_obj_get(in, "guild_id"));
        }
        else if (!strcmp(func, "channel_ids_allowed")) {
            out = w27_adapter_1f12c3_p6_discord_channel_ids_allowed(json_obj_get(in, "channel_ids"), json_obj_get(in, "allowed"));
        }
        else if (!strcmp(func, "warn_fail_closed")) {
            out = w27_adapter_1f12c3_p6_warn_if_fail_closed_default(json_obj_get(in, "state"));
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
