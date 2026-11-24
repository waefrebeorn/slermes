/* JSON-lines harness for the w31_mcp_de9cb4_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_mcp_de9cb4_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "validate_names")) {
            out = w31_mcp_de9cb4_p1_validate_names(in);
        } else if (!strcmp(func, "misrouted_error")) {
            out = w31_mcp_de9cb4_p1_misrouted_error(in);
        } else if (!strcmp(func, "bare_error_text")) {
            out = w31_mcp_de9cb4_p1_bare_error_text(in);
        } else if (!strcmp(func, "detail")) {
            out = w31_mcp_de9cb4_p1_detail(in);
        } else if (!strcmp(func, "tool_error_body")) {
            out = w31_mcp_de9cb4_p1_tool_error_body(in);
        } else if (!strcmp(func, "answer_env")) {
            out = w31_mcp_de9cb4_p1_answer_env(in);
        } else if (!strcmp(func, "catalog_instructions")) {
            out = w31_mcp_de9cb4_p1_catalog_instructions(in);
        } else if (!strcmp(func, "cancel_note")) {
            out = w31_mcp_de9cb4_p1_cancel_note(in);
        } else if (!strcmp(func, "retry_error")) {
            out = w31_mcp_de9cb4_p1_retry_error(in);
        } else if (!strcmp(func, "constants")) {
            out = w31_mcp_de9cb4_p1_constants();
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
