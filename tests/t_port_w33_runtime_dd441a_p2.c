/* JSON-lines harness for the w33_runtime_dd441a_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_runtime_dd441a_p2.h"

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
        if (!strcmp(func, "parse_mutation")) {
            out = w33_runtime_dd441a_p2_parse_mutation(json_obj_get(in, "operation"),
                                                       json_obj_get(in, "payload"));
        } else if (!strcmp(func, "reasoning_level")) {
            out = w33_runtime_dd441a_p2_reasoning_level(json_obj_get(in, "value"));
        } else if (!strcmp(func, "editor_fingerprint")) {
            out = w33_runtime_dd441a_p2_editor_fingerprint(json_obj_get(in, "value"));
        } else if (!strcmp(func, "generation_required")) {
            out = w33_runtime_dd441a_p2_generation_required(json_obj_get(in, "operation"));
        } else if (!strcmp(func, "metadata_fields")) {
            out = w33_runtime_dd441a_p2_metadata_fields();
        } else if (!strcmp(func, "constants")) {
            out = w33_runtime_dd441a_p2_constants();
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
