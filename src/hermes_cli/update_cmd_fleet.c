/*
 * update_cmd_fleet.c — C home for upstream Hermes hermes_cli/update_cmd_fleet.py.
 *
 * PoP: w15_updatecmd_1b7d31_is_serve_or_dashboard_unit @ hermes_cli/update_cmd_fleet.py:_is_serve_or_dashboard_unit
 * PoP: w15_updatecmd_1b7d31_is_hermes_gateway_unit @ hermes_cli/update_cmd_fleet.py:_is_hermes_gateway_unit
 * PoP: w15_updatecmd_1b7d31_receipt_looks_unfinished @ hermes_cli/update_cmd_fleet.py:_receipt_looks_unfinished
 * PoP: w15_updatecmd_1b7d31_restart_phase_failure_is_incomplete @ hermes_cli/update_cmd_fleet.py:_restart_phase_failure_is_incomplete
 */
#include "hermes_json.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w15_updatecmd_1b7d31.h"

static char *dupstr(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}

static int is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

json_t *w15_updatecmd_1b7d31_is_serve_or_dashboard_unit(const char *unit) {
    const char *u = unit ? unit : "";
    int ok = !strcmp(u, "hermes-serve.service")
        || !strncmp(u, "hermes-serve-", 13)
        || !strcmp(u, "hermes-dashboard.service")
        || !strncmp(u, "hermes-dashboard-", 17);
    return json_bool(ok);
}

json_t *w15_updatecmd_1b7d31_is_hermes_gateway_unit(const char *unit) {
    const char *u = unit ? unit : "";
    int ok = !strcmp(u, "hermes-gateway.service")
        || !strncmp(u, "hermes-gateway-", 15);
    if (!ok) {
        json_t *inner = w15_updatecmd_1b7d31_is_serve_or_dashboard_unit(u);
        ok = inner && inner->bool_val;
        json_free(inner);
    }
    return json_bool(ok);
}

json_t *w15_updatecmd_1b7d31_receipt_looks_unfinished(const json_t *receipt) {
    const json_t *obj = (receipt && receipt->type == JSON_OBJECT) ? receipt : NULL;
    const json_t *ec = obj ? json_obj_get(obj, "exit_code") : NULL;
    int ec_zero_or_none = is_none(ec) || (ec->type == JSON_NUMBER && ec->num_val == 0.0);
    const json_t *outcome = obj ? json_obj_get(obj, "outcome") : NULL;
    const char *outs = (outcome && outcome->type == JSON_STRING && outcome->str_val) ? outcome->str_val : "";
    int unfinished = !ec_zero_or_none
        || !strcmp(outs, "failed") || !strcmp(outs, "partial") || !strcmp(outs, "running");
    if (unfinished) return json_bool(true);
    const json_t *gr = obj ? json_obj_get(obj, "gateway_restart") : NULL;
    if (gr && gr->type == JSON_OBJECT && truthy(json_obj_get(gr, "incomplete"))) return json_bool(true);
    int succeeded = (ec && ec->type == JSON_NUMBER && ec->num_val == 0.0) || !strcmp(outs, "success");
    const json_t *sr = obj ? json_obj_get(obj, "stop_reason") : NULL;
    return json_bool(truthy(sr) && !succeeded);
}

json_t *w15_updatecmd_1b7d31_restart_phase_failure_is_incomplete(const json_t *surviving,
                                                                const json_t *pre_restart_pids) {
    if (is_none(surviving) || truthy(surviving)) return json_bool(true);
    return json_bool(is_none(pre_restart_pids) || truthy(pre_restart_pids));
}

void hermes_cli_update_cmd_fleet_port_pending(void) { (void)0; }
