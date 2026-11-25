/* JSON-lines harness for the w21_retirement_867fec gate
 * (persistence/sqlite/housekeeping/_retirement.py).
 *
 * Each case hands the port a `state_meta` table as data plus the clock value,
 * and gets back the prune plan. Nothing here touches a database or a clock. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_retirement_867fec.h"
#include "persistence/sqlite/housekeeping/_retirement.c"

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
        if (!strcmp(func, "prune")) {
            json_t *ot = json_obj_get(in, "older_than");
            int has = (ot && ot->type == JSON_NUMBER) ? 1 : 0;
            out = w21_retirement_867fec_prune_plan(json_obj_get(in, "rows"),
                                                   json_get_num(in, "now", 0.0), has,
                                                   has ? ot->num_val : 0.0);
        } else if (!strcmp(func, "identity_key")) {
            char *k = w21_retirement_867fec_identity_key(json_obj_get(in, "principal"),
                                                         json_obj_get(in, "target"),
                                                         json_obj_get(in, "request"));
            out = k ? json_string(k) : json_null();
            free(k);
        } else if (!strcmp(func, "constants")) {
            out = w21_retirement_867fec_constants();
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
