#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_mcptoolsch_81805c.h"

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
        if (!strcmp(func, "sanitize")) {
            char *s = w8_mcptoolsch_81805c_sanitize_mcp_name_component(json_get_str(in, "value", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "prefixed")) {
            char *s = w8_mcptoolsch_81805c_mcp_prefixed_tool_name(
                json_get_str(in, "server", ""), json_get_str(in, "tool", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "matches")) {
            out = json_bool(w8_mcptoolsch_81805c_matches_name_filter(
                json_get_str(in, "tool_name", ""), json_obj_get(in, "patterns")) ? true : false);
        } else if (!strcmp(func, "rewrite")) {
            out = w8_mcptoolsch_81805c_rewrite_local_refs(json_obj_get(in, "node"));
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
