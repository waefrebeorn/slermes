#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_cloudagents_e05d38.h"

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
        if (!strcmp(func, "org")) {
            out = w9_cloudagents_e05d38_org(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "agent")) {
            out = w9_cloudagents_e05d38_agent(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "response")) {
            out = w9_cloudagents_e05d38_discover_response(
                      (int)json_get_num(in, "status_code", 200),
                      json_obj_get(in, "body"), json_get_str(in, "org", NULL));
        } else if (!strcmp(func, "status")) {
            out = json_number((double)w9_cloudagents_e05d38_portal_signin_status());
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
