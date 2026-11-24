/* JSON-lines harness for the w30_updatecmdp_fa52e9_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_updatecmdp_fa52e9_p2.h"

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
        if (!strcmp(func, "names")) {
            out = w30_updatecmdp_fa52e9_p2_names(in);
        } else if (!strcmp(func, "describe")) {
            out = w30_updatecmdp_fa52e9_p2_describe(in);
        } else if (!strcmp(func, "drain_ack")) {
            out = w30_updatecmdp_fa52e9_p2_drain_ack(in);
        } else if (!strcmp(func, "supervisor_marker")) {
            out = w30_updatecmdp_fa52e9_p2_supervisor_marker(in);
        } else if (!strcmp(func, "left_running")) {
            out = w30_updatecmdp_fa52e9_p2_left_running(in);
        } else if (!strcmp(func, "abort_unsettled")) {
            out = w30_updatecmdp_fa52e9_p2_abort_unsettled(in);
        } else if (!strcmp(func, "bare_replay_env")) {
            out = w30_updatecmdp_fa52e9_p2_bare_replay_env(in);
        } else if (!strcmp(func, "gateway_on_home")) {
            out = w30_updatecmdp_fa52e9_p2_gateway_on_home(in);
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
