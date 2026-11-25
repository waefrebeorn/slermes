#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_webupload_e1e756.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long get_i64(const json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
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
        if (!strcmp(func, "check")) {
            out = w15_webupload_e1e756_check(
                json_get_str(in, "path", ""), json_get_str(in, "root_path", ""),
                json_get_str(in, "scope_type", "http"), json_get_str(in, "method", "POST"),
                json_get_bool(in, "browser_files", true) ? 1 : 0,
                json_get_str(in, "content_length", ""),
                get_i64(in, "chunk_total", 0),
                get_i64(in, "file_max", 16842752),
                get_i64(in, "image_max", 35018112));
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
