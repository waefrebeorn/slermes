#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_mcptoolcom_f3c09b.h"

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
        if (!strcmp(func, "env_ref_name")) {
            char *s = w8_mcptoolcom_f3c09b_env_ref_name(json_get_str(in, "ref", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "safe_numeric")) {
            const char *co = json_get_str(in, "coerce", "int");
            out = w8_mcptoolcom_f3c09b_safe_numeric(
                json_obj_get(in, "value"), json_get_num(in, "default", 0),
                strcmp(co, "float") != 0, json_get_num(in, "minimum", 1));
        } else if (!strcmp(func, "parse_boolish")) {
            out = json_bool(w8_mcptoolcom_f3c09b_parse_boolish(
                json_obj_get(in, "value"), json_get_bool(in, "default", 1)) ? true : false);
        } else if (!strcmp(func, "mcp_server_enabled")) {
            out = json_bool(w8_mcptoolcom_f3c09b_mcp_server_enabled(json_obj_get(in, "cfg")) ? true : false);
        } else if (!strcmp(func, "get_lifecycle_seconds")) {
            out = w8_mcptoolcom_f3c09b_get_lifecycle_seconds(json_obj_get(in, "cfg"),
                  json_get_str(in, "key", ""));
        } else if (!strcmp(func, "prepend_path")) {
            out = w8_mcptoolcom_f3c09b_prepend_path(json_obj_get(in, "env"),
                  json_get_str(in, "directory", ""));
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
