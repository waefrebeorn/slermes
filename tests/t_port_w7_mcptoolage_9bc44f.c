/* Port gate harness for tools/mcp_tool_agent.py (gate w7_mcptoolage_9bc44f). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_mcptoolage_9bc44f.h"

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
        if (!strcmp(func, "def_name")) {
            out = mcptagent_def_name(json_obj_get(in, "tool_def"));
        } else if (!strcmp(func, "agent_tool_names")) {
            out = mcptagent_agent_tool_names(json_obj_get(in, "tools"));
        } else if (!strcmp(func, "merge")) {
            out = mcptagent_merge_preserving_prefix(json_obj_get(in, "current_defs"),
                                                    json_obj_get(in, "new_defs"),
                                                    json_obj_get(in, "registered_names"),
                                                    json_obj_get(in, "held"));
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
