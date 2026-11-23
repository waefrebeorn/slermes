#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_extras_3292f9.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *arr_to_json(char **arr, size_t n) {
    json_t *o = json_array();
    for (size_t i = 0; i < n; i++) json_append(o, json_string(arr[i]));
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *extra = json_get_str(in, "extra", "");
        json_t *out = NULL;

        if (!strcmp(func, "canonical")) {
            char *s = w5_extras_3292f9_canonical(json_get_str(in, "name", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "hint")) {
            char *s = w5_extras_3292f9_install_hint(extra);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "anchors")) {
            size_t n = 0;
            char **a = w5_extras_3292f9_anchors(extra, &n);
            out = arr_to_json(a, n);
            w5_extras_3292f9_strings_free(a, n);
        } else if (!strcmp(func, "missing")) {
            json_t *impj = json_obj_get(in, "importable");
            size_t ni = json_array_size(impj);
            const char **imp = NULL;
            if (ni) imp = (const char **)calloc(ni, sizeof(char *));
            for (size_t i = 0; i < ni; i++) {
                json_t *v = json_get(impj, i);
                imp[i] = (v && v->type == JSON_STRING) ? v->str_val : "";
            }
            size_t n = 0;
            char **a = w5_extras_3292f9_missing(extra, imp, ni, &n);
            out = arr_to_json(a, n);
            w5_extras_3292f9_strings_free(a, n);
            free(imp);
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
