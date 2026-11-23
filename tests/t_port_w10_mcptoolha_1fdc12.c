#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_mcptoolha_1fdc12.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        if (!strcmp(func, "trust_approval_key")) {
            tmp = w10_mcptoolha_1fdc12_trust_approval_key(
                jstr(json_obj_get(in,"server")), jstr(json_obj_get(in,"tool")));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "result_is_error")) {
            out = json_bool(w10_mcptoolha_1fdc12_result_is_error(jstr(json_obj_get(in,"result"))));
        } else if (!strcmp(func, "json_type_stable_equal")) {
            out = json_bool(w10_mcptoolha_1fdc12_json_type_stable_equal(
                json_obj_get(in,"left"), json_obj_get(in,"right")));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
