#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_approvalno_b886ee.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
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
        if (!strcmp(func, "current")) {
            out = json_bool(w15_approvalno_b886ee_current(
                jstr(json_obj_get(in, "session_key")),
                jstr(json_obj_get(in, "current_lease_key"))) ? 1 : 0);
        } else if (!strcmp(func, "deliverable")) {
            out = json_bool(w15_approvalno_b886ee_deliverable(
                json_get_bool(in, "active", 0),
                json_get_bool(in, "has_callback", 0),
                json_get_bool(in, "gateway_has_notifier", 0)) ? 1 : 0);
        } else if (!strcmp(func, "revoke")) {
            out = w15_approvalno_b886ee_revoke(json_obj_get(in, "lease_keys"),
                                               jstr(json_obj_get(in, "session_key")));
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
