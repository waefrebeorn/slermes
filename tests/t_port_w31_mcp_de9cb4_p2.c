/* JSON-lines harness for the w31_mcp_de9cb4_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_mcp_de9cb4_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "missing_required")) {
            out = w31_mcp_de9cb4_p2_missing_required(in);
        } else if (!strcmp(func, "install_now_message")) {
            out = w31_mcp_de9cb4_p2_install_now_message(in);
        } else if (!strcmp(func, "late_key")) {
            out = w31_mcp_de9cb4_p2_late_key(in);
        } else if (!strcmp(func, "target_snapshot")) {
            out = w31_mcp_de9cb4_p2_target_snapshot(in);
        } else if (!strcmp(func, "no_card_result")) {
            out = w31_mcp_de9cb4_p2_no_card_result(in);
        } else if (!strcmp(func, "answer_plan")) {
            out = w31_mcp_de9cb4_p2_answer_plan(in);
        } else if (!strcmp(func, "installs_with_oauth")) {
            out = w31_mcp_de9cb4_p2_installs_with_oauth(in);
        } else if (!strcmp(func, "actor_for")) {
            out = w31_mcp_de9cb4_p2_actor_for(in);
        } else if (!strcmp(func, "observable")) {
            out = w31_mcp_de9cb4_p2_observable(in);
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
