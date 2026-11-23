#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_updatefini_fefa40.h"
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
        if (!strcmp(func, "restore_plan")) {
            out = w11_updatefini_fefa40_restore_plan(json_obj_get(in, "data"),
                        json_obj_get(in, "plan_fields"), json_obj_get(in, "rt_fields"));
        } else if (!strcmp(func, "tail_owed")) {
            out = json_bool(w11_updatefini_fefa40_tail_owed(
                        json_obj_get(in, "owed"), json_obj_get(in, "tail_followups")) ? true : false);
        } else if (!strcmp(func, "resume_handled")) {
            out = json_bool(w11_updatefini_fefa40_resume_handled(
                        json_get_bool(in, "attempted", false) ? 1 : 0,
                        json_obj_get(in, "token")) ? true : false);
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
