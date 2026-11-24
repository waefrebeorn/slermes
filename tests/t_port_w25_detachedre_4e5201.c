/* JSON-lines harness for the w25_detachedre_4e5201 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_detachedre_4e5201.h"

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
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "delegation_delivery")) {
            out = w25_detachedre_4e5201_delegation_delivery(in);
        } else if (!strcmp(func, "completion_note")) {
            out = w25_detachedre_4e5201_completion_note(in);
        } else if (!strcmp(func, "constants")) {
            out = w25_detachedre_4e5201_constants();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        { json_t *f = json_string(func); emit_key("func", f); json_free(f); }
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
