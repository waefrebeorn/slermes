/**
 * @file hermes_gateway_runtime_footer.h
 * @brief Runtime footer API (port of Python gateway/runtime_footer.py).
 */
#ifndef HERMES_GATEWAY_RUNTIME_FOOTER_H
#define HERMES_GATEWAY_RUNTIME_FOOTER_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Runtime Footer
 * ================================================================ */

/* Make HOME-relative path (~/foo). Port of _home_relative_cwd().
 * Returns malloc'd string. Caller must free. */
char *home_relative_cwd(const char *cwd);

/* Drop vendor/ prefix: "openai/gpt-5.4" -> "gpt-5.4".
 * Port of _model_short(). Returns malloc'd string. Caller must free. */
char *model_short(const char *model);

/* Resolve effective runtime-footer config for a platform.
 * Returns json_node_t with {enabled, fields}. Caller must json_free. */
json_node_t *resolve_footer_config(json_node_t *user_config,
                                    const char *platform_key);

/* Render the footer line. Returns malloc'd string (caller must free).
 * Empty string if no fields have data or footer is disabled. */
char *format_runtime_footer(const char *model,
                             int context_tokens,
                             int context_length,
                             const char *cwd,
                             json_node_t *fields);

/* Top-level entry point. Returns footer text (empty when disabled/no data).
 * Caller must free. */
/* Humanize a turn duration: "<1s", "22s", "1m05s". */
const char *format_latency(double seconds);

char *build_footer_line(json_node_t *user_config,
                         const char *platform_key,
                         const char *model,
                         int context_tokens,
                         int context_length,
                         const char *cwd);

#endif /* HERMES_GATEWAY_RUNTIME_FOOTER_H */