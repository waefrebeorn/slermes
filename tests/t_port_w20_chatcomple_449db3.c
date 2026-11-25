/* JSON-lines harness for the w20_chatcomple_449db3 gate.
 * One request object per stdin line; one response per line with "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_chatcomple_449db3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "run")) {
            out = w20_chatcomple_449db3_run(json_obj_get(in, "chunks"));
        } else if (!strcmp(func, "flatten")) {
            char *text = w20_chatcomple_449db3_flatten_text(json_obj_get(in, "content"));
            out = json_string(text ? text : "");
            free(text);
        } else if (!strcmp(func, "separate")) {
            char *text = w20_chatcomple_449db3_separate_reasoning(
                json_get_str(in, "previous", ""), json_obj_get(in, "delta"));
            out = json_string(text ? text : "");
            free(text);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
