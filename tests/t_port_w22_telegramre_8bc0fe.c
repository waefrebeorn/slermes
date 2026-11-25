/* JSON-lines harness for the w22_telegramre_8bc0fe gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_telegramre_8bc0fe.h"

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
        if (!strcmp(func, "escape")) {
            out = w22_telegramre_8bc0fe_escape(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "strip")) {
            out = w22_telegramre_8bc0fe_strip(json_get_str(in, "body", NULL),
                                              json_get_str(in, "deadline", ""));
        } else if (!strcmp(func, "plan")) {
            json_t *body = json_get_bool(in, "message", 1) ? json_obj_get(in, "body") : NULL;
            out = w22_telegramre_8bc0fe_plan(body, json_get_str(in, "deadline", ""),
                                             json_get_str(in, "decision", ""),
                                             json_get_str(in, "error", ""));
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
