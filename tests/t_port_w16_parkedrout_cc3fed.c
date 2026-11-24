#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_parkedrout_cc3fed.h"
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
        if (!strcmp(func, "refused_info")) {
            out = w16_parkedrout_cc3fed_refused_info();
        } else if (!strcmp(func, "profile_refusal")) {
            out = w16_parkedrout_cc3fed_profile_refusal(json_obj_get(in, "profile"));
        } else if (!strcmp(func, "notice_due")) {
            out = w16_parkedrout_cc3fed_notice_due(json_obj_get(in, "sent"), json_obj_get(in, "now"),
                                                  json_obj_get(in, "profile"), json_obj_get(in, "platform"),
                                                  json_obj_get(in, "chat_id"));
        } else if (!strcmp(func, "constants")) {
            out = w16_parkedrout_cc3fed_constants();
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
