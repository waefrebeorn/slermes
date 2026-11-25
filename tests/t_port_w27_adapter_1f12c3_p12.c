/* JSON-lines harness for the w27_adapter_1f12c3_p12 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_1f12c3_p12.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bots_inline_mention")) {
            out = w27_adapter_1f12c3_p12_discord_bots_require_inline_mention(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "channel_keys_from")) {
            out = w27_adapter_1f12c3_p12_discord_channel_keys_from_channel(json_obj_get(in, "channel"), json_obj_get(in, "parent_channel_id"));
        }
        else if (!strcmp(func, "channel_keys")) {
            out = w27_adapter_1f12c3_p12_discord_channel_keys(json_obj_get(in, "channel"), json_obj_get(in, "parent_channel_id"));
        }
        else if (!strcmp(func, "thread_require_mention")) {
            out = w27_adapter_1f12c3_p12_discord_thread_require_mention(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "history_backfill")) {
            out = w27_adapter_1f12c3_p12_discord_history_backfill(json_obj_get(in, "extra"), json_obj_get(in, "env"));
        }
        else if (!strcmp(func, "history_backfill_limit")) {
            out = w27_adapter_1f12c3_p12_discord_history_backfill_limit(json_obj_get(in, "extra"), json_obj_get(in, "env"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
