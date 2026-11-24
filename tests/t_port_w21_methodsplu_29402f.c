/* JSON-lines harness for the w21_methodsplu_29402f gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_methodsplu_29402f.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "lookup")) {
            out = w21_methodsplu_29402f_lookup(json_obj_get(in, "params"),
                                               json_obj_get(in, "entries"),
                                               json_obj_get(in, "removed"));
        } else if (!strcmp(func, "looks_like")) {
            out = w21_methodsplu_29402f_looks_like_catalog_name(json_obj_get(in, "name"));
        } else if (!strcmp(func, "normalize_repo")) {
            json_t *u = json_obj_get(in, "url");
            char *s = w21_methodsplu_29402f_normalize_repo(
                (u && u->type == JSON_STRING) ? u->str_val : NULL);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "match_removed")) {
            out = w21_methodsplu_29402f_match_removed(json_obj_get(in, "candidate"),
                                                      json_obj_get(in, "removed"));
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
