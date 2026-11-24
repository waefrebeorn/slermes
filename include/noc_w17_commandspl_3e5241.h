#ifndef NOC_W17_COMMANDSPL_3E5241_H
#define NOC_W17_COMMANDSPL_3E5241_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/commands_platforms.py (gate w17_commandspl_3e5241).
 * Pure name/description shapers for Telegram, Discord and Slack menus. */
json_t *w17_commandspl_3e5241_requires_argument(const char *args_hint);
json_t *w17_commandspl_3e5241_sanitize_telegram_name(const char *raw);
json_t *w17_commandspl_3e5241_normalize_telegram_desc(const char *desc);
json_t *w17_commandspl_3e5241_truncate_desc(const char *desc, int limit);
json_t *w17_commandspl_3e5241_clamp_command_names(const json_t *entries, const json_t *reserved);
json_t *w17_commandspl_3e5241_sanitized_rank(const json_t *raw_names);
json_t *w17_commandspl_3e5241_sanitize_slack_name(const char *raw);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_COMMANDSPL_3E5241_H */
