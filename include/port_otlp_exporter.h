/*
 * port_otlp_exporter.h — C11 port of agent/monitoring/otlp_exporter.py.
 *
 * Covers the deterministic helpers (config extraction, header resolution,
 * enable-check, span-attribute mapping, export_batch) AND the OTel SDK
 * seam (_require_sdk, build_exporter, _make_provider, OTLPStreamer,
 * start_streaming) — the latter reports unavailable in native builds.
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

/* ── OTel SDK seam (C has no OTel runtime; these mirror Python's lazy-import
 *    failure mode so start_streaming short-circuits exactly like Python would
 *    when `pip install hermes-agent[otlp]` is absent). ────────────── */

/* PoP: _require_sdk @ agent/monitoring/otlp_exporter.py:_require_sdk */
/* Port of Python: lazily "importing" the OTel SDK. In C there is no OTel SDK,
 * so this always reports unavailable — the same failure path Python hits when
 * the optional extra is absent. Returns NULL + sets *err_out (caller frees). */
bool otlp_exporter_require_sdk(char **err_out);

/* PoP: is_available @ agent/monitoring/otlp_exporter.py:is_available */
/* Pure check, no auto-install. In C the SDK is never present -> false. */
bool otlp_exporter_is_available(void);

/* PoP: is_enabled @ agent/monitoring/otlp_exporter.py:is_enabled  (decl below) */
bool otlp_exporter_is_enabled(const char *config_json);

/* PoP: _resource_attributes @ agent/monitoring/otlp_exporter.py:_resource_attributes */
/* Build the OTel resource attribute dict {service.name, service.instance.id,
 * telemetry.scope} from config + install id. Reuses gw_safe_instance_id and
 * the config monitoring.otlp section. malloc'd JSON string. */
char *otlp_exporter_resource_attributes(const char *config_json);

/* PoP: export_batch @ agent/monitoring/otlp_exporter.py:export_batch */
/* Map each event to span attributes via otlp_exporter_span_attrs and count
 * successful maps. (The C port has no live OTel tracer; this returns the
 * count of mappable events — the pure-logic core of the batch map.) */
int otlp_exporter_export_batch(const char *events_json_array);

/* PoP: build_exporter @ agent/monitoring/otlp_exporter.py:build_exporter */
/* PoP: _make_provider @ agent/monitoring/otlp_exporter.py:_make_provider */
/* SDK-wiring seams: no OTel SDK in native build, so always report
 * unavailable (start_streaming short-circuits). */
void *otlp_exporter_build_exporter(const char *config_json, char **err_out);
void *otlp_exporter_make_provider(const char *config_json);

/* PoP: __init__ @ agent/monitoring/otlp_exporter.py:OTLPStreamer.__init__ */
/* PoP: __call__ @ agent/monitoring/otlp_exporter.py:OTLPStreamer.__call__ */
/* PoP: shutdown @ agent/monitoring/otlp_exporter.py:OTLPStreamer.shutdown */
/* Opaque streamer that subscribes its exporter to the monitoring emitter. */
typedef struct otlp_streamer otlp_streamer_t;
otlp_streamer_t *otlp_streamer_new(const char *config_json);
void otlp_streamer_call(otlp_streamer_t *s, const char *events_json_array);
void otlp_streamer_shutdown(otlp_streamer_t *s);

/* PoP: start_streaming @ agent/monitoring/otlp_exporter.py:start_streaming */
/* If OTLP is enabled, attach a streamer to the singleton emitter. Returns the
 * streamer, or NULL when disabled/unavailable (mirrors Python returning None
 * when the SDK can't be made importable — never raises into startup). */
otlp_streamer_t *otlp_exporter_start_streaming(const char *config_json);

#ifdef __cplusplus
}
#endif

#endif /* PORT_OTLP_EXPORTER_H */
