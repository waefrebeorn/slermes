#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_backupinte_5de948.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static long long get_i64(json_t *o, const char *k, long long def) {
    json_t *v = json_obj_get(o, k);
    if (!v) return def;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
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
        if (!strcmp(func, "verify")) {
            out = w15_backupinte_5de948_verify_sqlite_integrity(
                json_get_str(in, "path", ""),
                json_get_bool(in, "check_header", 1),
                json_get_bool(in, "run_pragma", 1),
                get_i64(in, "max_bytes", 2147483648LL));
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
