#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_browservau_c65b17.h"

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
        if (!strcmp(func, "parse_json_result")) {
            out = w11_browservau_c65b17_parse_json_result(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "deep_probe")) {
            out = w11_browservau_c65b17_deep_probe(json_get_str(in, "selector", ""));
        } else if (!strcmp(func, "list")) {
            out = w11_browservau_c65b17_list(json_obj_get(in, "backends"),
                        json_get_bool(in, "can_prompt", false) ? 1 : 0);
        } else if (!strcmp(func, "unlock")) {
            out = w11_browservau_c65b17_unlock(json_obj_get(in, "backends"),
                        json_get_str(in, "backend_name", ""),
                        json_get_bool(in, "can_prompt", false) ? 1 : 0,
                        json_get_str(in, "prompt_master", ""));
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
