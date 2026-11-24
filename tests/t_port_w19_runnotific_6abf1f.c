#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_runnotific_6abf1f.h"

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
        if (!strcmp(func, "output_tail")) {
            out = w19_runnotific_6abf1f_output_tail(
                json_get_str(in, "output", ""), (long long)json_get_num(in, "limit", 0));
        } else if (!strcmp(func, "marker_profile")) {
            out = w19_runnotific_6abf1f_marker_profile(json_obj_get(in, "data"));
        } else if (!strcmp(func, "marker_age")) {
            out = w19_runnotific_6abf1f_marker_age(
                json_obj_get(in, "data"), json_get_str(in, "now_iso", ""));
        } else if (!strcmp(func, "read_since")) {
            if (json_get_bool(in, "io_error", 0)) {
                out = json_object();
                json_set(out, "text", json_string(""));
                json_set(out, "offset", json_number(json_get_num(in, "offset", 0)));
            } else {
                out = w19_runnotific_6abf1f_read_since(
                    json_get_str(in, "data", ""), (long long)json_get_num(in, "offset", 0));
            }
        } else if (!strcmp(func, "update_paths")) {
            out = w19_runnotific_6abf1f_update_paths(json_get_str(in, "hermes_home", ""));
        } else if (!strcmp(func, "update_exit_code")) {
            out = w19_runnotific_6abf1f_update_exit_code(json_get_str(in, "text", ""));
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
