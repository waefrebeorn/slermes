#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_deliveryle_311300.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "expected_profile")) {
            out = w19_deliveryle_311300_expected_profile(json_obj_get(in, "profile"));
        } else if (!strcmp(func, "sweep_recoverable")) {
            out = w19_deliveryle_311300_sweep_recoverable(json_obj_get(in, "rows"),
                                                           json_get_num(in, "now", 0.0),
                                                           json_obj_get(in, "platforms"),
                                                           json_obj_get(in, "targets"),
                                                           json_obj_get(in, "owner_pid"),
                                                           json_obj_get(in, "owner_started_at"),
                                                           json_obj_get(in, "dead_owners"));
        } else if (!strcmp(func, "sweep_runtime")) {
            out = w19_deliveryle_311300_sweep_runtime(json_obj_get(in, "rows"),
                                                      json_get_str(in, "platform", ""),
                                                      json_get_num(in, "now", 0.0),
                                                      json_obj_get(in, "profile"),
                                                      json_obj_get(in, "owner_pid"),
                                                      json_obj_get(in, "owner_started_at"),
                                                      json_obj_get(in, "due"));
        } else if (!strcmp(func, "runtime_candidates")) {
            out = w19_deliveryle_311300_runtime_candidates(json_obj_get(in, "rows"),
                                                           json_get_str(in, "platform", ""),
                                                           json_obj_get(in, "profile"),
                                                           json_obj_get(in, "owner_pid"),
                                                           json_obj_get(in, "owner_started_at"));
        } else if (!strcmp(func, "prune")) {
            out = w19_deliveryle_311300_prune(json_obj_get(in, "rows"), json_get_num(in, "now", 0.0));
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
