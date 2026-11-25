/* JSON-lines harness for the w34_reconcile_7686f7_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_reconcile_7686f7_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_at(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "json_file")) {
            out = w34_reconcile_7686f7_p1_json_file(str_at(in, "raw"),
                                                    json_get_bool(in, "bare_pid_ok", 0) ? 1 : 0);
        } else if (!strcmp(func, "same_home")) {
            out = w34_reconcile_7686f7_p1_same_home(str_at(in, "left"), str_at(in, "right"));
        } else if (!strcmp(func, "profile_label")) {
            out = w34_reconcile_7686f7_p1_profile_label(str_at(in, "path"));
        } else if (!strcmp(func, "repair_command")) {
            out = w34_reconcile_7686f7_p1_repair_command(str_at(in, "home"));
        } else if (!strcmp(func, "owner_remedy")) {
            out = w34_reconcile_7686f7_p1_owner_remedy(str_at(in, "home"));
        } else if (!strcmp(func, "owning_gateway_restart")) {
            out = w34_reconcile_7686f7_p1_owning_gateway_restart(str_at(in, "home"),
                                                                 str_at(in, "lock_text"));
        } else if (!strcmp(func, "lagging_damaged")) {
            out = w34_reconcile_7686f7_p1_lagging_damaged(str_at(in, "detail"));
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
