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

/* --- Constants shared with the Python original --- */
#define HE_DEFAULT_DIAGNOSTIC_SCOPE "hermes.gateway.diagnostics"
#define HE_REDACTION_LIMIT 500

#ifdef __cplusplus
}
#endif

#endif /* PORT_AGENT_MONITORING_HEALTH_EXPORT_H */
