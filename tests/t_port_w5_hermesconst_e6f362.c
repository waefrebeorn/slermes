#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesconst_e6f362.h"

static long long get_i64(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "encode")) {
            char *s = NULL;
            json_t *cv = json_obj_get(in, "chain");
            char *cs = cv ? json_dumps(cv, 0) : strdup("null");
            out = hcsdetach_encode_lineage(cs, &s) && s
                ? json_string(s) : json_null();
            free(s); free(cs);
        } else if (!strcmp(func, "decode")) {
            char *s = NULL;
            out = hcsdetach_decode_lineage(json_get_str(in, "lineage", ""), &s) && s
                ? json_parse(s, NULL) : json_null();
            free(s);
        } else if (!strcmp(func, "child_script")) {
            char *s = hcsdetach_child_script();
            out = s ? json_string(s) : json_null();
            free(s);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        if (!out) out = json_null();

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
    (void)get_i64;
    return 0;
}
