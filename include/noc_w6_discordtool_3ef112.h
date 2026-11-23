#ifndef NOC_W6_DISCORDTOOL_3EF112_H
#define NOC_W6_DISCORDTOOL_3EF112_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: discordtool_validate_snowflake @ tools/discord_tool.py:_validate_snowflake
 * Returns JSON null when the value is a 1-20 digit ASCII snowflake, else the
 * exact ValueError message string. Caller frees. */
json_t *discordtool_validate_snowflake(const char *value, const char *param_name);

/* PoP: discordtool_channel_type_name @ tools/discord_tool.py:_channel_type_name
 * `type_id` is an int64 decimal string. Returns the channel type name or
 * "unknown(<id>)". Caller frees. */
json_t *discordtool_channel_type_name(const char *type_id);

/* PoP: discordtool_limit_param @ tools/discord_tool.py:_limit_param
 * Both `limit` and `default` are decimal strings. Returns str(min(int(x),100))
 * with the Python fallback when parsing fails. Caller frees. */
json_t *discordtool_limit_param(const char *limit, const char *default_value);

/* PoP: discordtool_enrich_403 @ tools/discord_tool.py:_enrich_403
 * Returns the exact user-facing 403 guidance sentence. Caller frees. */
json_t *discordtool_enrich_403(const char *action, const char *body);

/* PoP: discordtool_token_cache_key @ tools/discord_tool.py:_token_cache_key
 * Returns sha256(token)[:16] as a hex string (in-process SHA-256). Caller frees. */
json_t *discordtool_token_cache_key(const char *token);

/* PoP: discordtool_available_actions @ tools/discord_tool.py:_available_actions
 * `caps` is an object, `allowlist` JSON null (unrestricted) or an array of
 * action names. Returns visible actions in _ACTIONS order. Caller frees. */
json_t *discordtool_available_actions(const json_t *caps, const json_t *allowlist);

#ifdef __cplusplus
}
#endif
#endif
