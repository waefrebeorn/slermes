/* JSON-lines harness for the w34_runtimeboo_7f06de gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back.  The request's "func" selects which
 * pure decision of gateway/runtime_bootstrap_windows.py is applied. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_runtimeboo_7f06de.h"

static void emit_value(const char *key, json_t *v) {
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
        if (!strcmp(func, "classify_refused_owner")) {
            out = w34_runtimeboo_7f06de_classify_refused_owner(in);
        } else if (!strcmp(func, "refused_owner_remedy")) {
            out = w34_runtimeboo_7f06de_refused_owner_remedy(in);
        } else if (!strcmp(func, "judge_pipe_server")) {
            out = w34_runtimeboo_7f06de_judge_pipe_server(in);
        } else if (!strcmp(func, "peer_subject")) {
            out = w34_runtimeboo_7f06de_peer_subject(in);
        } else if (!strcmp(func, "remaining_ms")) {
            out = w34_runtimeboo_7f06de_remaining_ms(in);
        } else if (!strcmp(func, "accept_outcome")) {
            out = w34_runtimeboo_7f06de_accept_outcome(in);
        } else if (!strcmp(func, "pipe_open_action")) {
            out = w34_runtimeboo_7f06de_pipe_open_action(in);
        } else if (!strcmp(func, "read_step")) {
            out = w34_runtimeboo_7f06de_read_step(in);
        } else if (!strcmp(func, "pipe_dacl")) {
            out = w34_runtimeboo_7f06de_pipe_dacl(in);
        } else if (!strcmp(func, "pipe_create_mode")) {
            out = w34_runtimeboo_7f06de_pipe_create_mode(in);
        } else if (!strcmp(func, "constants")) {
            out = w34_runtimeboo_7f06de_constants();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_value("func", json_string(func));
        fputs(",", stdout);
        emit_value("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_value(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
