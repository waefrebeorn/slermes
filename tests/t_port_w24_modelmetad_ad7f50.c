/* JSON-lines harness for the w24_modelmetad_ad7f50 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_modelmetad_ad7f50.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    size_t cap = 1u << 21;          /* one scripted state-machine case is long */
    char *line = (char *)malloc(cap);
    if (!line) return 1;
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, (int)cap, stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "entry_bytes")) {
            out = w24_modelmetad_ad7f50_entry_bytes(in);
        } else if (!strcmp(func, "oversize")) {
            out = w24_modelmetad_ad7f50_oversize(in);
        } else if (!strcmp(func, "memo")) {
            out = w24_modelmetad_ad7f50_memo(in);
        } else if (!strcmp(func, "forget")) {
            out = w24_modelmetad_ad7f50_forget(in);
        } else if (!strcmp(func, "constants")) {
            out = w24_modelmetad_ad7f50_constants();
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
