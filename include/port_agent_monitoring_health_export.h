/*
 * port_agent_monitoring_health_export.h — C11 port of pure helpers
 * from agent/monitoring/gateway_health_export.py.
 *
 * Ports the deterministic, I/O-free helpers of the Gateway Health &
 * Diagnostics OTLP export runtime: config-section extraction,
 * endpoint derivation, severity mapping, and diagnostic-attribute
 * shaping. The OTLP SDK wiring, snapshot threads, and log streaming
 * remain in Python; this header covers the pure logic only.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. Dict-returning functions take/return JSON object
 * strings via libjson.
 */

#ifndef PORT_AGENT_MONITORING_HEALTH_EXPORT_H
#define PORT_AGENT_MONITORING_HEALTH_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_t json_t;

/* PoP: _gateway_health_config @ agent/monitoring/gateway_health_export.py:_gateway_health_config */
/* config.monitoring.gateway_health_export section, as a JSON object
 * string ("{}" when absent). input: full config JSON. malloc'd. */
char *he_gateway_health_config(const char *config_json);

/* PoP: _otlp_config @ agent/monitoring/gateway_health_export.py:_otlp_config */
/* config.monitoring.export.otlp section, as a JSON object string
 * ("{}" when absent). input: full config JSON. malloc'd. */
char *he_otlp_config(const char *config_json);

/* PoP: _enabled @ agent/monitoring/gateway_health_export.py:_enabled */
/* True when gateway_health_export.enabled AND export.otlp.enabled AND
 * export.otlp.endpoint are all set. input: full config JSON. */
bool he_enabled(const char *config_json);

/* PoP: _metric_endpoint @ agent/monitoring/gateway_health_export.py:_metric_endpoint */
/* endpoint with trailing "/v1/traces" replaced by "/v1/metrics". */
char *he_metric_endpoint(const char *endpoint);

/* PoP: _logs_endpoint @ agent/monitoring/gateway_health_export.py:_logs_endpoint */
/* endpoint with trailing "/v1/traces" or "/v1/metrics" replaced by
 * "/v1/logs"; unchanged otherwise. */
char *he_logs_endpoint(const char *endpoint);

/* PoP: _resolve_headers @ agent/monitoring/gateway_health_export.py:_resolve_headers
 * Resolve headers from env-var-name mapping. input: JSON object; output: JSON
 * object with header names whose ENV_VAR resolves to a non-empty string. malloc'd. */
char *he_resolve_headers(const char *headers_env_json);

/* PoP: _severity_number @ agent/monitoring/gateway_health_export.py:_severity_number */
/* OTel severity number: critical/fatal->21(FATAL), error->17(ERROR),
 * info/information->9(INFO), debug->5(DEBUG), else 13(WARN).
 * Returns the int value. */
int he_severity_number(const char *severity);

/* PoP: _diagnostic_log_attributes @ agent/monitoring/gateway_health_export.py:_diagnostic_log_attributes */
/* Shape an event dict into "hermes.<key>" attributes, allowlisted by
 * _DIAGNOSTIC_ATTRIBUTE_KEYS, skipping None values. Non-string values
 * pass through (numbers/bools); string values are capped at 500 chars.
 * input: event JSON object; output: attrs JSON object string. malloc'd. */
char *he_diagnostic_log_attributes(const char *event_json);

/* PoP: _read_gateway_snapshot @ agent/monitoring/gateway_health_export.py:_read_gateway_snapshot */
/* Build a gateway health snapshot from config. Returns JSON string (malloc'd) or NULL. */
char *he_read_gateway_snapshot(const char *config_json);

/* PoP: _read_cron_snapshot @ agent/monitoring/gateway_health_export.py:_read_cron_snapshot */
/* Build a cron health snapshot. Returns JSON string (malloc'd) or NULL. */
char *he_read_cron_snapshot(void);

/* PoP: _read_background_work_count @ agent/monitoring/gateway_health_export.py:_read_background_work_count */
/* Count live background/subagent work (delegation tasks + running processes). */
long he_read_background_work_count(void);

/* PoP: _read_background_delegations_count @ agent/monitoring/gateway_health_export.py:_read_background_delegations_count */
/* Count live async delegation UNITS (dispatch/pool slots). */
long he_read_background_delegations_count(void);

/* PoP: _read_runtime_snapshot @ agent/monitoring/gateway_health_export.py:_read_runtime_snapshot */
/* Build a full runtime snapshot (gateway + background + cron metrics). Returns JSON. */
char *he_read_runtime_snapshot(const char *config_json);

/* PoP: _emit_snapshot_events @ agent/monitoring/gateway_health_export.py:_emit_snapshot_events */
/* Emit diagnostic events from a runtime snapshot. Best-effort. */
void he_emit_snapshot_events(const char *config_json);

/* PoP: _gateway_health_event @ agent/monitoring/gateway_health_export.py:_gateway_health_event */
bool he_gateway_health_event(const char *event_json);

/* PoP: _require_metrics_sdk @ agent/monitoring/gateway_health_export.py:_require_metrics_sdk */
bool he_require_metrics_sdk(const char *config_json);

/* PoP: _start_metric_provider @ agent/monitoring/gateway_health_export.py:_start_metric_provider */
void *he_start_metric_provider(const char *config_json);

/* PoP: start_gateway_health_export @ agent/monitoring/gateway_health_export.py:start_gateway_health_export */
/* Start P0 gateway health export if configured. Returns a JSON runtime descriptor. Never raises. */
char *he_start_gateway_health_export(const char *config_json);

/* --- GatewayHealthExportRuntime class (port of GatewayHealthExportRuntime dataclass) --- */
typedef struct he_runtime {
    bool enabled;
    char *reason;
    void *streamer;       /* opaque OTLP streamer handle */
    void *metric_provider; /* opaque OTLP metric provider */
    void *log_handler;    /* opaque log handler */
    void *log_streamer;   /* opaque diagnostic log streamer */
    void *thread;         /* opaque pthread handle */
    void *stop_event;     /* opaque stop-event handle */
} he_runtime_t;

/* PoP: GatewayHealthExportRuntime.shutdown @ agent/monitoring/gateway_health_export.py:GatewayHealthExportRuntime.shutdown */
void he_runtime_shutdown(he_runtime_t *runtime);

/* PoP: GatewayDiagnosticLogStreamer.__init__ @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.__init__ */
void *he_log_streamer_init(const char *config_json, void *sdk);

/* PoP: GatewayDiagnosticLogStreamer.__call__ @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.__call__ */
void he_log_streamer_call(void *streamer, const char *batch_json);

/* PoP: GatewayDiagnosticLogStreamer.shutdown @ agent/monitoring/gateway_health_export.py:GatewayDiagnosticLogStreamer.shutdown */
void he_log_streamer_shutdown(void *streamer);

/* PoP: _start_diagnostic_log_streamer @ agent/monitoring/gateway_health_export.py:_start_diagnostic_log_streamer */
void *he_start_diagnostic_log_streamer(const char *config_json, void *sdk);

/* PoP: _start_snapshot_thread @ agent/monitoring/gateway_health_export.py:_start_snapshot_thread */
void *he_start_snapshot_thread(const char *config_json, void *stop_event);

/* PoP: _attach_log_handler @ agent/monitoring/gateway_health_export.py:_attach_log_handler */
void *he_attach_log_handler(const char *config_json);

/* --- Constants shared with the Python original --- */
#define HE_DEFAULT_DIAGNOSTIC_SCOPE "hermes.gateway.diagnostics"
#define HE_REDACTION_LIMIT 500

#ifdef __cplusplus
}
#endif

#endif /* PORT_AGENT_MONITORING_HEALTH_EXPORT_H */
