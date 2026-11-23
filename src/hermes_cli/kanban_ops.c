/*
 * kanban_ops.c — C home for upstream Hermes hermes_cli/kanban_ops.py.
 *
 * PoP: w8_kanbanops_540bdd_kanban_config @ hermes_cli/kanban_ops.py:_kanban_config
 * PoP: w8_kanbanops_540bdd_daemon_deprecated @ hermes_cli/kanban_ops.py:_cmd_daemon
 * PoP: w8_kanbanops_540bdd_gc_retention_error @ hermes_cli/kanban_ops.py:_cmd_gc
 *
 * DB/filesystem side effects are replaced by injected seams (`cfg`, `_err`
 * representation in the oracle, retention values). All exports are
 * w8_kanbanops_540bdd_ prefixed.
 */
#include "noc_w8_kanbanops_540bdd.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>

static int ko_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_len(v) > 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0;
    return 0;
}

json_t *w8_kanbanops_540bdd_kanban_config(const struct json_t *cfg) {
    if (!cfg || cfg->type != JSON_OBJECT) return json_object();
    json_t *kb = json_obj_get(cfg, "kanban");
    if (!ko_truthy(kb)) return json_object();
    return json_copy(kb);
}

/* Exact copy of _DAEMON_DEPRECATED (U+2014 = \xE2\x80\x94). */
static const char *ko_daemon_message(void) {
    return
        "hermes kanban daemon: DEPRECATED \xE2\x80\x94 the dispatcher now runs\n"
        "inside the gateway. To use kanban:\n\n"
        "    hermes gateway start       # starts the gateway + embedded dispatcher\n\n"
        "Ready tasks will be picked up on the next dispatcher tick\n"
        "(default: every 60 seconds). Configure via config.yaml:\n\n"
        "    kanban:\n"
        "      dispatch_in_gateway: true      # default\n"
        "      dispatch_interval_seconds: 60\n"
        "      failure_limit: 2              # consecutive non-success attempts before auto-block\n\n"
        "Running both the gateway AND this standalone daemon will\n"
        "race for claims. If you truly need the old standalone\n"
        "daemon (no gateway available), rerun with --force.";
}

static json_t *ko_err(int code, const char *msg) {
    json_t *o = json_object();
    json_set(o, "code", json_number(code));
    json_set(o, "message", json_string(msg));
    return o;
}

json_t *w8_kanbanops_540bdd_daemon_deprecated(int force) {
    if (force) return json_null();   /* --force takes the DB path (out of scope) */
    return ko_err(2, ko_daemon_message());
}

json_t *w8_kanbanops_540bdd_gc_retention_error(double event_days, double log_days) {
    if (event_days < 0 || log_days < 0)
        return ko_err(2, "kanban gc: retention days must be >= 0 (0 disables that sweep)");
    return json_null();
}
