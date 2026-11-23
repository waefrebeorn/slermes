#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_updatecmd_d274f9.h"

static const char *str_or_null(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
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
        if (!strcmp(func, "sqlite_partial_completion_lines")) {
            out = w15_updatecmd_d274f9_sqlite_partial_completion_lines(
                json_get_str(in, "sqlite_version", ""),
                json_get_bool(in, "is_windows", false) ? 1 : 0);
        } else if (!strcmp(func, "update_complete_message")) {
            out = w15_updatecmd_d274f9_update_complete_message_ex(
                str_or_null(in, "pre_version"), str_or_null(in, "post_version"));
        } else if (!strcmp(func, "profile_skill_sync_status")) {
            out = w15_updatecmd_d274f9_profile_skill_sync_status(json_obj_get(in, "r"));
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
