/*
 * gateway_run_pure.h — opaque-struct-free declaration surface for the pure
 * gateway/run.py helpers ported in src/gateway/run_pure.c.
 *
 * These are deterministic string/data transforms with NO network, config-IO,
 * or async coupling. They are oracle-verified against the canonical Python.
 */

#ifndef GATEWAY_RUN_PURE_H
#define GATEWAY_RUN_PURE_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"   /* json_node_t */

#ifdef __cplusplus
extern "C" {
#endif

/* run.py _gateway_platform_value — normalize a platform string.
 * `platform` is taken as a C string here (enums should be pre-normalized).
 * Caller must free. */
char *gateway_platform_value(const void *platform);

/* run.py _gateway_surface_passes_raw_text */
bool gateway_surface_passes_raw_text(const char *platform);

/* run.py _non_conversational_metadata — returns a NEW object (caller frees).
 * When platform != discord, returns the input object unchanged (no free). */
json_node_t *gateway_non_conversational_metadata(json_node_t *metadata,
                                                  const char *platform);

/* run.py _looks_like_gateway_provider_error (regex-faithful variant;
 * the substring-based gateway_looks_like_provider_error in helpers.c is a
 * lower-fidelity port and is NOT used here). */
bool gateway_looks_like_provider_error_regex(const char *text);

/* run.py _gateway_provider_error_reply (regex-faithful variant; see above). */
char *gateway_provider_error_reply_regex(const char *text);

/* run.py _is_auto_continue_noise */
bool gateway_is_auto_continue_noise(const char *content);

/* run.py _strip_auto_continue_noise — returns malloc'd stripped string. */
char *gateway_strip_auto_continue_noise(const char *content);

/* run.py _telegramize_command_mentions — returns malloc'd rewritten string.
 * Non-telegram platforms return an unchanged copy. */
char *gateway_telegramize_command_mentions(const char *text,
                                            const char *platform);

/* run.py _coerce_gateway_timestamp — returns malloc'd epoch-seconds string
 * ("%.3f") or NULL when unparseable. */
char *gateway_coerce_timestamp(const char *value);

/* run.py _message_timestamps_enabled(user_config dict) */
bool gateway_message_timestamps_enabled(json_node_t *user_config);

/* run.py _is_transient_network_error(exc_name, cause_name, context_name). */
bool gateway_is_transient_network_error(const char *exc_name,
                                         const char *cause_name,
                                         const char *context_name);

/* run.py _event_media_type_at - per-attachment MIME type lookup */
const char *gateway_event_media_type_at(const json_node_t *event, int index);

/* run.py _event_media_is_image - True if attachment at index is an image */
bool gateway_event_media_is_image(const json_node_t *event, int index);

/* run.py _event_media_is_audio - True if attachment at index is audio */
bool gateway_event_media_is_audio(const json_node_t *event, int index);

/* run.py _event_media_is_video - True if attachment at index is video */
bool gateway_event_media_is_video(const json_node_t *event, int index);

/* run.py _build_media_placeholder - text placeholder for media events */
char *gateway_build_media_placeholder(const char *media_urls_json,
                                       const char *media_types_json,
                                       const char *message_type);

/* run.py _build_document_context_note - context note for document attachments */
char *gateway_build_document_context_note(const char *display_name,
                                           const char *agent_path,
                                           const char *mime_type);

/* ===========================================================================
 *  web_server.py pure helpers ported from hermes_cli/web_server.py
 * =========================================================================== */

/* PoP: _tail_lines @ hermes_cli/web_server.py:_tail_lines
 * Return malloc'd string with last n lines of file at path. */
char *web_tail_lines(const char *path, int n);

/* PoP: _dashboard_spawn_executable @ hermes_cli/web_server.py:_dashboard_spawn_executable
 * Returns malloc'd string - pythonw.exe on Windows, sys.executable otherwise. */
char *web_dashboard_spawn_executable(void);

/* PoP: _record_completed_action @ hermes_cli/web_server.py:_record_completed_action
 * Simple stub - the full action tracking requires subprocess management. */
void web_record_completed_action(const char *name, int exit_code, const char *message);

/* PoP: _normalize_config_for_web @ hermes_cli/web_server.py:_normalize_config_for_web
 * Normalize config for web UI: flatten model dict to string, extract context_length.
 * Input and output are JSON objects (caller frees). */
json_node_t *web_normalize_config_for_web(json_node_t *config);

/* run.py _resolve_gateway_display_bool */
bool gateway_resolve_gateway_display_bool(const json_node_t *user_config,
                                           const char *platform_key,
                                           const char *setting,
                                           bool default_val,
                                           const char *platform,
                                           const json_node_t *require_platform_override_for);

/* run.py _has_platform_display_override */
bool gateway_has_platform_display_override(const json_node_t *user_config, const char *platform_key, const char *setting);

/* ===========================================================================
 *  run.py display/format helpers
 * =========================================================================== */

/* run.py _float_env - read env var as float with fallback */
float gateway_float_env(const char *name, float default_val);

/* run.py _parse_session_key - parse session key into components */
json_node_t *gateway_parse_session_key(const char *session_key);

/* run.py _format_gateway_process_notification - format watch event */
char *gateway_format_gateway_process_notification(const json_node_t *evt);

/* run.py _normalize_empty_agent_response - ensure required fields exist */
json_node_t *gateway_normalize_empty_agent_response(json_node_t *agent_result);

/* run.py _voice_key - extract voice key from event */
const char *gateway_voice_key(const json_node_t *event);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_RUN_PURE_H */