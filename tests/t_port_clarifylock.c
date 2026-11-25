#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_clarifylock.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int has_key(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return 0;
    for (size_t i = 0; i < json_object_size(obj); i++)
        if (json_object_get_key_at(obj, i) && !strcmp(json_object_get_key_at(obj, i), key))
            return 1;
    return 0;
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
        if (!strcmp(func, "lock")) {
            out = clarifylock_lock(json_obj_get(in, "params"),
                                   json_obj_get(in, "remaining"),
                                   json_get_bool(in, "lock_raises", 0) ? 1 : 0);
        } else if (!strcmp(func, "coerce_answer")) {
            /* absent key -> params.get('answer','') == '' ; present null -> None */
            out = clarifylock_coerce_answer(has_key(in, "answer")
                        ? json_obj_get(in, "answer") : NULL);
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
