#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegateto_56c3a1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static long long idx(const json_t *in) { return (long long)json_get_num(in, "index", 0); }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "param_error")) {
            out = w14_delegateto_56c3a1_param_error(idx(in), json_get_str(in, "name", ""),
                        json_obj_get(in, "value"), json_obj_get(in, "schema"));
        } else if (!strcmp(func, "clean_route")) {
            out = w14_delegateto_56c3a1_clean_route(json_get_str(in, "provider_name", ""), json_obj_get(in, "route"));
        } else if (!strcmp(func, "with_inherited_fallback")) {
            out = w14_delegateto_56c3a1_with_inherited_fallback(json_obj_get(in, "route"), json_obj_get(in, "config"));
        } else if (!strcmp(func, "report")) {
            out = w14_delegateto_56c3a1_route_report(json_get_str(in, "backend", "default"), json_obj_get(in, "label"));
        } else if (!strcmp(func, "refused_entry")) {
            out = w14_delegateto_56c3a1_route_refused_entry(idx(in), json_get_str(in, "backend", "default"),
                        json_obj_get(in, "label"), json_obj_get(in, "error"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
