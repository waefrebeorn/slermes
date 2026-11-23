#ifndef NOC_W8_KANBANOPS_540BDD_H
#define NOC_W8_KANBANOPS_540BDD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_kanbanops_540bdd_kanban_config @ hermes_cli/kanban_ops.py:_kanban_config
 * Explicit seam: `cfg` is the already-loaded config object. Returns
 * cfg.get("kanban", {}) or {} (or {} when cfg is not an object). */
struct json_t *w8_kanbanops_540bdd_kanban_config(const struct json_t *cfg);

/* PoP: w8_kanbanops_540bdd_daemon_deprecated @ hermes_cli/kanban_ops.py:_cmd_daemon
 * Non-force branch only. With the injected _err seam this returns
 * {"code":2,"message":_DAEMON_DEPRECATED}; a truthy force returns JSON null
 * (the DB path is out of scope for this deterministic gate). */
struct json_t *w8_kanbanops_540bdd_daemon_deprecated(int force);

/* PoP: w8_kanbanops_540bdd_gc_retention_error @ hermes_cli/kanban_ops.py:_cmd_gc
 * Validation branch only: returns the {"code":2,"message":...} object when
 * either retention-day value is negative, else JSON null. */
struct json_t *w8_kanbanops_540bdd_gc_retention_error(double event_days, double log_days);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_KANBANOPS_540BDD_H */
