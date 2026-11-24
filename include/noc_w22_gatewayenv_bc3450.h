#ifndef NOC_W22_GATEWAYENV_BC3450_H
#define NOC_W22_GATEWAYENV_BC3450_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/whatsapp/gateway_env.py (gate w22_gatewayenv_bc3450).
 *
 * The module is the WhatsApp (Baileys bridge) env -> gateway config bridge: a
 * declarative AUTHZ table, the one env name its explicit-disable WARNING cites,
 * the `_enabled_flag` step and the `home_step` channel step. The steps mutate a
 * live GatewayConfig, so each answer here is the *decision* the step takes for
 * one env reading; the C takes the env value explicitly (the Windows CRT cannot
 * carry a present-but-empty variable) and the caller applies it.
 */

/* PoP: w22_gatewayenv_bc3450_env_enable_credentials @ plugins/platforms/whatsapp/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * ["WHATSAPP_ENABLED"] -- the env names the explicit-disable WARNING cites. */
json_t *w22_gatewayenv_bc3450_env_enable_credentials(void);

/* PoP: w22_gatewayenv_bc3450_authz @ plugins/platforms/whatsapp/gateway_env.py:AUTHZ
 * {"allowed_users_env":..., "allow_all_env":..., "own_policy_env":[...,...],
 *  "allowlist_suppresses_pairing":true} in the literal's insertion order. */
json_t *w22_gatewayenv_bc3450_authz(void);

/* PoP: w22_gatewayenv_bc3450_enabled_flag @ plugins/platforms/whatsapp/gateway_env.py:_enabled_flag
 * `_enabled_flag` for one reading of WHATSAPP_ENABLED (`raw`; NULL/absent reads
 * as ""). `has_platform` is `config.platforms.get(Platform.WHATSAPP) is not
 * None`; `yaml_enabled` is that entry's `enabled` before the step runs.
 * Answer: {"action":"disable"|"enable"|"none","enabled":bool|null}, where the
 * flag is post-step (null when the config carries no WhatsApp entry).
 * WHATSAPP_ENABLED=false|0|no turns the entry off; a truthy value runs the
 * credential enable only when the value is not one of those (and never beats
 * the explicit disable). */
json_t *w22_gatewayenv_bc3450_enabled_flag(const char *raw, int has_platform, int yaml_enabled);

/* PoP: w22_gatewayenv_bc3450_home_channel @ plugins/platforms/whatsapp/gateway_env.py:home_step
 * `home_step(Platform.WHATSAPP, "WHATSAPP_HOME_CHANNEL")` applied to a config:
 * the HomeChannel it writes, or null when the channel env is empty or the
 * platform is not configured. `name` is WHATSAPP_HOME_CHANNEL_NAME (default
 * "Home"), `thread_id` is WHATSAPP_HOME_CHANNEL_THREAD_ID (empty -> null). */
json_t *w22_gatewayenv_bc3450_home_channel(const char *chat_id, const char *name,
                                           const char *thread_id, int platform_configured);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W22_GATEWAYENV_BC3450_H */
