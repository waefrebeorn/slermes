#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_sessiondis_3b3fbe.h"
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
        if (!strcmp(func, "discover")) {
            out = w12_sessiondis_3b3fbe_discover(json_obj_get(in, "capabilities"),
                json_obj_get(in, "actor_profile"), json_obj_get(in, "authority_profile"),
                json_obj_get(in, "params"), json_obj_get(in, "completion"), json_obj_get(in, "profile_ok"));
        } else if (!strcmp(func, "list_check")) {
            out = w12_sessiondis_3b3fbe_list_check(json_obj_get(in, "capabilities"),
                json_obj_get(in, "actor_profile"), json_obj_get(in, "authority_profile"),
                json_obj_get(in, "params"));
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
