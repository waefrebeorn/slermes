/* JSON-lines harness for the w32_toolsmcppl_39bf88 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_toolsmcppl_39bf88.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "methods")) {
            out = w32_toolsmcppl_39bf88_methods();
        } else if (!strcmp(func, "enums")) {
            out = w32_toolsmcppl_39bf88_enums();
        } else if (!strcmp(func, "params_spec")) {
            out = w32_toolsmcppl_39bf88_params_spec(json_get_str(in, "method", ""));
        } else if (!strcmp(func, "validate_params")) {
            out = w32_toolsmcppl_39bf88_validate_params(json_get_str(in, "method", ""),
                                                        json_obj_get(in, "params"));
        } else if (!strcmp(func, "normalize_params")) {
            out = w32_toolsmcppl_39bf88_normalize_params(json_get_str(in, "method", ""),
                                                         json_obj_get(in, "params"));
        } else if (!strcmp(func, "validate_result")) {
            out = w32_toolsmcppl_39bf88_validate_result(json_get_str(in, "method", ""),
                                                        json_obj_get(in, "result"));
        } else if (!strcmp(func, "profile_scoped")) {
            out = w32_toolsmcppl_39bf88_profile_scoped();
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
