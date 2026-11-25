/* t_port_w9_compactione_f03b0c.c — fixture-driven differential harness.
 * Reads one JSON case per stdin line, dispatches by "func", emits
 * {"func":..., "out":..., ...all input keys echoed...}. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_compactione_f03b0c.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "closed")) out = w9_compactione_f03b0c_closed(in);
        else if (!strcmp(func, "int")) out = w9_compactione_f03b0c_int(in);
        else if (!strcmp(func, "failure_class")) out = w9_compactione_f03b0c_failure_class(in);
        else if (!strcmp(func, "event_name")) out = w9_compactione_f03b0c_event_name(in);
        else if (!strcmp(func, "attempt_outcome")) out = w9_compactione_f03b0c_attempt_outcome(in);
        else if (!strcmp(func, "attempt_trigger")) out = w9_compactione_f03b0c_attempt_trigger(in);
        else if (!strcmp(func, "payload")) out = w9_compactione_f03b0c_payload(in);
        else if (!strcmp(func, "attempt_payload")) out = w9_compactione_f03b0c_attempt_payload(in);
        else if (!strcmp(func, "micro_payload")) out = w9_compactione_f03b0c_micro_payload(in);
        else if (!strcmp(func, "prune_payload")) out = w9_compactione_f03b0c_prune_payload(in);
        else if (!strcmp(func, "provider_native_payload")) out = w9_compactione_f03b0c_provider_native_payload(in);
        if (!out) out = json_null();
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "out")) continue;
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
