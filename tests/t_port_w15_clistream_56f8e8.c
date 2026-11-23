#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_clistream_56f8e8.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "slow_command_status")) {
            out = w15_clistream_56f8e8_slow_command_status(json_get_str(in, "command", ""));
        } else if (!strcmp(func, "current_reasoning_callback")) {
            out = w15_clistream_56f8e8_current_reasoning_callback(
                      json_obj_get(in, "show_reasoning"),
                      json_obj_get(in, "streaming_enabled"),
                      json_obj_get(in, "verbose"));
        } else if (!strcmp(func, "reset_stream_state")) {
            out = w15_clistream_56f8e8_reset_stream_state();
        } else if (!strcmp(func, "flush_reasoning_preview")) {
            out = w15_clistream_56f8e8_flush_reasoning_preview(
                      json_get_str(in, "buf", ""),
                      json_get_bool(in, "force", 0) ? 1 : 0,
                      (int)json_get_num(in, "columns", 80));
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
