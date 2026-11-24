/*
 * update_cmd_fleet_gatewayless.c -- real C11 port of
 * hermes_cli/update_cmd_fleet_gatewayless.py (gate w21_updatecmdf_e182aa):
 * gateway-less host evidence for the update-restart obligation.
 *
 * PoP: w21_updatecmdf_e182aa_runtime_outside_gateway_evidence @ hermes_cli/update_cmd_fleet_gatewayless.py:runtime_outside_gateway_evidence
 * PoP: w21_updatecmdf_e182aa_host_owes_no_gateway_restart @ hermes_cli/update_cmd_fleet_gatewayless.py:host_owes_no_gateway_restart
 *
 * The two readers ask the live host directly. Three surfaces stay upstream and
 * reach the port as values:
 *   - `hermes_cli.update_serve_obligations.defer_manual_serve` is ported whole
 *     (`defer_manual_serve` below); only the liveness probe it ends in,
 *     `process_identity._pid_alive_matches`, is a seam -- a clock and process
 *     table read -- answered by the `alive` tri-state;
 *   - `gateway.status.read_runtime_status` per profile home is the filesystem
 *     seam, answered by the `records` array;
 *   - `hermes_cli.update_inventory.collect_runtime_inventory` is the inventory
 *     seam, answered by the `runtimes` rows.
 */
#include "hermes_json.h"
#include "noc_w21_updatecmdf_e182aa.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* update_cmd_fleet._SUPERVISOR_OWNED_SERVE_BACKENDS:
 * _SUPERVISED_SERVE_BACKENDS - {"manual-serve"} */
static const char *const UCF182_SUPERVISOR_OWNED[] = {
    "desktop", "desktop-ssh", "systemd", "launchd", "windows-service", "service", NULL
};

/* update_receipt._NOT_EXPECTED_STATES */
static const char *const UCF182_NOT_EXPECTED[] = {"stopped", "startup_failed", NULL};

/* Python truthiness for a decoded JSON value; NULL (absent) reads as None. */
static int ucf182_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* `runtime.get(key) == value` for a string field. */
static int ucf182_field_is(const json_t *obj, const char *key, const char *value) {
    const char *s;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    s = json_get_str(obj, key, NULL);
    return s && !strcmp(s, value);
}

/* `type(pid) is int` -- a JSON bool is not an int -- and `pid > 0`. */
static int ucf182_py_int(const json_t *v, long long *out) {
    double d;
    if (!v || v->type != JSON_NUMBER) return 0;
    d = v->num_val;
    if (!isfinite(d) || floor(d) != d) return 0;
    *out = (long long)d;
    return 1;
}

/* Liveness as `_pid_alive_matches` answers it: 1 true, 0 false, -1 unknown. */
static int ucf182_alive_state(const json_t *alive) {
    if (!alive || alive->type == JSON_NULL) return -1;
    if (alive->type == JSON_BOOL) return alive->bool_val ? 1 : 0;
    return -1;
}

/* defer_manual_serve(runtime, require_alive=False) from
 * hermes_cli/update_serve_obligations.py, with `_write_reminder` (a durable
 * file write) folded into the True it leads to. */
static int ucf182_defer_manual_serve(const json_t *runtime, int alive) {
    json_t *detail, *pidv, *created;
    long long pid;
    int identified;
    if (!runtime || runtime->type != JSON_OBJECT) return 0;
    if (!ucf182_field_is(runtime, "kind", "serve") && !ucf182_field_is(runtime, "kind", "dashboard")) return 0;
    if (!ucf182_field_is(runtime, "supervisor", "manual-serve")) return 0;
    if (!ucf182_field_is(runtime, "restart_via", "respawn-argv")) return 0;
    detail = json_obj_get(runtime, "detail");
    if (!detail || detail->type != JSON_OBJECT) return 0;
    pidv = json_obj_get(runtime, "pid");
    if (!ucf182_py_int(pidv, &pid) || pid <= 0) return 0;
    created = json_obj_get(detail, "create_time");
    identified = created && created->type == JSON_NUMBER && isfinite(created->num_val) && created->num_val > 0;
    if (!identified) {
        /* no recorded creation time: only a provably dead pid discharges it */
        return alive == 0;
    }
    /* require_alive is False here, so anything but "dead" files the reminder */
    return 1;
}

json_t *w21_updatecmdf_e182aa_runtime_outside_gateway_evidence(const json_t *runtime,
                                                               const json_t *alive) {
    int supervised = 0;
    int i;
    if (!runtime || runtime->type != JSON_OBJECT) return json_bool(0);
    if (!ucf182_field_is(runtime, "kind", "serve") && !ucf182_field_is(runtime, "kind", "dashboard"))
        return json_bool(0);
    if (ucf182_defer_manual_serve(runtime, ucf182_alive_state(alive))) return json_bool(1);
    {
        const char *sup = json_get_str(runtime, "supervisor", NULL);
        if (sup) {
            for (i = 0; UCF182_SUPERVISOR_OWNED[i]; i++) {
                if (!strcmp(sup, UCF182_SUPERVISOR_OWNED[i])) { supervised = 1; break; }
            }
        }
    }
    return json_bool(supervised);
}

/* record.get("gateway_state") if isinstance(record, dict) else None */
static const char *ucf182_record_state(const json_t *record) {
    if (!record || record->type != JSON_OBJECT) return NULL;
    return json_get_str(record, "gateway_state", NULL);
}

json_t *w21_updatecmdf_e182aa_host_owes_no_gateway_restart(const json_t *records,
                                                           const json_t *runtimes,
                                                           const json_t *alive_map) {
    size_t i, n;
    if (records && records->type == JSON_ARRAY) {
        n = records->c.count;
        for (i = 0; i < n; i++) {
            const json_t *record = records->c.items[i];
            const char *state;
            int expected = 0, k;
            if (!record || record->type == JSON_NULL) continue;   /* record is None */
            state = ucf182_record_state(record);
            if (state) {
                for (k = 0; UCF182_NOT_EXPECTED[k]; k++) {
                    if (!strcmp(state, UCF182_NOT_EXPECTED[k])) { expected = 1; break; }
                }
            }
            if (!expected) return json_bool(0);
        }
    }
    if (!runtimes || runtimes->type != JSON_ARRAY) return json_bool(1);
    n = runtimes->c.count;
    for (i = 0; i < n; i++) {
        const json_t *runtime = runtimes->c.items[i];
        const json_t *alive = NULL;
        char key[32];
        if (alive_map && alive_map->type == JSON_OBJECT) {
            json_t *pidv = json_obj_get(runtime, "pid");
            long long pid = 0;
            if (ucf182_py_int(pidv, &pid)) {
                snprintf(key, sizeof(key), "%lld", pid);
                alive = json_obj_get(alive_map, key);
            }
            if (!alive && pidv && pidv->type == JSON_STRING && pidv->str_val)
                alive = json_obj_get(alive_map, pidv->str_val);
        }
        {
            json_t *verdict = w21_updatecmdf_e182aa_runtime_outside_gateway_evidence(runtime, alive);
            int outside = verdict && verdict->type == JSON_BOOL && verdict->bool_val;
            json_free(verdict);
            if (!outside) return json_bool(0);
        }
    }
    return json_bool(1);
}
