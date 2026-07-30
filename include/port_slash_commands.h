/*
 * port_slash_commands.h — Faithful C11 ports of gateway/slash_commands.py
 * (GatewaySlashCommandsMixin) helpers that are self-contained or thin wrappers
 * over already-ported subsystems (reasoning parser, i18n, config write, slash
 * access policy). Runtime-object-coupled handlers (self._agent, live queues)
 * stay in the gateway runner; these are the pure-logic / dependency-backed
 * chokepoints.
 */

#ifndef PORT_SLASH_COMMANDS_H
#define PORT_SLASH_COMMANDS_H

#include <stdbool.h>
#include "hermes_json.h"
#include "hermes_gateway_types.h"

/* hermes_constants.parse_reasoning_effort — parse a reasoning effort level into
 * a config object. Returns:
 *   - a malloc'd json_t object {"enabled": false} for "none"/"false"/"disabled"
 *   - {"enabled": true, "effort": <level>} for a valid level
 *   - NULL when empty/unrecognized (caller uses provider default)
 * effort_is_bool_false: pass true to model the YAML boolean False case
 * (reasoning_effort: false/off/no) which always means disabled. When true the
 * string arg is ignored. */
json_t *reasoning_parse_effort(const char *effort, bool effort_is_bool_false);

/* slash_commands._reasoning_picker_choices — build the interactive picker
 * choice list for /reasoning. Returns a malloc'd json_t array of
 * {value,label,is_current} objects. current_effort may be NULL. */
json_t *slash_reasoning_picker_choices(const char *current_effort);

/* slash_commands._save_gateway_config_key — set a dot-separated key in
 * config.yaml (creating intermediate objects) and atomically write it back.
 * config_path is the target file (typically <home>/config.yaml). value is set
 * verbatim (already a json_t). Returns true on success, false on any error. */
bool slash_save_gateway_config_key(const char *config_path,
                                   const char *key_path,
                                   json_t *value);

/* slash_commands._resume_caller_is_admin — whether source is an EXPLICITLY
 * configured admin allowed to make a cross-origin /resume or /sessions listing.
 * Stricter than slash_policy_is_admin(): requires gating ENABLED and a real
 * configured admin uid. Fails closed (false) on any error. */
bool slash_resume_caller_is_admin(json_node_t *gateway_config,
                                  const gw_session_source_t *source);

#endif /* PORT_SLASH_COMMANDS_H */
