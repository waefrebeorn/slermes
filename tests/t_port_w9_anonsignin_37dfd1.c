#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_anonsignin_37dfd1.h"

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
        if (!strcmp(func, "format_wait_line"))
            out = w9_anonsignin_37dfd1_format_wait_line(json_obj_get(in, "expires_in"));
        else if (!strcmp(func, "completed_copy")) {
            json_t *changed = json_obj_get(in, "model_changed");
            json_t *term = json_obj_get(in, "terminal");
            out = w9_anonsignin_37dfd1_completed_copy(
                json_obj_get(in, "email"), json_obj_get(in, "model"),
                (changed && changed->type == JSON_BOOL && changed->bool_val) ? 1 : 0,
                json_obj_get(in, "handoff"),
                (term && term->type == JSON_BOOL && term->bool_val) ? 1 : 0);
        } else if (!strcmp(func, "code_copy_with_wait"))
            out = w9_anonsignin_37dfd1_code_copy_with_wait(json_obj_get(in, "expires_in"));
        else if (!strcmp(func, "state_copy"))
            out = w9_anonsignin_37dfd1_state_copy(json_get_str(in, "kind", ""), json_obj_get(in, "detail"));
        else
            out = json_string("UNKNOWN_FUNC");
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
