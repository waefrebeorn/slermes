/* t_port_w14_hostedroom_818b6c.c — oracle harness for
 * gateway/hosted_rooms_common.py (gate w14_hostedroom_818b6c). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_hostedroom_818b6c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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

        if (!strcmp(func, "identifier")) {
            out = w14_hostedroom_818b6c_identifier(
                json_obj_get(in, "value"), json_get_str(in, "label", "id"),
                (int)json_get_num(in, "max_chars", 128),
                json_get_bool(in, "skip_pattern", false) ? 1 : 0,
                opt_str(in, "invalid"));
        } else if (!strcmp(func, "bounded_int")) {
            json_t *hi = json_obj_get(in, "high");
            out = w14_hostedroom_818b6c_bounded_int(
                json_obj_get(in, "value"), json_get_str(in, "message", ""),
                (long)json_get_num(in, "low", 0), hi ? 1 : 0,
                hi ? (long)json_get_num(in, "high", 0) : 0);
        } else if (!strcmp(func, "exact_fields")) {
            out = w14_hostedroom_818b6c_exact_fields(
                json_obj_get(in, "value"), json_get_str(in, "label", "obj"),
                json_obj_get(in, "required"), json_obj_get(in, "optional"),
                opt_str(in, "not_object"),
                json_get_str(in, "missing_fmt", "{label} is missing fields: {fields}"),
                json_get_str(in, "unknown_fmt", "{label} has unknown fields: {fields}"));
        } else if (!strcmp(func, "text")) {
            out = w14_hostedroom_818b6c_text(
                json_obj_get(in, "value"), json_get_str(in, "label", "note"),
                (int)json_get_num(in, "max_bytes", 32),
                json_get_bool(in, "strip", true) ? 1 : 0);
        } else if (!strcmp(func, "utf8_len")) {
            out = w14_hostedroom_818b6c_utf8_len(json_obj_get(in, "parts"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
