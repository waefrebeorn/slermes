#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_paths_f8daf3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int has_key(json_t *o, const char *k) {
    return o && o->type == JSON_OBJECT && json_obj_get(o, k) && !json_is_null(json_obj_get(o, k));
}
static const char *sval(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
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
        char *s = NULL;

        if (!strcmp(func, "install_root")) {
            const char *env = has_key(in, "env") ? sval(in, "env") : NULL;
            if (pmpaths_install_root(sval(in, "repo"), env, &s)) out = json_string(s ? s : "");
        } else if (!strcmp(func, "install_stamp_path")) {
            int res = json_get_bool(in, "resolves", 0);
            if (pmpaths_install_stamp_path(sval(in, "project"), res, sval(in, "inst"), &s))
                out = json_string(s ? s : "");
        } else if (!strcmp(func, "partials_root")) {
            if (pmpaths_partials_root(sval(in, "root"), &s)) out = json_string(s ? s : "");
        } else if (!strcmp(func, "facts_path")) {
            if (pmpaths_facts_path(sval(in, "store"), &s)) out = json_string(s ? s : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(s);
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
    return 0;
}
