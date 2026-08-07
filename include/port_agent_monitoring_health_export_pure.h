/*
 * port_agent_monitoring_health_export_pure.h
 * Pure helpers from agent/monitoring/gateway_health_export.py.
 */
#ifndef PORT_AGENT_MONITORING_HEALTH_EXPORT_PURE_H
#define PORT_AGENT_MONITORING_HEALTH_EXPORT_PURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _redact_string @ agent/monitoring/gateway_health_export.py:_redact_string */
/* redact_for_export with fail-open. Truncated to limit (default 500). */
char *ghe_redact_string(const char *raw, int limit);

/* PoP: _safe_resource_attributes @ agent/monitoring/gateway_health_export.py:_safe_resource_attributes */
/* Allowlist resource keys, reject values that change under redaction.
 * raw_json: JSON object string. Returns malloc'd JSON object string. */
char *ghe_safe_resource_attributes(const char *raw_json);

/* PoP: _version @ agent/monitoring/gateway_health_export.py:_version */
const char *ghe_version(void);

/* PoP: _profile @ agent/monitoring/gateway_health_export.py:_profile */
const char *ghe_profile(void);

/* PoP: _install_id @ agent/monitoring/gateway_health_export.py:_install_id */
/* config_json: JSON config dict. Returns malloc'd string ("unknown" if not found).
 * Caller frees. */
char *ghe_install_id(const char *config_json);

/* PoP: _supervision_mode @ agent/monitoring/gateway_health_export.py:_supervision_mode */
const char *ghe_supervision_mode(void);

/* PoP: _runtime_resource_attributes @ agent/monitoring/gateway_health_export.py:_runtime_resource_attributes */
/* Build the safe OTLP resource dict. config_json + telemetry_scope in.
 * Returns malloc'd JSON object string. */
char *ghe_runtime_resource_attributes(const char *config_json, const char *telemetry_scope);

#ifdef __cplusplus
}
#endif
#endif
