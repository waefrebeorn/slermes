#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_approvalpro_21df2f.h"

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
        if (!strcmp(func, "consent")) {
            out = json_string(w8_approvalpro_21df2f_consent(json_get_str(in, "choice", ""),
                                             json_get_str(in, "unresolved", "decline")));
        } else if (!strcmp(func, "attempt")) {
            out = w8_approvalpro_21df2f_attempt(json_get_str(in, "name", ""),
                                 json_obj_get(in, "choice"), json_obj_get(in, "failure"),
                                 json_obj_get(in, "fallback"));
        } else if (!strcmp(func, "unanswered")) {
            out = w8_approvalpro_21df2f_unanswered(json_get_str(in, "cause", ""));
        } else if (!strcmp(func, "cli_decision")) {
            out = json_string(w8_approvalpro_21df2f_cli_decision(json_get_str(in, "choice", ""),
                        json_get_bool(in, "once_only", false) ? 1 : 0,
                        json_get_bool(in, "allow_permanent", false) ? 1 : 0,
                        json_obj_get(in, "once_inputs"), json_obj_get(in, "deny_inputs")));
        } else if (!strcmp(func, "i18n_key")) {
            const char *k = w8_approvalpro_21df2f_i18n_key(json_get_str(in, "decision", ""));
            out = k ? json_string(k) : json_null();
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