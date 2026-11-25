/* JSON-lines harness for the w22_locallinea_3bb0fd gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_locallinea_3bb0fd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "validate")) {
            out = w22_locallinea_3bb0fd_validate(json_obj_get(in, "receipt"),
                                                 json_get_str(in, "local_id", ""),
                                                 json_get_str(in, "root", ""),
                                                 json_obj_get(in, "db"));
        } else if (!strcmp(func, "routed")) {
            out = w22_locallinea_3bb0fd_routed_entry(json_get_str(in, "route", ""),
                                                     json_get_str(in, "tip", ""),
                                                     json_obj_get(in, "db"));
        } else if (!strcmp(func, "advance")) {
            out = w22_locallinea_3bb0fd_advance(json_get_str(in, "parent_session_id", ""),
                                                json_get_str(in, "child_session_id", ""),
                                                json_obj_get(in, "entry"),
                                                json_obj_get(in, "db"),
                                                json_get_str(in, "local_id", ""),
                                                json_get_str(in, "root", ""));
        } else if (!strcmp(func, "policy_prefix")) {
            out = json_string(w22_locallinea_3bb0fd_policy_prefix());
        } else if (!strcmp(func, "legacy_prefix")) {
            out = json_string(w22_locallinea_3bb0fd_legacy_prefix());
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
