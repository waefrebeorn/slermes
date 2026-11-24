/*
 * noc_w25_adaptercon_1cab56.h - C surface for upstream
 * plugins/platforms/discord/adapter_config_readers.py (gate w25_adaptercon_1cab56).
 *
 * The config.yaml readers of the Discord adapter as pure value functions: the
 * raw config value is the fixed input here (the config file itself is a seam),
 * and what is ported is what those values decide -- the DM role-auth guild
 * opt-in, the clamped button-view timeout, and the exec-approval admin gate.
 */
#ifndef NOC_W25_ADAPTERCON_1CAB56_H
#define NOC_W25_ADAPTERCON_1CAB56_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* raw discord.dm_role_auth_guild -> positive guild id, or null (secure default). */
json_t *w25_adaptercon_1cab56_dm_role_auth_guild(const json_t *raw);

/* raw approvals.discord_prompt_timeout -> seconds, default 300, clamped. */
json_t *w25_adaptercon_1cab56_discord_prompt_timeout(const json_t *raw);

/* extra -> {"require_admin": bool, "admin_user_ids": [str, ...]}. */
json_t *w25_adaptercon_1cab56_exec_approval_gate(const json_t *extra);

/* raw admin/user id list -> the normalized, sorted id list. */
json_t *w25_adaptercon_1cab56_coerce_id_list(const json_t *raw);

/* The timeout bounds and gate keys as an object. */
json_t *w25_adaptercon_1cab56_constants(void);

#ifdef __cplusplus
}
#endif
#endif
