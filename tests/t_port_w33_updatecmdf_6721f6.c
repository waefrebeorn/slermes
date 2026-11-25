/* JSON-lines harness for the w33_updatecmdf_6721f6 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w33_updatecmdf_6721f6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "fleet_probe_expected")) {
        return w33_updatecmdf_6721f6_fleet_probe_expected(
            json_obj_get(in, "plan_runtimes"), json_obj_get(in, "pre_restart_pids"),
            json_obj_get(in, "restarted_services"), json_obj_get(in, "killed_pids"));
    }
    if (!strcmp(func, "identity_pending")) {
        return w33_updatecmdf_6721f6_identity_pending(
            json_obj_get(in, "row"), json_obj_get(in, "pre_restart_pids"));
    }
    if (!strcmp(func, "live_gateway_pids")) {
        return w33_updatecmdf_6721f6_live_gateway_pids(json_obj_get(in, "rows"));
    }
    if (!strcmp(func, "is_serve_unit")) {
        return w33_updatecmdf_6721f6_is_serve_unit(json_obj_get(in, "name"));
    }
    if (!strcmp(func, "restart_phase_owes_gateway")) {
        return w33_updatecmdf_6721f6_restart_phase_owes_gateway(json_obj_get(in, "restart"));
    }
    if (!strcmp(func, "split_unaccounted")) {
        return w33_updatecmdf_6721f6_split_unaccounted(json_obj_get(in, "outcomes"));
    }
    if (!strcmp(func, "owed_restart_reason")) {
        return w33_updatecmdf_6721f6_owed_restart_reason(
            json_obj_get(in, "restart"), json_obj_get(in, "stopped"),
            json_obj_get(in, "gateway_owed"), json_obj_get(in, "stale_serves"),
            json_obj_get(in, "down_serves"));
    }
    if (!strcmp(func, "row_is_external")) {
        return w33_updatecmdf_6721f6_row_is_external(json_obj_get(in, "row"));
    }
    if (!strcmp(func, "restarted_units_gone")) {
        return w33_updatecmdf_6721f6_restarted_units_gone(
            json_obj_get(in, "units"), json_obj_get(in, "responses"));
    }
    if (!strcmp(func, "gateway_unit_runs_one_of")) {
        return w33_updatecmdf_6721f6_gateway_unit_runs_one_of(
            json_obj_get(in, "unit"), json_obj_get(in, "pids"), json_obj_get(in, "responses"));
    }
    if (!strcmp(func, "constants")) return w33_updatecmdf_6721f6_constants();
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(in, func);
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
