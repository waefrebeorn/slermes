/* Port gate harness for agent/auxiliary_key_rotation.py (gate auxkeyrot). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_auxkeyrot.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* string-or-null fetch: NULL pointer means Python None. */
static const char *jstr_or_null(const json_t *in, const char *k, int *is_null) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) { *is_null = 1; return NULL; }
    *is_null = 0;
    return v->type == JSON_STRING ? v->str_val : "";
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
        if (!strcmp(func, "rotate")) {
            int old_null = 0, new_null = 0, cur_null = 0;
            const char *old = jstr_or_null(in, "old", &old_null);
            const char *newv = jstr_or_null(in, "new", &new_null);
            const char *cur = jstr_or_null(in, "current", &cur_null);
            int is_dict = json_get_bool(in, "runtime_is_dict", 1) ? 1 : 0;
            int rotated = 0;
            char *res = auxkeyrot_rotate_runtime_main_api_key(
                old, old_null, newv, new_null, cur, cur_null, is_dict, &rotated);
            out = res ? json_string(res) : json_null();
            free(res);
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
