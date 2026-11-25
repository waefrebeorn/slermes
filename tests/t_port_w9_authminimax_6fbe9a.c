#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_authminimax_6fbe9a.h"

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
        json_t *out = NULL;
        double now = json_get_num(in, "now_seconds", 0.0);
        if (!strcmp(func, "looks_like_ms"))
            out = json_bool(w9_authminimax_6fbe9a_expired_in_looks_like_unix_ms(
                        json_obj_get(in, "expired_in"), json_obj_get(in, "now_ms")) ? true : false);
        else if (!strcmp(func, "resolve"))
            out = json_number(w9_authminimax_6fbe9a_resolve_token_expiry_unix(
                        json_obj_get(in, "expired_in"), now));
        else if (!strcmp(func, "expiry_fields"))
            out = w9_authminimax_6fbe9a_expiry_fields(json_obj_get(in, "expired_in"), now);
        else
            out = json_string("UNKNOWN_FUNC");
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
