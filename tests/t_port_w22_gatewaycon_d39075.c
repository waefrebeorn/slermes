/* JSON-lines harness for the w22_gatewaycon_d39075 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_gatewaycon_d39075.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "dial_fields")) {
            out = w22_gatewaycon_d39075_dial_fields();
        } else if (!strcmp(func, "poll_interval")) {
            out = json_number(w22_gatewaycon_d39075_poll_interval());
        } else if (!strcmp(func, "fingerprints")) {
            out = w22_gatewaycon_d39075_fingerprints(json_obj_get(in, "registry"));
        } else if (!strcmp(func, "check")) {
            out = w22_gatewaycon_d39075_check(json_obj_get(in, "prev_registry"),
                                              json_obj_get(in, "prev_stamp"),
                                              json_obj_get(in, "new_stamp"),
                                              json_obj_get(in, "new_registry"));
        } else if (!strcmp(func, "teardown")) {
            out = w22_gatewaycon_d39075_teardown_plan(json_obj_get(in, "changed"),
                                                      json_obj_get(in, "removed"));
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
