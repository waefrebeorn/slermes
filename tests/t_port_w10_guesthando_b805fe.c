#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_guesthando_b805fe.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int jint(json_t *v, int dflt){ return (v && v->type==JSON_NUMBER) ? (int)v->num_val : dflt; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "classify")) {
            out = w10_guesthando_b805fe_classify(jint(json_obj_get(in,"status"),0),
                                                 json_obj_get(in,"payload"));
        } else if (!strcmp(func, "result_lines")) {
            out = w10_guesthando_b805fe_result_lines(json_obj_get(in,"status_obj"));
        } else if (!strcmp(func, "status_lines")) {
            out = w10_guesthando_b805fe_status_lines(json_obj_get(in,"status_obj"));
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
