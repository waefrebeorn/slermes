/* Port gate harness for gateway/session_config.py (gate w8_sessionconf_6d4604). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessionconf_6d4604.h"

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
        if (!strcmp(func, "mcp_revision")) {
            out = sessionconf_mcp_revision(json_obj_get(in, "source"));
        } else if (!strcmp(func, "config_get_problem")) {
            out = sessionconf_config_get_problem(json_obj_get(in, "params"));
        } else if (!strcmp(func, "model_options_problem")) {
            out = sessionconf_model_options_problem(json_obj_get(in, "params"));
        } else if (!strcmp(func, "fast_value")) {
            out = sessionconf_fast_value(json_get_str(in, "tier", NULL));
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
