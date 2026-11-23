#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_install_184801.h"

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
        char *tmp = NULL;

        if (!strcmp(func, "extra")) {
            tmp = w5_install_184801_extra_key(json_get_str(in, "name", ""));
            out = tmp ? json_string(tmp) : json_null();
        } else if (!strcmp(func, "identity")) {
            json_t *arts = json_obj_get(in, "artifacts");
            size_t n = json_is_array(arts) ? json_len(arts) : 0;
            const char **sha = (const char **)calloc(n ? n : 1, sizeof(char *));
            for (size_t i = 0; i < n; i++) {
                json_t *e = json_get(arts, i);
                sha[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
            }
            out = w5_install_184801_identity(json_get_str(in, "target", ""), sha, n);
            free(sha);
        } else if (!strcmp(func, "order")) {
            json_t *names = json_obj_get(in, "names");
            json_t *deps = json_obj_get(in, "deps");
            size_t n = json_is_array(names) ? json_len(names) : 0;
            const char **ns = (const char **)calloc(n ? n : 1, sizeof(char *));
            const char ***ds = (const char ***)calloc(n ? n : 1, sizeof(char **));
            for (size_t i = 0; i < n; i++) {
                json_t *e = json_get(names, i);
                ns[i] = (e && e->type == JSON_STRING) ? e->str_val : "";
                json_t *dv = json_obj_get(deps, ns[i]);
                size_t m = json_is_array(dv) ? json_len(dv) : 0;
                const char **arr = (const char **)calloc(m + 1, sizeof(char *));
                for (size_t k = 0; k < m; k++) {
                    json_t *x = json_get(dv, k);
                    arr[k] = (x && x->type == JSON_STRING) ? x->str_val : "";
                }
                arr[m] = NULL;
                ds[i] = arr;
            }
            size_t on = 0;
            char **res = w5_install_184801_dependents_first(ns, n,
                                (const char *const *const *)ds, &on);
            out = json_array();
            for (size_t i = 0; i < on; i++) json_append(out, json_string(res[i]));
            w5_install_184801_strings_free(res, on);
            for (size_t i = 0; i < n; i++) free((void *)ds[i]);
            free(ds); free(ns);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(tmp);

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
