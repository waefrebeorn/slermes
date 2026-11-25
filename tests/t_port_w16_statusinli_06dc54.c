/* t_port_w16_statusinli_06dc54.c — harness for src/gateway/status_inline_source.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_statusinli_06dc54.h"

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
        json_t *tokens = json_obj_get(in, "tokens");
        json_t *out = NULL;
        if (!strcmp(func, "inline_source_flag_index"))
            out = w16_statusinli_06dc54_inline_source_flag_index(tokens);
        else if (!strcmp(func, "command_line_runs_inline_source"))
            out = json_bool(w16_statusinli_06dc54_command_line_runs_inline_source(tokens));
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
        fflush(stdout);
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
