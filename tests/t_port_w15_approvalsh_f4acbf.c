#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_approvalsh_f4acbf.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static long long get_i64(const json_t *in, const char *key, long long def) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return def;
    return (long long)v->num_val;
}
static const char *jstrd(const json_t *in, const char *k, const char *d) {
    const json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : d;
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
        if (!strcmp(func, "backtick_end")) {
            long long r = w15_approvalsh_f4acbf_backtick_end_from(
                jstrd(in, "segment", ""), get_i64(in, "i", 0));
            out = r < 0 ? json_null() : json_number((double)r);
        } else if (!strcmp(func, "shell_tokens")) {
            out = w15_approvalsh_f4acbf_shell_tokens_with_spans(
                jstrd(in, "segment", ""), get_i64(in, "start", 0));
            if (!out) out = json_null();
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
