/* JSON-lines harness for the w20_contract_80eced gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_contract_80eced.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "resolves")) {
            out = json_bool(w20_contract_80eced_resolves(json_get_str(in, "kind", ""),
                                                         json_get_str(in, "state", "")) ? 1 : 0);
        } else if (!strcmp(func, "allowed")) {
            out = w20_contract_80eced_allowed(json_get_str(in, "kind", ""),
                                              json_get_str(in, "current", ""),
                                              json_get_str(in, "to", ""));
        } else if (!strcmp(func, "transitions")) {
            out = w20_contract_80eced_transitions(json_get_str(in, "kind", ""));
        } else if (!strcmp(func, "kinds")) {
            out = w20_contract_80eced_kinds();
        } else if (!strcmp(func, "resolved_states")) {
            out = w20_contract_80eced_resolved_states();
        } else if (!strcmp(func, "enum_values")) {
            out = w20_contract_80eced_enum_values(json_get_str(in, "enum", ""));
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
