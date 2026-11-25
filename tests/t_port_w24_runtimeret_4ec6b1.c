/* JSON-lines harness for the w24_runtimeret_4ec6b1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_runtimeret_4ec6b1.h"

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
        if (!strcmp(func, "identity_key")) {
            out = w24_runtimeret_4ec6b1_identity_key(json_obj_get(in, "principal_id"),
                                                     json_obj_get(in, "session_id"),
                                                     json_obj_get(in, "request_id"));
        } else if (!strcmp(func, "receipt_key")) {
            out = w24_runtimeret_4ec6b1_receipt_key(json_obj_get(in, "principal_id"),
                                                    json_obj_get(in, "session_id"),
                                                    json_obj_get(in, "request_id"));
        } else if (!strcmp(func, "first_generation")) {
            out = w24_runtimeret_4ec6b1_first_generation(json_obj_get(in, "generations"),
                                                         json_obj_get(in, "retired"),
                                                         json_obj_get(in, "pinned"),
                                                         json_obj_get(in, "identity_keys"),
                                                         json_obj_get(in, "rows"),
                                                         json_obj_get(in, "start"));
        } else if (!strcmp(func, "keep_passed_fences")) {
            out = w24_runtimeret_4ec6b1_keep_passed_fences(json_obj_get(in, "generations"),
                                                           json_obj_get(in, "session_id"),
                                                           json_obj_get(in, "marked"));
        } else if (!strcmp(func, "constants")) {
            out = w24_runtimeret_4ec6b1_constants();
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
