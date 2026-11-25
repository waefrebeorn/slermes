/* t_port_w13_cliconfig_2dca4f.c — oracle harness for
 * hermes_cli/cli_config_load.py (gate w13_cliconfig_2dca4f). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_cliconfig_2dca4f.h"

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
        char *s = NULL;

        if (!strcmp(func, "merge_file_config")) {
            out = w13_cliconfig_2dca4f_merge_file_config(
                json_obj_get(in, "defaults"), json_obj_get(in, "file_config"));
        } else if (!strcmp(func, "resolve_prefill_messages_file")) {
            s = w13_cliconfig_2dca4f_resolve_prefill_messages_file(
                json_get_str(in, "env_value", ""), json_obj_get(in, "config"));
            out = json_string(s ? s : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(s);

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
