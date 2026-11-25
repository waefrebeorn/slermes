/* JSON-lines harness for the w20_sessionvau_bfb027 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_sessionvau_bfb027.h"

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
        if (!strcmp(func, "code_request")) {
            out = w20_sessionvau_bfb027_code_request(json_obj_get(in, "ops"));
        } else if (!strcmp(func, "user_id")) {
            char *id = w20_sessionvau_bfb027_user_id(json_obj_get(in, "source"));
            out = json_string(id ? id : "");
            free(id);
        } else if (!strcmp(func, "scope_gate")) {
            out = w20_sessionvau_bfb027_scope_gate(json_obj_get(in, "turn"));
        } else if (!strcmp(func, "wait_slice")) {
            out = json_number(w20_sessionvau_bfb027_wait_slice(json_get_num(in, "remaining", 0.0)));
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
