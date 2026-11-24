#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_sessionscm_da8730.h"

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
        char *r;
        if (!strcmp(func, "status_tag")) {
            r = w18_sessionscm_da8730_status_tag(json_get_str(in, "status", ""));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "label")) {
            r = w18_sessionscm_da8730_label(json_obj_get(in, "session"));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "clip")) {
            r = w18_sessionscm_da8730_clip(json_get_str(in, "text", ""),
                                           (int)json_get_num(in, "n", 0));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "msgs_str")) {
            r = w18_sessionscm_da8730_msgs_str(json_obj_get(in, "session"));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "match")) {
            out = json_bool(w18_sessionscm_da8730_match(json_obj_get(in, "session"),
                                                        json_get_str(in, "query", "")));
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
