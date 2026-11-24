/* JSON-lines harness for the w23_mutation_6cb83f gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_mutation_6cb83f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        const json_t *req;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        req = json_obj_get(in, "req");
        if (!strcmp(func, "mutate")) {
            out = w23_mutation_6cb83f_mutate(req);
        } else if (!strcmp(func, "receipt_key")) {
            out = w23_mutation_6cb83f_receipt_key(req);
        } else if (!strcmp(func, "parse")) {
            out = json_bool(w23_mutation_6cb83f_parse(req) != 0);
        } else if (!strcmp(func, "require_generation")) {
            out = json_bool(w23_mutation_6cb83f_require_generation(
                                req && req->type == JSON_OBJECT ? json_obj_get(req, "operation") : NULL) != 0);
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
