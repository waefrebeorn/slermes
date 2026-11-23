#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_skilllock_842861.h"

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
        if (!strcmp(func, "skill_refusal")) {
            char *s = w6_skilllock_842861_skill_refusal(json_get_str(in, "name", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "unquoted_newlines_to_semicolons")) {
            char *s = w6_skilllock_842861_unquoted_newlines_to_semicolons(json_get_str(in, "command", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "segments")) {
            out = w6_skilllock_842861_segments(json_obj_get(in, "tokens"));
        } else if (!strcmp(func, "operands")) {
            out = w6_skilllock_842861_operands(json_obj_get(in, "args"));
        } else if (!strcmp(func, "target_directory")) {
            out = w6_skilllock_842861_target_directory(json_obj_get(in, "args"));
        } else if (!strcmp(func, "find_exec_commands")) {
            out = w6_skilllock_842861_find_exec_commands(json_obj_get(in, "args"));
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
