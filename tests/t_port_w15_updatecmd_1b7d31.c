#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_updatecmd_1b7d31.h"

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
        if (!strcmp(func, "is_serve_or_dashboard_unit")) {
            out = w15_updatecmd_1b7d31_is_serve_or_dashboard_unit(json_get_str(in, "unit", ""));
        } else if (!strcmp(func, "is_hermes_gateway_unit")) {
            out = w15_updatecmd_1b7d31_is_hermes_gateway_unit(json_get_str(in, "unit", ""));
        } else if (!strcmp(func, "receipt_looks_unfinished")) {
            out = w15_updatecmd_1b7d31_receipt_looks_unfinished(json_obj_get(in, "receipt"));
        } else if (!strcmp(func, "restart_phase_failure_is_incomplete")) {
            out = w15_updatecmd_1b7d31_restart_phase_failure_is_incomplete(
                json_obj_get(in, "surviving"), json_obj_get(in, "pre_restart_pids"));
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
