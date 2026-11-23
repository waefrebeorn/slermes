/* t_port_w9_runagentca_634fc9.c — harness for src/gateway/run_agent_cache.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_runagentca_634fc9.h"

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
        if (!strcmp(func, "first")) {
            out = w9_runagentca_634fc9_first(in);
        } else if (!strcmp(func, "tuple")) {
            out = w9_runagentca_634fc9_tuple(in);
        } else if (!strcmp(func, "config_sig")) {
            out = w9_runagentca_634fc9_config_sig(in);
        }
        printf("{");
        json_t *fv = json_string(func);
        emit_key("func", fv);
        json_free(fv);
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
