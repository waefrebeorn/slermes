#ifndef NOC_W22_GATEWAYENV_E84967_H
#define NOC_W22_GATEWAYENV_E84967_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/slack/gateway_env.py (gate w22_gatewayenv_e84967).
 *
 * The module is the Slack platform's env -> gateway-config bridge. `getenv`
 * reads the process environment through the active secret scope and the
 * `GatewayConfig` / `PlatformConfig` / `HomeChannel` objects are the config
 * seam; both are injected here:
 *
 *   - home:   `_home(config)` with the env values supplied explicitly. A falsy
 *             SLACK_HOME_CHANNEL changes nothing; otherwise it creates a
 *             disabled PlatformConfig if the platform has none yet and installs
 *             a HomeChannel whose thread_id drops an empty value to None and
 *             whose user_id/scope_id provenance survives only while the chat_id
 *             is unchanged.
 *   - authz:  the AUTHZ field table core reads when a live entry lacks it.
 *   - env_enable_credentials: the env names an explicit disable warns about.
 *   - env_overrides: the ordered ENV_OVERRIDES steps (one EnvCred, then _home).
 */

/* PoP: w22_gatewayenv_e84967_home @ plugins/platforms/slack/gateway_env.py:_home
 * env: {"SLACK_HOME_CHANNEL": str|null, "SLACK_HOME_CHANNEL_NAME": str|null,
 *       "SLACK_HOME_CHANNEL_THREAD_ID": str|null} (missing == unset == "").
 * existing: null when the config has no Slack PlatformConfig, else
 *           {"enabled": bool, "chat_id": str|null, "user_id": str|null,
 *            "scope_id": str|null} (its current home_channel).
 * -> {"changed": false} when SLACK_HOME_CHANNEL is empty, else
 *    {"changed": true, "created": bool, "enabled": bool, "chat_id": str,
 *     "name": str, "thread_id": str|null, "user_id": str|null,
 *     "scope_id": str|null}
 */
json_t *w22_gatewayenv_e84967_home(const json_t *env, const json_t *existing);

/* PoP: w22_gatewayenv_e84967_authz @ plugins/platforms/slack/gateway_env.py:AUTHZ */
json_t *w22_gatewayenv_e84967_authz(void);

/* PoP: w22_gatewayenv_e84967_env_enable_credentials @ plugins/platforms/slack/gateway_env.py:ENV_ENABLE_CREDENTIALS */
json_t *w22_gatewayenv_e84967_env_enable_credentials(void);

/* PoP: w22_gatewayenv_e84967_env_overrides @ plugins/platforms/slack/gateway_env.py:ENV_OVERRIDES
 * [{"kind": "cred", "platform": "slack", "creds": ["SLACK_BOT_TOKEN"],
 *   "token": "SLACK_BOT_TOKEN"},
 *  {"kind": "step", "name": "_home"}] */
json_t *w22_gatewayenv_e84967_env_overrides(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYENV_E84967_H */
