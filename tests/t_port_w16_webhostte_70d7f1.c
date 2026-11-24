#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_webhostte_70d7f1.h"

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
        if (!strcmp(func, "lc")) {
            out = w16_webhostte_70d7f1_terminal_lc_ctype(
                json_obj_get(in, "env"), json_get_str(in, "platform", ""));
        } else if (!strcmp(func, "dim")) {
            out = json_number(w16_webhostte_70d7f1_query_dimension(
                json_obj_get(in, "raw"), json_get_num(in, "default", 10),
                json_get_num(in, "maximum", 100)));
        } else if (!strcmp(func, "allowed")) {
            out = json_bool(w16_webhostte_70d7f1_request_allowed(
                json_get_bool(in, "host_terminal", false),
                json_get_bool(in, "auth_required", false),
                json_get_str(in, "bound_host", ""),
                json_obj_get(in, "loopback_hosts")));
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
