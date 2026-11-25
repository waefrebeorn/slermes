#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_delegateto_7a3a62.h"

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
        json_t *args = json_obj_get(in, "in");
        json_t *catalog = json_obj_get(args, "catalog");
        json_t *out = NULL;
        if (!strcmp(func, "is_mcp")) {
            out = w16_delegateto_7a3a62_is_mcp_toolset_name(
                json_get_str(args, "name", ""), json_get_str(args, "alias_target", ""));
        } else if (!strcmp(func, "blocked_for_role")) {
            out = w16_delegateto_7a3a62_blocked_toolsets_for_role(
                json_get_str(args, "role", ""), catalog);
        } else if (!strcmp(func, "strip_blocked")) {
            out = w16_delegateto_7a3a62_strip_blocked_tools(
                json_obj_get(args, "toolsets"), catalog);
        } else if (!strcmp(func, "expand_parent")) {
            out = w16_delegateto_7a3a62_expand_parent_toolsets(
                json_obj_get(args, "parent"), catalog);
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
