/* t_port_appquote.c — oracle harness for tools/approval_quoted_newlines.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_appquote.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *cmd = json_get_str(in, "command", "");

        char *r = NULL;
        if (!strcmp(func, "mask")) {
            r = appquote_mask_quoted_newlines(cmd);
        } else if (!strcmp(func, "mask_span")) {
            /* int64 passed as decimal strings (libjson numbers are double). */
            const char *ss = json_get_str(in, "start", "0");
            const char *es = json_get_str(in, "end", "");
            long long s = strtoll(ss, NULL, 10);
            long long e = es[0] ? strtoll(es, NULL, 10) : (long long)strlen(cmd);
            r = appquote_mask_quoted_newlines_span(cmd, s, e);
        }
        json_t *out = r ? json_string(r) : json_string("UNKNOWN_FUNC");
        free(r);

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
