/*
 * port_cli_config_load.h — pure helpers from hermes_cli/cli_config_load.py.
 *
 * _parse_service_tier_config reuses the fast_mode parsing table; the config
 * file IO in load_cli_config is the unported half, and _merge_file_config
 * takes the parsed configs as parameters and returns the merged result.
 */
#ifndef PORT_CLI_CONFIG_LOAD_H
#define PORT_CLI_CONFIG_LOAD_H

#include "hermes_json.h"

/* cli_config_load._parse_service_tier_config(raw): delegates to the fast_mode
 * parser (agent/fast_mode.py parse_service_tier). Returns a malloc'd tier
 * ("priority", "ultrafast", "auto", "cold") or NULL for normal/blank/unknown.
 * Caller frees. */
char *ccl_parse_service_tier_config(const char *raw);

/* cli_config_load._merge_file_config(defaults, file_config): overlays a parsed
 * config file onto defaults in place (model normalization, deep merge, legacy
 * max_turns). defaults_json must be a JSON object whose "model" is a dict and
 * "agent" is a dict. Returns a malloc'd JSON object = the merged defaults.
 * Caller frees. */
char *ccl_merge_file_config(const char *defaults_json, const char *file_json);

/* cli_config_load._cli_config_defaults(): the built-in CLI config defaults
 * dict (file overlays these). Includes the imported DEFAULT_SANDBOX_IMAGE.
 * Returns a new json_t object, caller frees. */
json_t *ccl_cli_config_defaults(void);

#endif /* PORT_CLI_CONFIG_LOAD_H */
