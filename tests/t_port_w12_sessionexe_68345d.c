#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_sessionexe_68345d.h"
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
        if (!strcmp(func, "skip_context_files")) {
            out = w12_sessionexe_68345d_skip_context_files(json_obj_get(in, "policy"),
                json_obj_get(in, "platforms_cfg"), json_obj_get(in, "platform_key"));
        } else if (!strcmp(func, "current_message_count")) {
            out = w12_sessionexe_68345d_current_message_count(json_obj_get(in, "has_store"),
                json_obj_get(in, "session_id"), json_obj_get(in, "row"));
        } else if (!strcmp(func, "cached_sid_is_dead")) {
            out = w12_sessionexe_68345d_cached_sid_is_dead(json_obj_get(in, "has_lock"),
                json_obj_get(in, "cache_entry"), json_obj_get(in, "session_key"),
                json_obj_get(in, "session_id"), json_obj_get(in, "dead_check"));
        } else {
            out = json_string("UNKNOWN_FUNC");
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
