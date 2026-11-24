/* t_port_w17_onboarding_4983bc.c — harness for src/session_rules/onboarding_flow.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_onboarding_4983bc.h"

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
        if (!strcmp(func, "next_beat"))
            out = w17_onboarding_4983bc_next_beat(json_obj_get(in, "record"));
        else if (!strcmp(func, "is_active"))
            out = w17_onboarding_4983bc_is_active(json_obj_get(in, "record"));
        else if (!strcmp(func, "applies_to"))
            out = w17_onboarding_4983bc_applies_to(json_get_str(in, "tool", ""));
        else if (!strcmp(func, "has_suggestions"))
            out = w17_onboarding_4983bc_has_suggestions(json_obj_get(in, "suggest"));
        else if (!strcmp(func, "is_names"))
            out = w17_onboarding_4983bc_is_names(json_obj_get(in, "value"));
        else if (!strcmp(func, "valid_answer"))
            out = w17_onboarding_4983bc_valid_answer(json_get_str(in, "beat", ""),
                        json_obj_get(in, "answer"), json_obj_get(in, "facts"), json_obj_get(in, "picks"));
        else if (!strcmp(func, "build_values"))
            out = w17_onboarding_4983bc_build_values(json_obj_get(in, "facts"), json_obj_get(in, "picks"));
        else if (!strcmp(func, "constants"))
            out = w17_onboarding_4983bc_constants();
        else
            out = json_string("UNKNOWN_FUNC");
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
