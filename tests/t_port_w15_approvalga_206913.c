#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_approvalga_206913.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *str_or_null(char *s) {
    json_t *o = s ? json_string(s) : json_null();
    free(s);
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "identity_digest")) {
            out = str_or_null(w15_approvalga_206913_identity_digest(jstr(json_obj_get(in, "text"))));
        } else if (!strcmp(func, "same_answerers")) {
            out = json_bool(w15_approvalga_206913_same_answerers(
                json_obj_get(in, "leader"), json_obj_get(in, "follower")) ? 1 : 0);
        } else if (!strcmp(func, "hook_choice")) {
            out = str_or_null(w15_approvalga_206913_hook_choice(
                json_get_bool(in, "resolved", 0), json_obj_get(in, "choice"),
                json_obj_get(in, "extra")));
        } else if (!strcmp(func, "cancel_cause")) {
            const char *ir = jstr(json_obj_get(in, "interrupt_reason"));
            out = str_or_null(w15_approvalga_206913_cancel_cause(
                json_get_str(in, "state", ""), json_obj_get(in, "entry"), ir));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
