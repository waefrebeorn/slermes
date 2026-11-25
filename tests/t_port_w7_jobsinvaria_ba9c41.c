/* Port gate harness for cron/jobs_invariants.py (gate w7_jobsinvaria_ba9c41). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_jobsinvaria_ba9c41.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "script_is_command_line")) {
            json_t *sp = json_obj_get(in, "script_path");
            int r = jobsinvariants_script_is_command_line(
                json_get_str(in, "script", ""),
                (sp && sp->type == JSON_STRING) ? sp->str_val : NULL,
                json_get_bool(in, "script_is_file", 0) ? 1 : 0);
            out = json_bool(r ? 1 : 0);
        } else if (!strcmp(func, "mode_error")) {
            out = jobsinvariants_mode_error(in);
        } else if (!strcmp(func, "schedule_error")) {
            out = jobsinvariants_schedule_error(in);
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
