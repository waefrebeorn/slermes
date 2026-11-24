/* JSON-lines harness for the w22_sessionwor_930a87 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_sessionwor_930a87.h"
#include "gateway/session_worker_delegation.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *opt_str(const char *s) {
    return s ? json_string(s) : NULL;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "plan")) {
            out = w22_sessionwor_930a87_plan(json_get_str(in, "operation", NULL),
                                             json_obj_get(in, "receipt"),
                                             json_obj_get(in, "probe"));
        } else if (!strcmp(func, "probe")) {
            out = w22_sessionwor_930a87_probe(json_obj_get(in, "probe"));
        } else if (!strcmp(func, "delivery_verb")) {
            out = opt_str(w22_sessionwor_930a87_delivery_verb(json_get_str(in, "name", NULL)));
        } else if (!strcmp(func, "request_verdict")) {
            out = opt_str(w22_sessionwor_930a87_request_verdict(json_obj_get(in, "params_keys"),
                                                               json_get_bool(in, "has_adopt", 0)));
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
