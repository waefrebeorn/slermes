#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_sessionmut_83f0c3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Does the JSON array `arr` contain a string equal to `id`? */
static int arr_contains(json_t *arr, const char *id) {
    if (!arr || arr->type != JSON_ARRAY || !id) return 0;
    for (size_t i = 0; i < json_len(arr); ++i) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, id)) return 1;
    }
    return 0;
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
        if (!strcmp(func, "runtime_owns_authority")) {
            json_t *registry = json_obj_get(in, "registry");
            int has_registry = registry && registry->type == JSON_ARRAY;
            const char *authority_id = json_get_str(in, "authority_id", "");
            int registry_contains = arr_contains(registry, authority_id);
            const char *sa = json_get_str(in, "session_authority", NULL);
            int sa_is = (sa && !strcmp(sa, authority_id)) ? 1 : 0;
            out = json_bool(w12_sessionmut_83f0c3_runtime_owns_authority(
                      has_registry, registry_contains, sa_is) ? 1 : 0);
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
