#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_contextpin_633375.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int kind_of(const char *s) {
    if (!s || !strcmp(s, "int")) return W6_CP_INT;
    if (!strcmp(s, "bool")) return W6_CP_BOOL;
    if (!strcmp(s, "float")) return W6_CP_FLOAT;
    if (!strcmp(s, "none")) return W6_CP_NONE;
    return W6_CP_STR;
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
        int ck = kind_of(json_get_str(in, "context_kind", "int"));
        int pk = kind_of(json_get_str(in, "config_kind", "int"));
        const char *cv = json_get_str(in, "context", "0");
        const char *pv = json_get_str(in, "config", "0");
        const char *model = json_get_str(in, "model", "");
        if (!strcmp(func, "is_context_pinned")) {
            out = json_bool(w6_contextpin_633375_is_context_pinned(ck, cv, pk, pv));
        } else if (!strcmp(func, "context_pin_suffix")) {
            out = json_string(w6_contextpin_633375_context_pin_suffix(ck, cv, pk, pv));
        } else if (!strcmp(func, "context_window_marker")) {
            out = json_string(w6_contextpin_633375_context_window_marker(ck, cv, pk, pv, model));
        } else if (!strcmp(func, "context_window_suffix")) {
            out = json_string(w6_contextpin_633375_context_window_suffix(ck, cv, pk, pv, model));
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
