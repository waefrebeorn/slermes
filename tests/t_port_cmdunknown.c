/* t_port_cmdunknown.c — differential harness for gate cmdunknown.
 * func "base": -> out = str.split()[0] or original.
 * func "close": -> out = single get_close_matches winner (or null).
 * func "lines": -> out = [lead, pointer].
 * Gate: run_one_oracle.sh cmdunknown oracle_cmdunknown.py "" \
 *   "src/hermes_cli/cli_unknown_command.o lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_cmdunknown.h"

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
        if (!strcmp(func, "base")) {
            char *b = cmdunknown_command_base(json_get_str(in, "typed", ""));
            out = json_string(b ? b : "");
            free(b);
        } else if (!strcmp(func, "close")) {
            char *b = cmdunknown_command_base(json_get_str(in, "typed", ""));
            char *close = cmdunknown_close_match(b, json_obj_get(in, "known"));
            out = close ? json_string(close) : json_null();
            free(close);
            free(b);
        } else if (!strcmp(func, "lines")) {
            char *lead = NULL, *pointer = NULL;
            cmdunknown_unknown_command_lines(json_get_str(in, "typed", ""),
                                             json_obj_get(in, "known"),
                                             &lead, &pointer);
            out = json_array();
            json_append(out, json_string(lead ? lead : ""));
            json_append(out, json_string(pointer ? pointer : ""));
            free(lead); free(pointer);
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
