/* JSON-lines harness for the w21_agentrunti_df442e gate
 * (agent_runtime_helpers_placeholders).
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_agentrunti_df442e.h"
#include "agent/agent_runtime_helpers_placeholders.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "row")) {
            out = w21_agentrunti_df442e_hidden_interrupt_placeholder_row();
        } else if (!strcmp(func, "placeholder")) {
            out = json_string(w21_agentrunti_df442e_interrupted_placeholder());
        } else if (!strcmp(func, "legacy")) {
            out = json_string(w21_agentrunti_df442e_legacy_placeholder());
        } else if (!strcmp(func, "matches")) {
            json_t *text = json_obj_get(in, "text");
            const char *s = (text && text->type == JSON_STRING) ? text->str_val : NULL;
            out = json_bool(w21_agentrunti_df442e_is_placeholder(s));
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
