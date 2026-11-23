/* Port gate harness for hermes_cli/plugins_state.py (gate w10_pluginssta_a8131d). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_pluginssta_a8131d.h"

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
        if (!strcmp(func, "segments")) {
            out = w10_pluginssta_a8131d_relative_segments(json_get_str(in, "key", ""));
        } else if (!strcmp(func, "nested_value")) {
            out = w10_pluginssta_a8131d_nested_value(json_obj_get(in, "root"),
                     json_obj_get(in, "segments"), json_obj_get(in, "default"));
        } else if (!strcmp(func, "nested_mapping")) {
            out = w10_pluginssta_a8131d_nested_mapping(json_obj_get(in, "segments"),
                     json_obj_get(in, "value"));
        } else if (!strcmp(func, "state_key_valid")) {
            out = w10_pluginssta_a8131d_state_key_valid(json_get_str(in, "key", ""));
        } else if (!strcmp(func, "data_ns")) {
            out = w10_pluginssta_a8131d_data_namespace(
                     json_get_str(in, "plugin_id", ""),
                     json_get_str(in, "data_namespace", ""),
                     json_get_str(in, "portable_ns", ""));
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
