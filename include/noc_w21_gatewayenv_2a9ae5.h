#ifndef NOC_W21_GATEWAYENV_2A9AE5_H
#define NOC_W21_GATEWAYENV_2A9AE5_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/qqbot/gateway_env.py (gate w21_gatewayenv_2a9ae5).
 *
 * The module is a declarative env->gateway-config bridge: one EnvCred row plus
 * the QQ home-channel tail. Everything it declares -- and the whole `_home`
 * env resolution -- is here; `EnvCred.__call__` itself lives in
 * gateway/platform_env.py and stays upstream. */

/* PoP: w21_gatewayenv_2a9ae5_home @ plugins/platforms/qqbot/gateway_env.py:_home
 * `_home(config, qq_config)` resolved into a value:
 *   {"name_env": "QQBOT_HOME_CHANNEL_NAME"|"QQ_HOME_CHANNEL_NAME",
 *    "warned": bool,                       # the deprecated-var warning fired
 *    "home_channel": {"platform":"qqbot","chat_id":...,"name":...,
 *                     "thread_id":...|null} | null}
 * `env` is the environment as a JSON object of strings; a missing name reads
 * as getenv's default ("" unless one is given), so an empty var is set-but-blank
 * and differs from absent for the name/thread fallbacks. */
json_t *w21_gatewayenv_2a9ae5_home(const json_t *env);

/* PoP: w21_gatewayenv_2a9ae5_authz @ plugins/platforms/qqbot/gateway_env.py:AUTHZ
 * The PlatformEntry authorization env names QQ declares. */
json_t *w21_gatewayenv_2a9ae5_authz(void);

/* PoP: w21_gatewayenv_2a9ae5_env_enable @ plugins/platforms/qqbot/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * The env names the explicit-disable WARNING cites. */
json_t *w21_gatewayenv_2a9ae5_env_enable(void);

/* PoP: w21_gatewayenv_2a9ae5_override_plan @ plugins/platforms/qqbot/gateway_env.py:ENV_OVERRIDES
 * The single EnvCred row as declared: platform key, the all-of/any-of cred
 * groups, the env_extras specs (plain and stripped), and whether the tail step
 * is `_home`. */
json_t *w21_gatewayenv_2a9ae5_override_plan(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W21_GATEWAYENV_2A9AE5_H */
