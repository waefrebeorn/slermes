#ifndef NOC_W22_GATEWAYENV_1C1B2E_H
#define NOC_W22_GATEWAYENV_1C1B2E_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/telegram/gateway_env.py (gate w22_gatewayenv_1c1b2e).
 *
 * The module is a declaration: the env names that gate the credential, the
 * ordered ``ENV_OVERRIDES`` steps that build the Telegram PlatformConfig, the
 * authorization env names core reads when the live entry lacks them, and the
 * one inline step (``_fallback_ips``) that has real logic.
 */

/* PoP: w22_gatewayenv_1c1b2e_enable_credentials @ plugins/platforms/telegram/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * The tuple verbatim: ["TELEGRAM_BOT_TOKEN"]. */
json_t *w22_gatewayenv_1c1b2e_enable_credentials(void);

/* PoP: w22_gatewayenv_1c1b2e_authz @ plugins/platforms/telegram/gateway_env.py:AUTHZ
 * The mapping verbatim, in source order, including the boolean
 * ``allowlist_suppresses_pairing``. */
json_t *w22_gatewayenv_1c1b2e_authz(void);

/* PoP: w22_gatewayenv_1c1b2e_overrides @ plugins/platforms/telegram/gateway_env.py:ENV_OVERRIDES
 * The ordered steps as descriptors, in application order:
 *   {"kind":"cred","platform":"telegram","creds":[[...]],"token":"..."}
 *   {"kind":"reply_mode","platform":"telegram","env":"..."}
 *   {"kind":"callable","name":"_fallback_ips"}
 *   {"kind":"home","platform":"telegram","env_base":"...","strip":false}
 * Order matters: the reply mode / fallback IPs may create the entry the home
 * channel then attaches to. */
json_t *w22_gatewayenv_1c1b2e_overrides(void);

/* PoP: w22_gatewayenv_1c1b2e_fallback_ips @ plugins/platforms/telegram/gateway_env.py:_fallback_ips
 * `if ips := getenv("TELEGRAM_FALLBACK_IPS"):
 *      config.platforms.setdefault(Platform.TELEGRAM, PlatformConfig())
 *            .extra["fallback_ips"] = csv_list(ips)`
 *
 * `env` is the environment mapping.  The getenv seam only ever yields text, so
 * a non-string (or missing) value reads as "unset" -- exactly `_getenv_str`.
 * Answer: null when unset/blank, else {"fallback_ips": [...]} with csv_list's
 * trim-and-drop-empty semantics. */
json_t *w22_gatewayenv_1c1b2e_fallback_ips(const json_t *env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYENV_1C1B2E_H */
