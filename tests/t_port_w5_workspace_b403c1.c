#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_workspace_b403c1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "member_ignored")) {
            out = w5_workspace_b403c1_member_ignored(json_obj_get(in, "names"));
        } else if (!strcmp(func, "without_release_age")) {
            out = w5_workspace_b403c1_without_release_age(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "conjuncts")) {
            out = w5_workspace_b403c1_conjuncts(json_get_str(in, "marker", ""));
        } else if (!strcmp(func, "member_key")) {
            out = w5_workspace_b403c1_member_key(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "solve_only_for")) {
            out = w5_workspace_b403c1_solve_only_for(json_obj_get(in, "environments"),
                                                     json_obj_get(in, "target"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
