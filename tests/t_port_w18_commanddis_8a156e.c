#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_commanddis_8a156e.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "item")) {
            json_t *dv = a ? json_obj_get(a, "display") : NULL;
            out = w18_commanddis_8a156e_item(json_get_str(a, "text", ""), json_get_str(a, "meta", ""),
                                             (dv && dv->type == JSON_STRING) ? dv->str_val : NULL);
        } else if (!strcmp(func, "details_root_meta")) {
            out = w18_commanddis_8a156e_details_root_meta(json_get_str(a, "candidate", ""));
        } else if (!strcmp(func, "details_completions")) {
            out = w18_commanddis_8a156e_details_completions(json_get_str(a, "text", ""));
        } else out = json_string("UNKNOWN_FUNC");
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
