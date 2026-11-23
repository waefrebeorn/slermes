/* t_port_w10_wakeworde_ebd387.c -- JSON-lines harness for src/tools/wake_word_engines.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_wakeworde_ebd387.h"

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
        if (!strcmp(func, "sub")) {
            out = w10_wakeworde_ebd387_sub(json_obj_get(in, "cfg"), json_get_str(in, "key", ""));
        } else if (!strcmp(func, "path")) {
            out = w10_wakeworde_ebd387_looks_like_path(json_get_str(in, "value", ""),
                    json_get_bool(in, "exists", false) ? 1 : 0, json_get_str(in, "sep", "\\"));
        } else if (!strcmp(func, "root")) {
            out = w10_wakeworde_ebd387_sherpa_model_root(json_get_str(in, "home", ""));
        } else if (!strcmp(func, "openwake")) {
            out = w10_wakeworde_ebd387_openwake_process(json_obj_get(in, "scores"),
                    json_get_num(in, "threshold", 0.6), (int)json_get_num(in, "needed", 1.0),
                    (int)json_get_num(in, "streak", 0.0));
        } else if (!strcmp(func, "sherpa")) {
            out = w10_wakeworde_ebd387_sherpa_process(json_obj_get(in, "results"),
                    json_obj_get(in, "map"));
        } else if (!strcmp(func, "porcupine")) {
            out = w10_wakeworde_ebd387_porcupine_process(json_obj_get(in, "values"));
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
