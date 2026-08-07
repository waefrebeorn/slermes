/*
 * port_otlp_exporter.h — pure helpers from agent/monitoring/otlp_exporter.py.
 *
 * Faithful port of the deterministic, I/O-free helpers. The OTel SDK wiring
 * (_require_sdk, build_exporter, _make_provider, OTLPStreamer) remains in
 * Python; this covers: config-section extraction, header resolution,
 * enable-check, and event→span-attribute mapping.
 */

#ifndef PORT_OTLP_EXPORTER_H
#define PORT_OTLP_EXPORTER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _otlp_config @ agent/monitoring/otlp_exporter.py:_otlp_config */
/* config.monitoring.export.otlp section as JSON object string ("{}" when
 * absent). input: full config JSON. malloc'd, caller frees. */
char *otlp_exporter_otlp_config(const char *config_json);

/* PoP: _resolve_headers @ agent/monitoring/otlp_exporter.py:_resolve_headers */
/* Resolve {header: ENV_VAR} -> {header: value} from environment. input:
 * JSON object; output: JSON object with headers whose ENV_VAR is set. malloc'd. */
char *otlp_exporter_resolve_headers(const char *headers_env_json);

/* PoP: is_enabled @ agent/monitoring/otlp_exporter.py:is_enabled */
/* True when export.otlp.enabled AND export.otlp.endpoint are both set. */
bool otlp_exporter_is_enabled(const char *config_json);

/* PoP: _span_attrs @ agent/monitoring/otlp_exporter.py:_span_attrs */
/* Map a monitoring event JSON object to span attributes. Strings are passed
 * through the redaction layer and capped at 500 chars. malloc'd JSON string. */
char *otlp_exporter_span_attrs(const char *event_json);

/* Internal: redact_for_export — hermes_redact_force + regex passes. */
char *otlp_exporter_redact_for_export(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* PORT_OTLP_EXPORTER_H */
