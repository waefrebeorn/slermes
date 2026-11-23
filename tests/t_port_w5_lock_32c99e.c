#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_lock_32c99e.h"

static char *field_json(json_t *in, const char *key, const char *def) {
    json_t *v = json_obj_get(in, key);
    return v ? json_dumps(v, 0) : strdup(def);
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

        if (!strcmp(func, "templatize")) {
            char *e = field_json(in, "env", "{}");
            char *s = pmlock_templatize(e, json_get_str(in, "root", ""));
            out = s ? json_parse(s, NULL) : json_null();
            free(s); free(e);
        } else if (!strcmp(func, "resolve")) {
            char *e = field_json(in, "env", "{}");
            char *s = pmlock_resolve(e, json_get_str(in, "root", ""));
            out = s ? json_parse(s, NULL) : json_null();
            free(s); free(e);
        } else if (!strcmp(func, "artifacts")) {
            char *p = field_json(in, "packages", "{}");
            char *s = NULL;
            int ok = pmlock_artifacts(p, json_get_str(in, "name", ""),
                                      json_get_str(in, "target", ""), &s);
            out = ok && s ? json_parse(s, NULL) : json_null();
            free(s); free(p);
        } else if (!strcmp(func, "tool_rows")) {
            char *f = field_json(in, "fact", "null");
            char *s = pmlock_tool_rows(f);
            out = s ? json_parse(s, NULL) : json_null();
            free(s); free(f);
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
    return 0;
}
