#ifndef NOC_W22_GATEWAYENV_8633BA_H
#define NOC_W22_GATEWAYENV_8633BA_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/sms/gateway_env.py (gate w22_gatewayenv_8633ba).
 *
 * The module is the declarative env -> gateway-config bridge for the SMS
 * (Twilio) platform: the credential-gated enable (`ENV_ENABLE_CREDENTIALS`,
 * `ENV_OVERRIDES`) and the authorization env names core reads when the live
 * PlatformEntry lacks them (`AUTHZ`).
 *
 * The step callables are values: a credential step renders its EnvCred fields
 * and names its tail (`then=_api_key`); a home step renders the
 * `home_step(Platform.SMS, "SMS_HOME_CHANNEL")` partial it is.
 */

/* PoP: w22_gatewayenv_8633ba_enable_credentials @ plugins/platforms/sms/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * The env names whose presence enables the platform, in order. */
json_t *w22_gatewayenv_8633ba_enable_credentials(void);

/* PoP: w22_gatewayenv_8633ba_overrides @ plugins/platforms/sms/gateway_env.py:ENV_OVERRIDES
 * The ordered steps: a credential step {"kind":"cred", platform, creds, token,
 * fixed, optional, optional_stripped, warn_missing, then, home, home_strip} and
 * a home step {"kind":"home", platform, env_base, strip}. */
json_t *w22_gatewayenv_8633ba_overrides(void);

/* PoP: w22_gatewayenv_8633ba_authz @ plugins/platforms/sms/gateway_env.py:AUTHZ
 * The authz table as written: allowed_users_env, allow_all_env, and the
 * allowlist_suppresses_pairing bool. */
json_t *w22_gatewayenv_8633ba_authz(void);

/* PoP: w22_gatewayenv_8633ba_authz_get @ plugins/platforms/sms/gateway_env.py:AUTHZ
 * `AUTHZ.get(key)`: the value for a key that names one of the three fields,
 * JSON null for a missing key or a key that is not a string (a number/bool key
 * is hashable in Python and simply misses). */
json_t *w22_gatewayenv_8633ba_authz_get(const json_t *key);

/* PoP: w22_gatewayenv_8633ba_cred_active @ plugins/platforms/sms/gateway_env.py:ENV_OVERRIDES
 * The EnvCred gate `all(env_first(group) for group in creds)` for the one
 * credential this platform declares: true when the SID value is a non-empty
 * string. NULL means the variable is unset (Python reads "" and the gate is
 * false). */
int w22_gatewayenv_8633ba_cred_active(const char *account_sid);

/* PoP: w22_gatewayenv_8633ba_api_key @ plugins/platforms/sms/gateway_env.py:_api_key
 * `sms_config.api_key = getenv("TWILIO_AUTH_TOKEN")`, where getenv is
 * gateway.platform_env's str helper: a missing variable reads "" (never None).
 * Returns the string that is assigned; `token` NULL means unset. */
json_t *w22_gatewayenv_8633ba_api_key(const char *token);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYENV_8633BA_H */
