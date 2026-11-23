#ifndef NOC_W8_SESSIONCONF_6D4604_H
#define NOC_W8_SESSIONCONF_6D4604_H

/*
 * Public C API for the port of gateway/session_config.py (gate w8_sessionconf_6d4604).
 * All exported symbols are sessionconf_-prefixed to avoid collisions.
 */
#include "hermes_json.h"

/* PoP: sessionconf_mcp_revision @ gateway/session_config.py:mcp_revision
 * `source` is policy.config() when a policy is present, else cfg. Returns the
 * 12-hex-character revision over the three MCP sections. */
json_t *sessionconf_mcp_revision(const json_t *source);

/* PoP: sessionconf_config_get_problem @ gateway/session_config.py:config_get
 * The pure parameter-validation prefix: "invalid_params" | "busy" | "ok". */
json_t *sessionconf_config_get_problem(const json_t *params);

/* PoP: sessionconf_model_options_problem @ gateway/session_config.py:model_options
 * The pure parameter-validation prefix: "invalid_params" | "ok". */
json_t *sessionconf_model_options_problem(const json_t *params);

/* PoP: sessionconf_fast_value @ gateway/session_config.py:_fast
 * {'priority': 'fast', 'ultrafast': 'ultrafast'}.get(tier, 'normal'). */
json_t *sessionconf_fast_value(const char *tier);

#endif /* NOC_W8_SESSIONCONF_6D4604_H */
