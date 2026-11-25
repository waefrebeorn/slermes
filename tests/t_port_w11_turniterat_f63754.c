#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_turniterat_f63754.h"
static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "anchors")) {
            int r = w11_turniterat_f63754_anchors_current_turn(
                json_obj_get(in, "messages"), json_get_str(in, "idx", ""),
                json_get_str(in, "user_message", ""), json_obj_get(in, "view"));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "previous_tool_round")) {
            out = w11_turniterat_f63754_previous_tool_round(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "neutralise")) {
            out = w11_turniterat_f63754_neutralise_replay_echo_ghosts(
                json_obj_get(in, "seq"),
                json_get_str(in, "scaffold_marker", ""),
                json_get_str(in, "legacy_placeholder", ""),
                json_get_str(in, "current_placeholder", ""));
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
