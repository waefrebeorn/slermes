/* JSON-lines harness for the w26_processreg_e923be gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_processreg_e923be.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "owner")) {
            out = w26_processreg_e923be_owner(json_obj_get(in, "evt"));
        } else if (!strcmp(func, "notifies")) {
            out = w26_processreg_e923be_notifies(json_obj_get(in, "session"));
        } else if (!strcmp(func, "retained")) {
            out = w26_processreg_e923be_retained(json_obj_get(in, "session"));
        } else if (!strcmp(func, "adjust")) {
            out = w26_processreg_e923be_adjust(json_obj_get(in, "in_flight"),
                                               json_obj_get(in, "owners"),
                                               json_obj_get(in, "delta"));
        } else if (!strcmp(func, "awaits")) {
            out = w26_processreg_e923be_awaits(json_obj_get(in, "state"));
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
