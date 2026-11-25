/* t_port_w13_cliloopsm_ec3523.c — oracle harness for hermes_cli/cli_loops_mixin.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_cliloopsm_ec3523.h"

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
        if (!strcmp(func, "preview")) {
            out = w13_cliloopsm_ec3523_preview(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "parse_queue_index")) {
            out = w13_cliloopsm_ec3523_parse_queue_index(json_obj_get(in, "token"));
        } else if (!strcmp(func, "last_assistant_response_text")) {
            out = w13_cliloopsm_ec3523_last_assistant_response_text(json_obj_get(in, "history"));
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
