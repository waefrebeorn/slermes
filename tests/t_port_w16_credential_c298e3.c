/* t_port_w16_credential_c298e3.c — JSON-lines harness for credential_pool_errors.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_credential_c298e3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "parse_absolute_timestamp")) {
            out = w16_credential_c298e3_parse_absolute_timestamp(json_obj_get(in, "value"));
        } else if (!strcmp(func, "redact_error_text")) {
            out = w16_credential_c298e3_redact_error_text(json_get_str(in, "text", ""),
                                                          json_obj_get(in, "known_secrets"));
        } else if (!strcmp(func, "normalize_error_context")) {
            out = w16_credential_c298e3_normalize_error_context(json_obj_get(in, "error_context"),
                                                               json_obj_get(in, "known_secrets"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
