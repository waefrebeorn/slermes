#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_approvalans_fc15b6.h"

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
        const char *text = json_get_str(in, "text", "");
        json_t *out = NULL;
        if (!strcmp(func, "expand")) {
            char *r = w8_approvalans_fc15b6_expand(json_get_str(in, "command", ""));
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "span_end")) {
            long long e = w8_approvalans_fc15b6_span_end(text, (long long)json_get_num(in, "start", 0));
            out = e < 0 ? json_null() : json_number((double)e);
        } else if (!strcmp(func, "comment_start")) {
            out = json_bool(w8_approvalans_fc15b6_is_shell_comment_start(text, (long long)json_get_num(in, "index", 0)) ? true : false);
        } else if (!strcmp(func, "skip_cont")) {
            out = json_number((double)w8_approvalans_fc15b6_skip_continuations(text, (long long)json_get_num(in, "i", 0)));
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