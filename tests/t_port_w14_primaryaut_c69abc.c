#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_primaryaut_c69abc.h"

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
        if (!strcmp(func, "route_snapshot")) {
            out = w14_primaryaut_c69abc_route_snapshot(json_obj_get(in, "runtime_kwargs"));
        } else if (!strcmp(func, "failure_for_route")) {
            out = w14_primaryaut_c69abc_failure_for_route(json_obj_get(in, "failure"),
                json_obj_get(in, "resolved"), json_obj_get(in, "runtime_kwargs"));
        } else if (!strcmp(func, "primary_auth_lead")) {
            const char *r = w14_primaryaut_c69abc_primary_auth_lead(
                json_obj_get(in, "failure"), json_get_str(in, "backup", ""),
                json_obj_get(in, "failure_reason"), json_obj_get(in, "error"));
            out = json_string(r ? r : "");
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
