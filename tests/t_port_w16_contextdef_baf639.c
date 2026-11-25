/* t_port_w16_contextdef_baf639.c — JSON-lines harness for context_default_record.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_contextdef_baf639.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "key")) {
            out = w16_contextdef_baf639_key(json_get_str(in, "profile_home", ""),
                                            json_get_str(in, "model", ""),
                                            json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "was_context_defaulted")) {
            out = w16_contextdef_baf639_was_context_defaulted(json_obj_get(in, "records"),
                    json_get_str(in, "profile_home", ""), json_get_str(in, "context_length", "0"),
                    json_get_str(in, "model", ""), json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "record_resolution")) {
            out = w16_contextdef_baf639_record_resolution(json_obj_get(in, "records"),
                    json_get_str(in, "profile_home", ""),
                    (long long)json_get_num(in, "max_records", 512),
                    json_get_bool(in, "outer_present", 0) ? 1 : 0,
                    json_get_bool(in, "flag", 0) ? 1 : 0,
                    json_get_str(in, "context_length", "0"),
                    json_get_str(in, "model", ""), json_get_str(in, "base_url", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
