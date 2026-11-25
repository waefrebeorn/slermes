#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_onboarding_ec6652.h"

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
        if (!strcmp(func, "builds_onboarding")) {
            out = json_bool(w10_onboarding_ec6652_builds_onboarding(json_obj_get(in, "valid")) ? true : false);
        } else if (!strcmp(func, "keeps_toolsets")) {
            out = json_bool(w10_onboarding_ec6652_keeps_toolsets(json_obj_get(in, "enabled")) ? true : false);
        } else if (!strcmp(func, "with_onboarding")) {
            out = w10_onboarding_ec6652_with_onboarding(
                json_obj_get(in, "enabled"), json_obj_get(in, "all_toolsets"),
                json_obj_get(in, "session_platforms"));
        } else if (!strcmp(func, "carry_onboarding_toolset")) {
            out = w10_onboarding_ec6652_carry_onboarding_toolset(
                json_obj_get(in, "enabled"), json_obj_get(in, "agent_toolsets"));
        } else if (!strcmp(func, "append_turn_note")) {
            char *s = w10_onboarding_ec6652_append_turn_note(
                json_get_str(in, "prior", ""), json_get_str(in, "note", ""));
            out = json_string(s ? s : "");
            free(s);
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
