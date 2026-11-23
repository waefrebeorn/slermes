#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_provenance_7265b6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *nonempty_prev(json_t *in) {
    json_t *v = json_obj_get(in, "previous");
    return (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) ? v->str_val : NULL;
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *sessions = json_obj_get(in, "sessions");
        const char *acp = json_get_str(in, "acp_id", "");
        const char *cur = json_get_str(in, "current", "");
        const char *prev = nonempty_prev(in);
        json_t *out = NULL;
        if (!strcmp(func, "build"))
            out = w8_provenance_7265b6_build(sessions, acp, cur, prev);
        else if (!strcmp(func, "meta"))
            out = w8_provenance_7265b6_meta(sessions, acp, cur, prev);
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
