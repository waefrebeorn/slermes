#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_turnmarker_94f189.h"
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
        if (!strcmp(func, "started_at")) {
            out = json_number(w10_turnmarker_94f189_started_at(json_obj_get(in, "entry")));
        } else if (!strcmp(func, "marker_writer_state")) {
            out = json_string(w10_turnmarker_94f189_marker_writer_state(
                json_obj_get(in, "entry"), (int)json_get_num(in, "liveness", 0)));
        } else if (!strcmp(func, "prune")) {
            out = w10_turnmarker_94f189_prune(json_obj_get(in, "entries"), json_get_num(in, "now", 0));
        } else if (!strcmp(func, "read_turn_marker")) {
            out = w10_turnmarker_94f189_read_turn_marker(json_get_str(in, "session_key", ""),
                                                          json_obj_get(in, "stored"));
            if (!out) out = json_null();
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
