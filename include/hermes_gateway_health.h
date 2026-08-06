#ifndef HERMES_GATEWAY_HEALTH_H
#define HERMES_GATEWAY_HEALTH_H

/*
 * hermes_gateway_health.h — Gateway health and diagnostics signal producer.
 * Port of agent/monitoring/gateway_health.py.
 * C11, no C++.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── GatewayMetric / GatewayHealthSnapshot data classes ───── */

typedef struct {
    const char *name;
    double value;         /* int | float in Python */
    json_t    *attributes; /* Dict[str, str] */
} gw_metric_t;

typedef struct {
    gw_metric_t *metrics;
    size_t       n_metrics;
    json_t      *events;    /* JSON array of GatewayHealthEvent | GatewayDiagnosticEvent */
} gw_health_snapshot_t;

/* ── Classification helpers ───────────────────────────────── */

/* PoP: source_logger_for_export @ agent/monitoring/gateway_health.py:source_logger_for_export */
/* Return a bounded source-controlled gateway logger name, or NULL if invalid. */
const char *gw_source_logger_for_export(const char *name);

/* PoP: redact_gateway_message @ agent/monitoring/gateway_health.py:redact_gateway_message */
/* Redact gateway diagnostic free text. Returns malloc'd string (caller free). */
char *gw_redact_gateway_message(const char *message);

/* PoP: classify_gateway_error @ agent/monitoring/gateway_health.py:classify_gateway_error */
/* Reduce free-form error text to a bounded operational class. */
const char *gw_classify_gateway_error(const char *raw);

/* PoP: classify_exit_reason @ agent/monitoring/gateway_health.py:classify_exit_reason */
/* Reduce free-form shutdown text to a bounded class. Returns string literal. */
const char *gw_classify_exit_reason(const char *raw, const char *state, bool restart_requested);

/* PoP: subsystem_for_logger @ agent/monitoring/gateway_health.py:subsystem_for_logger */
/* Map a logger name to a bounded subsystem label. */
const char *gw_subsystem_for_logger(const char *logger_name);

/* PoP: platform_for_subsystem @ agent/monitoring/gateway_health.py:platform_for_subsystem */
/* Extract platform name from a subsystem label, or NULL. */
const char *gw_platform_for_subsystem(const char *subsystem);

/* ── Gateway state helpers ────────────────────────────────── */

/* PoP: _bounded_state @ agent/monitoring/gateway_health.py:_bounded_state */
const char *gw_bounded_state(const char *raw);

/* PoP: _safe_instance_id @ agent/monitoring/gateway_health.py:_safe_instance_id */
/* Return a stable opaque instance key. Caller frees returned string. */
char *gw_safe_instance_id(const char *raw);

/* PoP: _parse_active_agents @ agent/monitoring/gateway_health.py:_parse_active_agents */
int gw_parse_active_agents(const char *raw);

/* PoP: _derive_busy @ agent/monitoring/gateway_health.py:_derive_busy */
bool gw_derive_busy(bool gateway_running, const char *gateway_state, int active_agents);

/* PoP: _derive_drainable @ agent/monitoring/gateway_health.py:_derive_drainable */
bool gw_derive_drainable(bool gateway_running, const char *gateway_state);

/* ── Snapshot builder ────────────────────────────────────── */

/* PoP: build_gateway_health_snapshot @ agent/monitoring/gateway_health.py:build_gateway_health_snapshot */
/* Convert runtime state into P0 signals. Returns malloc'd GatewayHealthSnapshot. */
gw_health_snapshot_t *gw_build_health_snapshot(
    const json_t *runtime,
    bool gateway_running,
    const char *profile,
    const char *install_id,
    const char *version,
    const char *supervision_mode
);

void gw_health_snapshot_free(gw_health_snapshot_t *s);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_HEALTH_H */