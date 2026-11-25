#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_webserver_a210ca.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "truncate")) {
            out = w15_webserver_a210ca_truncate_token(json_get_str(in, "value", ""),
                        (int)json_get_num(in, "visible", 6),
                        json_get_bool(in, "callable", false) ? 1 : 0);
        } else if (!strcmp(func, "profile")) {
            out = w15_webserver_a210ca_oauth_profile_name(json_get_str(in, "profile", ""));
        } else if (!strcmp(func, "token_status")) {
            out = w15_webserver_a210ca_token_status(json_get_str(in, "source", ""),
                        json_get_str(in, "label", ""), json_obj_get(in, "creds"),
                        json_get_bool(in, "access_token_callable", false) ? 1 : 0);
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
