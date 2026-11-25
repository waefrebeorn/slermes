/* JSON-lines harness for the w25_callbackan_bbb650 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_callbackan_bbb650.h"

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
        if (!strcmp(func, "is_stale_query_error")) {
            out = w25_callbackan_bbb650_is_stale_query_error(in);
        } else if (!strcmp(func, "answer_callback")) {
            out = w25_callbackan_bbb650_answer_callback(in);
        } else if (!strcmp(func, "answer_or_note")) {
            out = w25_callbackan_bbb650_answer_or_note(in);
        } else if (!strcmp(func, "refuse_tap")) {
            out = w25_callbackan_bbb650_refuse_tap(in);
        } else if (!strcmp(func, "constants")) {
            out = w25_callbackan_bbb650_constants();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
