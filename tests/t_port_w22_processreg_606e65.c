/* JSON-lines harness for the w22_processreg_606e65 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_processreg_606e65.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *tri(int v) { return json_number((double)v); }

/* 64-bit values travel as decimal strings. */
static json_t *ll_str(long long v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", v);
    return json_string(buf);
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
        if (!strcmp(func, "fields")) {
            out = w22_processreg_606e65_field_names();
        } else if (!strcmp(func, "constants")) {
            out = json_object();
            json_set(out, "retention_seconds",
                     ll_str(w22_processreg_606e65_retention_seconds()));
            json_set(out, "max_retained",
                     ll_str(w22_processreg_606e65_max_retained()));
        } else if (!strcmp(func, "select")) {
            out = w22_processreg_606e65_select_retained(json_obj_get(in, "rows"),
                                                        json_get_str(in, "now", NULL));
        } else if (!strcmp(func, "owns")) {
            out = tri(w22_processreg_606e65_owns_result(json_get_str(in, "owner", NULL),
                                                        json_get_str(in, "parent", NULL),
                                                        json_obj_get(in, "lineage")));
        } else if (!strcmp(func, "admissible")) {
            out = tri(w22_processreg_606e65_admissible(json_get_str(in, "prefix", NULL),
                                                       json_get_str(in, "stem", NULL),
                                                       json_get_str(in, "record_id", NULL),
                                                       json_get_str(in, "owner", NULL),
                                                       json_get_str(in, "parent", NULL),
                                                       json_obj_get(in, "lineage")));
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
