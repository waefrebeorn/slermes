#ifndef NOC_W9_PERMISSIONS_D89402_H
#define NOC_W9_PERMISSIONS_D89402_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure permission-bridging helpers of acp_adapter/permissions.py.
 * Every symbol is prefixed with the gate id. */

/* PoP: w9_permissions_d89402_resolve_timeout @ acp_adapter/permissions.py:resolve_permission_timeout
 * given != 0 => return timeout; otherwise return config_timeout (the injected
 * _get_approval_timeout() value). Mirrors float() coercion. */
double w9_permissions_d89402_resolve_timeout(double timeout, int given, double config_timeout);

/* PoP: w9_permissions_d89402_map_outcome @ acp_adapter/permissions.py:_map_outcome_to_hermes
 * valid mirrors isinstance(outcome, AllowedOutcome); allowed is a JSON array of option ids.
 * Returns a borrowed static string ("once"/"session"/"always"/"deny"). */
const char *w9_permissions_d89402_map_outcome(const char *option_id, int valid, const json_t *allowed);

/* PoP: w9_permissions_d89402_build_options @ acp_adapter/permissions.py:_build_permission_options
 * supports_reject_always is the injected _permission_option_supports_kind("reject_always").
 * Returns a JSON array of {"option_id","kind","name"} objects. Caller frees. */
json_t *w9_permissions_d89402_build_options(int allow_permanent, int allow_session,
                                            int smart_denied, int supports_reject_always);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_PERMISSIONS_D89402_H */
