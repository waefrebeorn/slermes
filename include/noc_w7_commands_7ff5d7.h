#ifndef NOC_W7_COMMANDS_7FF5D7_H
#define NOC_W7_COMMANDS_7FF5D7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure command-table / parsing / text-formatting logic of
 * acp_adapter/commands.py. Every symbol is prefixed with the gate id. */

/* PoP: w7_commands_7ff5d7_commands_table @ acp_adapter/commands.py:SlashCommandsMixin._COMMANDS
 * name -> [help, advertised_description, hint_or_null]. Caller frees. */
json_t *w7_commands_7ff5d7_commands_table(void);

/* PoP: w7_commands_7ff5d7_mid_turn_blocked @ acp_adapter/commands.py:_MID_TURN_BLOCKED_COMMANDS */
json_t *w7_commands_7ff5d7_mid_turn_blocked(void);

/* PoP: w7_commands_7ff5d7_parse_slash @ acp_adapter/commands.py:SlashCommandsMixin._handle_slash_command
 * Mirrors text.split(maxsplit=1) + lstrip('/').lower() + args.strip().
 * Returns {"cmd":..., "args":..., "known":...}. Caller frees. */
json_t *w7_commands_7ff5d7_parse_slash(const char *text);

/* PoP: w7_commands_7ff5d7_help_text @ acp_adapter/commands.py:SlashCommandsMixin._cmd_help
 * Caller frees the returned string. */
char *w7_commands_7ff5d7_help_text(void);

/* PoP: w7_commands_7ff5d7_model_status @ acp_adapter/commands.py:SlashCommandsMixin._cmd_model
 * The no-args branch: "Current model: <model>\nProvider: <provider>".
 * Caller frees the returned string. */
char *w7_commands_7ff5d7_model_status(const char *model, const char *fallback, const char *provider);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_COMMANDS_7FF5D7_H */
