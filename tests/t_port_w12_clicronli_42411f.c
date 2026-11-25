#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_clicronli_42411f.h"

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
        if (!strcmp(func, "normalize_skills")) {
            out = w12_clicronli_42411f_normalize_skills(json_obj_get(in, "values"));
        } else if (!strcmp(func, "parse_cron_flags")) {
            out = w12_clicronli_42411f_parse_cron_flags(json_obj_get(in, "tokens"));
        } else if (!strcmp(func, "add_lines")) {
            out = w12_clicronli_42411f_add_lines(json_get_str(in, "subcommand", ""),
                        json_obj_get(in, "opts"), json_obj_get(in, "result"));
        } else if (!strcmp(func, "job_action_lines")) {
            out = w12_clicronli_42411f_job_action_lines(json_get_str(in, "subcommand", ""),
                        json_obj_get(in, "opts"), json_obj_get(in, "result"));
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
