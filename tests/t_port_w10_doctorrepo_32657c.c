#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_doctorrepo_32657c.h"
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
        if (!strcmp(func, "check_bool")) {
            out = w10_doctorrepo_32657c_check_bool(json_obj_get(in, "cond"),
                    json_get_bool(in, "fail", false) ? 1 : 0,
                    json_obj_get(in, "ok"), json_obj_get(in, "bad"));
        } else if (!strcmp(func, "finding_merge")) {
            out = w10_doctorrepo_32657c_finding_merge(json_obj_get(in, "first"), json_obj_get(in, "second"));
        } else if (!strcmp(func, "ensure_dir")) {
            out = w10_doctorrepo_32657c_ensure_dir(json_get_bool(in, "exists", false) ? 1 : 0,
                    json_get_bool(in, "should_fix", false) ? 1 : 0,
                    json_get_str(in, "exists_msg", ""), json_get_str(in, "created_msg", ""),
                    json_get_str(in, "missing_msg", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
