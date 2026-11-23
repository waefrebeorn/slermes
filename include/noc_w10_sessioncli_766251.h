#ifndef NOC_W10_SESSIONCLI_766251_H
#define NOC_W10_SESSIONCLI_766251_H

#include <hermes_json.h>

/* C11 ports of agent/session_client_env.py.  Every symbol is prefixed
 * w10_sessioncli_766251_.  Each takes one borrowed json_t *args object and
 * returns a NEW json_t *.  Secret-substring state (normally
 * tools.code_execution_env._SECRET_SUBSTRINGS) is injected as "secret". */
json_t *w10_sessioncli_766251_configured_names(json_t *a);
json_t *w10_sessioncli_766251_client_env_rules(json_t *a);
json_t *w10_sessioncli_766251_travels(json_t *a);
json_t *w10_sessioncli_766251_normalise(json_t *a);
json_t *w10_sessioncli_766251_accept(json_t *a);
json_t *w10_sessioncli_766251_retry_matches(json_t *a);

#endif /* NOC_W10_SESSIONCLI_766251_H */
