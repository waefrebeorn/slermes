/* JSON-lines harness for the w22_audit_b943f2 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_audit_b943f2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "bounded")) {
            out = w22_audit_b943f2_bounded(json_obj_get(in, "value"));
        } else if (!strcmp(func, "event_value")) {
            const char *v = w22_audit_b943f2_event_value(json_get_str(in, "name", NULL));
            out = v ? json_string(v) : json_null();
        } else if (!strcmp(func, "is_redacted")) {
            out = json_bool(w22_audit_b943f2_is_redacted(json_get_str(in, "key", NULL)) != 0);
        } else if (!strcmp(func, "line")) {
            char *text = w22_audit_b943f2_line(json_get_str(in, "ts", NULL),
                                              json_get_str(in, "event", NULL),
                                              json_obj_get(in, "fields"));
            out = text ? json_string(text) : json_null();
            free(text);
        } else if (!strcmp(func, "rotation")) {
            out = w22_audit_b943f2_rotation(json_get_str(in, "name", NULL),
                                            json_get_str(in, "size", NULL),
                                            json_get_str(in, "pending", NULL),
                                            json_obj_get(in, "existing"));
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
