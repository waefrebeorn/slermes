/* JSON-lines harness for the w20_hostedroom_dfc5ca gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_hostedroom_dfc5ca.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "budget_remaining")) {
            out = w20_hostedroom_dfc5ca_budget_remaining(
                json_get_num(in, "timeout", 0.0), json_get_num(in, "elapsed", 0.0),
                json_get_bool(in, "closed", 0));
        } else if (!strcmp(func, "budget_track")) {
            out = w20_hostedroom_dfc5ca_budget_track(json_get_bool(in, "closed", 0));
        } else if (!strcmp(func, "budget_close")) {
            out = w20_hostedroom_dfc5ca_budget_close(
                json_get_num(in, "timeout", 0.0), (int)json_get_num(in, "sockets", 0.0),
                json_get_bool(in, "shutdown_fails", 0));
        } else if (!strcmp(func, "open_plan")) {
            out = w20_hostedroom_dfc5ca_open_plan(
                json_obj_get(in, "targets"), json_get_str(in, "host", ""),
                json_obj_get(in, "port"), json_get_str(in, "scheme", "https"),
                json_get_num(in, "timeout", 0.0), json_get_num(in, "elapsed", 0.0));
        } else if (!strcmp(func, "max_attempts")) {
            out = json_number(w20_hostedroom_dfc5ca_max_outstanding_attempts());
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
