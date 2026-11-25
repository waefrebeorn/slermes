/* JSON-lines harness for the w24_cli_c07d7e gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_cli_c07d7e.h"

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
        if (!strcmp(func, "dispatch")) {
            out = w24_cli_c07d7e_dispatch(in);
        } else if (!strcmp(func, "format_list")) {
            out = w24_cli_c07d7e_format_list(in);
        } else if (!strcmp(func, "approve_message")) {
            out = w24_cli_c07d7e_approve_message(in);
        } else if (!strcmp(func, "remove_result")) {
            out = w24_cli_c07d7e_remove_result(in);
        } else if (!strcmp(func, "run_banner")) {
            out = w24_cli_c07d7e_run_banner(in);
        } else if (!strcmp(func, "ping_fail")) {
            out = w24_cli_c07d7e_ping_fail(in);
        } else if (!strcmp(func, "ping_missing")) {
            out = w24_cli_c07d7e_ping_missing(in);
        } else if (!strcmp(func, "constants")) {
            out = w24_cli_c07d7e_constants();
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
