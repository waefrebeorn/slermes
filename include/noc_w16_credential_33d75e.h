#ifndef NOC_W16_CREDENTIAL_33D75E_H
#define NOC_W16_CREDENTIAL_33D75E_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/credential_pool_plugin.py.
 * Pool-side side effects (logging/sync/mark-dead) are out of scope; the three
 * deterministic decision/merge helpers are ported exactly. */
json_t *w16_credential_33d75e_is_terminal_plugin_refresh_error(const json_t *exc);
json_t *w16_credential_33d75e_plugin_row_is_expiring(const char *expires_at_ms, const char *now_ms);
json_t *w16_credential_33d75e_apply_plugin_refresh_result(const json_t *entry, const json_t *result);

#endif /* NOC_W16_CREDENTIAL_33D75E_H */
