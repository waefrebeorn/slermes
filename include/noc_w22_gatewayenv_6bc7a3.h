#ifndef NOC_W22_GATEWAYENV_6BC7A3_H
#define NOC_W22_GATEWAYENV_6BC7A3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of plugins/platforms/matrix/gateway_env.py (gate w22_gatewayenv_6bc7a3).
 *
 * The module is a declaration: one EnvCred step plus one home-channel step, the
 * authorization env names, and the `_e2ee` tail step that folds MATRIX_E2EE_MODE
 * / MATRIX_ENCRYPTION / MATRIX_DEVICE_ID into PlatformConfig.extra. Applying the
 * declaration to a live GatewayConfig stays upstream; the env reads are injected
 * as a map so the decisions run without touching the process environment.
 */

/* PoP: w22_gatewayenv_6bc7a3_e2ee @ plugins/platforms/matrix/gateway_env.py:_e2ee
 * `env` is the environment as a mapping of name -> string (a JSON null means
 * unset; any other value and any non-object `env` carries no variables).
 * Answer, in `extra` order: {"encryption": bool} always, plus "e2ee_mode" when
 * MATRIX_E2EE_MODE strips to a non-empty mode, plus "device_id" when
 * MATRIX_DEVICE_ID is truthy. */
json_t *w22_gatewayenv_6bc7a3_e2ee(const json_t *env);

/* PoP: w22_gatewayenv_6bc7a3_authz @ plugins/platforms/matrix/gateway_env.py:AUTHZ
 * The PlatformEntry authorization env names. */
json_t *w22_gatewayenv_6bc7a3_authz(void);

/* PoP: w22_gatewayenv_6bc7a3_enable_credentials @ plugins/platforms/matrix/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * The env names the explicit-disable WARNING cites. */
json_t *w22_gatewayenv_6bc7a3_enable_credentials(void);

/* PoP: w22_gatewayenv_6bc7a3_env_overrides @ plugins/platforms/matrix/gateway_env.py:ENV_OVERRIDES
 * The declared steps, in order, as data:
 *   {"kind":"EnvCred","platform":..,"creds":[[..]],"token":..,"fixed":[[..]],
 *    "optional":[[..]],"optional_stripped":[],"warn_missing":[env,msg],
 *    "then":"_e2ee","home":null,"home_strip":false}
 *   {"kind":"home_step","platform":"matrix","env_base":"MATRIX_HOME_ROOM","strip":false} */
json_t *w22_gatewayenv_6bc7a3_env_overrides(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_GATEWAYENV_6BC7A3_H */
