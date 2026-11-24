/* JSON-lines harness for the w21_updatecmdf_e182aa gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * an absent `alive` reads as "could not tell", and absent rows as empty sets.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_updatecmdf_e182aa.h"
#include "hermes_cli/update_cmd_fleet_gatewayless.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "runtime_outside_gateway_evidence")) {
            out = w21_updatecmdf_e182aa_runtime_outside_gateway_evidence(json_obj_get(in, "runtime"),
                                                                         json_obj_get(in, "alive"));
        } else if (!strcmp(func, "host_owes_no_gateway_restart")) {
            out = w21_updatecmdf_e182aa_host_owes_no_gateway_restart(json_obj_get(in, "records"),
                                                                     json_obj_get(in, "runtimes"),
                                                                     json_obj_get(in, "alive_map"));
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
