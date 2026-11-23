/* Port gate harness for agent/route_verdicts.py (gate w7_routeverdic_40e4a2). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_routeverdic_40e4a2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "route_visit")) {
            out = json_number((double)w7_routeverdic_40e4a2_route_visit(json_obj_get(in, "visit")));
        } else if (!strcmp(func, "snapshot")) {
            out = w7_routeverdic_40e4a2_snapshot(json_obj_get(in, "agent"));
        } else if (!strcmp(func, "apply")) {
            out = w7_routeverdic_40e4a2_apply(json_obj_get(in, "agent"), json_obj_get(in, "values"));
        } else if (!strcmp(func, "reset")) {
            out = w7_routeverdic_40e4a2_reset(json_obj_get(in, "agent"),
                                              json_get_bool(json_obj_get(in, "agent"), "_disable_streaming_seed", false));
        } else if (!strcmp(func, "restore_primary")) {
            out = w7_routeverdic_40e4a2_restore_primary(json_obj_get(in, "rt"),
                                                        json_get_str(in, "endpoint", ""),
                                                        json_obj_get(in, "agent"));
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
