/* t_port_w5_buildenv_1eb2c8.c — harness for pm/build_env.py port.
 * Reads {"func":<name>, ...args...}, emits
 * {"func":<name>,"out":<value>, ...echoed input keys...}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_buildenv_1eb2c8.h"

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
        if (!strcmp(func, "w5_buildenv_1eb2c8_parse_requirements")) {
            out = w5_buildenv_1eb2c8_parse_requirements(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "w5_buildenv_1eb2c8_record_selection_invalid")) {
            int r = w5_buildenv_1eb2c8_record_selection_invalid(
                (int)json_get_bool(in, "record_selection", 0),
                (int)json_get_bool(in, "build", 0),
                (int)json_get_num(in, "requirements_count", 0),
                (int)json_get_bool(in, "manager_runtime", 0),
                (int)json_get_bool(in, "all_extras", 0),
                (int)json_get_num(in, "groups_count", 0),
                (int)json_get_num(in, "extras_count", 0));
            out = json_bool(r != 0);
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
