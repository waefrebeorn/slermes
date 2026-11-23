/* t_port_w9_permissions_d89402.c -- JSON-lines harness for src/acp_adapter/permissions.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_permissions_d89402.h"

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
        if (!strcmp(func, "resolve_timeout")) {
            json_t *tv = json_obj_get(in, "timeout");
            int given = (tv && tv->type == JSON_NUMBER) ? 1 : 0;
            double timeout = given ? tv->num_val : 0.0;
            double cfg = json_get_num(in, "config_timeout", 0.0);
            out = json_number(w9_permissions_d89402_resolve_timeout(timeout, given, cfg));
        } else if (!strcmp(func, "map_outcome")) {
            const char *r = w9_permissions_d89402_map_outcome(
                json_get_str(in, "option_id", ""),
                json_get_bool(in, "valid_outcome", false) ? 1 : 0,
                json_obj_get(in, "allowed"));
            out = json_string(r ? r : "deny");
        } else if (!strcmp(func, "build_options")) {
            out = w9_permissions_d89402_build_options(
                json_get_bool(in, "allow_permanent", false) ? 1 : 0,
                json_get_bool(in, "allow_session", true) ? 1 : 0,
                json_get_bool(in, "smart_denied", false) ? 1 : 0,
                json_get_bool(in, "supports_reject_always", true) ? 1 : 0);
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
