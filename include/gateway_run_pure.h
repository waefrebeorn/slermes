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

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_RUN_PURE_H */
