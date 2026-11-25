/* JSON-lines harness for the w21_gatewaylau_1a86bf gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_gatewaylau_1a86bf.h"
#include "hermes_cli/gateway_launchd_runtime.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "budget")) {
            out = w21_gatewaylau_1a86bf_budget(json_obj_get(in, "deadline"),
                                               json_get_num(in, "now", 0.0),
                                               json_get_num(in, "cap", 0.0));
        } else if (!strcmp(func, "poll")) {
            out = w21_gatewaylau_1a86bf_poll(in);
        } else if (!strcmp(func, "finished_jobs")) {
            out = w21_gatewaylau_1a86bf_finished_jobs(in);
        } else if (!strcmp(func, "job")) {
            out = w21_gatewaylau_1a86bf_job(in);
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
