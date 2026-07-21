/**
 * @file hermes_gateway_display_config.h
 * @brief Display config API (port of Python gateway/display_config.py).
 */
#ifndef HERMES_GATEWAY_DISPLAY_CONFIG_H
#define HERMES_GATEWAY_DISPLAY_CONFIG_H

#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* ================================================================
 *  Display Config
 * ================================================================ */

/* Resolve a display setting with per-platform override support.
 * Resolution: platform override > global setting > default.
 * Returns malloc'd string or NULL. Caller must free. */
char *resolve_display_setting(json_node_t *user_config,
                               const char *platform_key,
                               const char *setting,
                               const char *fallback);

/* Normalize a display value string (lowercase).
 * Returns malloc'd string. Caller must free. */
char *normalise_display_value(const char *setting, const char *value);

#endif /* HERMES_GATEWAY_DISPLAY_CONFIG_H */