/* JSON-lines harness for the w24_kanbanprac_15da5e gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_kanbanprac_15da5e.h"

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
        if (!strcmp(func, "decide")) {
            out = w24_kanbanprac_15da5e_decide(json_obj_get(in, "snapshot"),
                                               json_obj_get(in, "expected_run_id"),
                                               json_obj_get(in, "metadata"),
                                               json_obj_get(in, "assignee"));
        } else if (!strcmp(func, "pr_match")) {
            out = w24_kanbanprac_15da5e_pr_match(json_obj_get(in, "url"));
        } else if (!strcmp(func, "contract_ok")) {
            out = w24_kanbanprac_15da5e_contract_ok(json_obj_get(in, "value"));
        } else if (!strcmp(func, "record_outcome")) {
            out = w24_kanbanprac_15da5e_record_outcome(json_obj_get(in, "snapshot_matches"),
                                                       json_obj_get(in, "receipt"));
        } else if (!strcmp(func, "constants")) {
            out = w24_kanbanprac_15da5e_constants();
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
