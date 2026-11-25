/* JSON-lines harness for the w34_webhostter_a3f534_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_webhostter_a3f534_p1.h"

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
        if (!strcmp(func, "constants")) {
            out = w34_webhostter_a3f534_p1_constants();
        } else if (!strcmp(func, "request_identity")) {
            out = w34_webhostter_a3f534_p1_request_identity(in);
        } else if (!strcmp(func, "home_live")) {
            out = w34_webhostter_a3f534_p1_home_live(in);
        } else if (!strcmp(func, "request_home")) {
            out = w34_webhostter_a3f534_p1_request_home(in);
        } else if (!strcmp(func, "resolve_verdict")) {
            out = w34_webhostter_a3f534_p1_resolve_verdict(in);
        } else if (!strcmp(func, "get_registry_state")) {
            out = w34_webhostter_a3f534_p1_get_registry_state(in);
        } else if (!strcmp(func, "lifespan_exit")) {
            out = w34_webhostter_a3f534_p1_lifespan_exit(in);
        } else if (!strcmp(func, "reap_plan")) {
            out = w34_webhostter_a3f534_p1_reap_plan(in);
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
