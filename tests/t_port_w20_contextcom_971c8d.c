/* JSON-lines harness for the w20_contextcom_971c8d gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_contextcom_971c8d.h"

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
        if (!strcmp(func, "typed_summary_messages")) {
            out = w20_contextcom_971c8d_typed_summary_messages(json_obj_get(in, "turns"),
                                                               json_obj_get(in, "records"),
                                                               json_obj_get(in, "slices"),
                                                               json_obj_get(in, "real_user"),
                                                               json_obj_get(in, "render_args"));
        } else if (!strcmp(func, "call_arguments")) {
            out = w20_contextcom_971c8d_call_arguments(json_obj_get(in, "raw"),
                                                       json_obj_get(in, "render_args"));
        } else if (!strcmp(func, "typed_turns_pointer")) {
            out = json_string(w20_contextcom_971c8d_typed_turns_pointer());
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
