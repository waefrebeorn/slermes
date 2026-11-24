/* t_port_w18_updatepaus_c43de1.c — harness for src/hermes_cli/update_pause_record.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_updatepaus_c43de1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "without")) {
            out = w18_updatepaus_c43de1_without(json_obj_get(in, "token"),
                                                json_obj_get(in, "pids"));
        } else if (!strcmp(func, "has_work")) {
            out = json_bool(w18_updatepaus_c43de1_has_work(
                        json_obj_get(in, "token")) ? true : false);
        } else if (!strcmp(func, "relaunch_keys")) {
            out = w18_updatepaus_c43de1_relaunch_keys(json_obj_get(in, "token"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
