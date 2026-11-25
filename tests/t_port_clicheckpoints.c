#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_clicheckpoints.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int cmp_keys(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
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
        json_t *shape = clicheckpoints_build_parser_shape();
        if (!strcmp(func, "shape")) {
            out = json_copy(shape);
        } else if (!strcmp(func, "keys")) {
            size_t cnt = json_object_size(shape);
            const char **ks = cnt ? (const char **)malloc(cnt * sizeof(char *)) : NULL;
            for (size_t i = 0; i < cnt; i++) ks[i] = json_object_get_key_at(shape, i);
            if (cnt > 1) qsort(ks, cnt, sizeof(char *), cmp_keys);
            out = json_array();
            for (size_t i = 0; i < cnt; i++) json_append(out, json_string(ks[i]));
            free(ks);
        } else if (!strcmp(func, "missing")) {
            out = json_null();
        } else {
            json_t *v = json_obj_get(shape, func);
            out = v ? json_copy(v) : json_null();
        }
        json_free(shape);
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
