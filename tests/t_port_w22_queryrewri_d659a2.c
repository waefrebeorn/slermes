/* JSON-lines harness for the w22_queryrewri_d659a2 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_queryrewri_d659a2.h"
#include "plugins/memory/query_rewrite.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_out(char *s) {
    json_t *out = s ? json_string(s) : json_null();
    free(s);
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
        if (!strcmp(func, "bounded")) {
            out = str_out(w22_queryrewri_d659a2_bounded_user_message(json_get_str(in, "message", NULL)));
        } else if (!strcmp(func, "extract")) {
            out = str_out(w22_queryrewri_d659a2_extract_response_text(json_obj_get(in, "response")));
        } else if (!strcmp(func, "normalize")) {
            out = str_out(w22_queryrewri_d659a2_normalize_rewrite(json_get_str(in, "text", NULL)));
        } else if (!strcmp(func, "rewrite")) {
            out = str_out(w22_queryrewri_d659a2_rewrite(json_get_str(in, "message", NULL),
                                                        json_obj_get(in, "response"),
                                                        json_get_bool(in, "llm_raises", false)));
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
