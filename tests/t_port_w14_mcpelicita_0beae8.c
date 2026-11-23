#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_mcpelicita_0beae8.h"

static void emit_key(const char *key, struct json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        const struct json_t *spec = json_obj_get(in, "spec");
        const struct json_t *raw = json_obj_get(in, "raw");
        if (!strcmp(func, "options")) {
            out = w14_mcpelicita_0beae8_options(spec);
        } else if (!strcmp(func, "field_kind")) {
            out = w14_mcpelicita_0beae8_field_kind(spec);
        } else if (!strcmp(func, "match_option")) {
            out = w14_mcpelicita_0beae8_match_option(spec, json_get_str(in, "text", ""));
        } else if (!strcmp(func, "coerce_boolean")) {
            out = w14_mcpelicita_0beae8_coerce_boolean(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "format_error")) {
            out = json_string(w14_mcpelicita_0beae8_format_error(
                      json_get_str(in, "fmt", ""), json_get_str(in, "text", "")));
        } else if (!strcmp(func, "coerce_string")) {
            out = w14_mcpelicita_0beae8_coerce_string(spec, json_get_str(in, "text", ""));
        } else if (!strcmp(func, "coerce_number")) {
            out = w14_mcpelicita_0beae8_coerce_number(spec, json_get_str(in, "text", ""));
        } else if (!strcmp(func, "coerce_multi")) {
            out = w14_mcpelicita_0beae8_coerce_multi(spec, raw);
        } else if (!strcmp(func, "coerce_answer")) {
            out = w14_mcpelicita_0beae8_coerce_answer(
                      spec, raw, json_get_bool(in, "required", false) ? 1 : 0);
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
