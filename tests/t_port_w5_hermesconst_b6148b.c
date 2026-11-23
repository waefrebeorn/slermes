#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesconst_b6148b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long get_i64(json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
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

        if (!strcmp(func, "is_permission_error")) {
            int r = hcsstuck_is_permission_error(json_get_bool(in, "is_oserror", 0),
                                                  get_i64(in, "err", 0));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "accessible")) {
            int r = hcsstuck_accessible_ex(json_get_bool(in, "lexists", 0),
                                            json_get_bool(in, "access_ok", 0));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "still_stuck")) {
            const char *rec = json_get_str(in, "rec", "null");
            const char *fp  = json_get_str(in, "fp", "null");
            const char *acc = json_get_str(in, "access", "[]");
            int r = hcsstuck_still_stuck(rec, fp, get_i64(in, "uid", 0),
                                         json_get_num(in, "now", 0.0), acc);
            out = json_bool(r != 0);
        } else if (!strcmp(func, "shlex_quote")) {
            char *q = hcsstuck_shlex_quote(json_get_str(in, "s", ""));
            out = q ? json_string(q) : json_null();
            free(q);
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
