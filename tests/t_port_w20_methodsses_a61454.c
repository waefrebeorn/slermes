/* JSON-lines harness for the w20_methodsses_a61454 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * every seam the live body reads is a fixture field, and absent ones fall back
 * to the same defaults the Python call sites use. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_methodsses_a61454.h"

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
        if (!strcmp(func, "branch_rows")) {
            out = w20_methodsses_a61454_branch_rows(json_obj_get(in, "history"),
                                                    json_obj_get(in, "copy_fields"),
                                                    json_obj_get(in, "identities"));
        } else if (!strcmp(func, "branch_persist")) {
            out = w20_methodsses_a61454_branch_persist(json_obj_get(in, "spec"));
        } else if (!strcmp(func, "title_is_user")) {
            out = json_bool(w20_methodsses_a61454_seed_title_source(
                                json_get_str(in, "title_source", "")) ? 1 : 0);
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
