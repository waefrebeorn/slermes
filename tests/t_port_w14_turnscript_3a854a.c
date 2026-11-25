#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_turnscript_3a854a.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "prelude_call_id")) {
            char *s = w14_turnscript_3a854a_prelude_call_id(
                json_get_str(in, "session_id", ""),
                json_get_str(in, "name", ""),
                json_obj_get(in, "args"),
                (long long)json_get_num(in, "index", 0));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "play_prelude")) {
            out = w14_turnscript_3a854a_play_prelude(
                json_get_bool(in, "prelude_present", false) ? 1 : 0,
                json_obj_get(in, "verdict"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
