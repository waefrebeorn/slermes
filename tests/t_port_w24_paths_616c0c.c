/* JSON-lines harness for the w24_paths_616c0c gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_paths_616c0c.h"

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
        if (!strcmp(func, "encode")) {
            out = w24_paths_616c0c_encode(json_obj_get(in, "segments"));
        } else if (!strcmp(func, "decode")) {
            out = w24_paths_616c0c_decode(json_obj_get(in, "encoded"));
        } else if (!strcmp(func, "validate")) {
            out = w24_paths_616c0c_validate(in);
        } else if (!strcmp(func, "covers")) {
            out = w24_paths_616c0c_covers(json_obj_get(in, "lock"), json_obj_get(in, "path"));
        } else if (!strcmp(func, "covering_lock")) {
            out = w24_paths_616c0c_covering_lock(json_obj_get(in, "locks"), json_obj_get(in, "path"));
        } else if (!strcmp(func, "from_dotted")) {
            out = w24_paths_616c0c_from_dotted(json_obj_get(in, "dotted"));
        } else if (!strcmp(func, "constants")) {
            out = w24_paths_616c0c_constants();
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
