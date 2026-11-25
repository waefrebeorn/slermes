/* JSON-lines harness for the w34_reconcile_7686f7_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_reconcile_7686f7_p2.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "history_tables")) {
            out = w34_reconcile_7686f7_p2_history_tables(json_obj_get(in, "names"));
        } else if (!strcmp(func, "integrity_problem")) {
            out = w34_reconcile_7686f7_p2_integrity_problem(json_obj_get(in, "rows"));
        } else if (!strcmp(func, "budget")) {
            out = w34_reconcile_7686f7_p2_budget(json_get_num(in, "deadline", 0.0),
                                                 json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "budget_check")) {
            out = w34_reconcile_7686f7_p2_budget_check(
                json_get_bool(in, "expired", 0) ? 1 : 0, json_get_num(in, "remaining", 0.0));
        } else if (!strcmp(func, "constants")) {
            out = w34_reconcile_7686f7_p2_constants();
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
