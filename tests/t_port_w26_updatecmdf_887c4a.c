/* JSON-lines harness for the w26_updatecmdf_887c4a gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_updatecmdf_887c4a.h"

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
        if (!strcmp(func, "restart_timeout")) {
            out = w26_updatecmdf_887c4a_restart_timeout(in);
        } else if (!strcmp(func, "unit_job_budget")) {
            out = w26_updatecmdf_887c4a_unit_job_budget(in);
        } else if (!strcmp(func, "unit_job_state")) {
            out = w26_updatecmdf_887c4a_unit_job_state(in);
        } else if (!strcmp(func, "settled")) {
            out = w26_updatecmdf_887c4a_settled(json_obj_get(in, "state"));
        } else if (!strcmp(func, "service_restart_sec")) {
            out = w26_updatecmdf_887c4a_service_restart_sec(in);
        } else if (!strcmp(func, "constants")) {
            out = w26_updatecmdf_887c4a_constants();
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
