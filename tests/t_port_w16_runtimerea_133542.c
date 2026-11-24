/* JSON-lines harness for the w16_runtimerea_133542 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_runtimerea_133542.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_opt(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "suffix")) {
            char *s = w16_runtimerea_133542_missing_suffix(
                str_opt(in, "provider"),
                json_get_bool(in, "profile_present", 0) ? 1 : 0,
                json_get_str(in, "hint", ""),
                (int)json_get_num(in, "hint_kind", 0));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "creds")) {
            out = json_bool(w16_runtimerea_133542_has_credentials(
                str_opt(in, "api_key"),
                json_get_bool(in, "api_key_callable", 0) ? 1 : 0,
                str_opt(in, "command"),
                json_obj_get(in, "usable_secrets")) ? 1 : 0);
        } else if (!strcmp(func, "failure")) {
            out = w16_runtimerea_133542_readiness_failure(
                json_get_str(in, "error_text", ""),
                json_get_bool(in, "has_error_code", 0) ? 1 : 0,
                json_get_str(in, "error_code", ""));
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
