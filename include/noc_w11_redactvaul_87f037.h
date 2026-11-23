#ifndef NOC_W11_REDACTVAUL_87F037_H
#define NOC_W11_REDACTVAUL_87F037_H

#include <hermes_json.h>

/* C11 ports of agent/redact_vault_values.py.  Every symbol is prefixed
 * w11_redactvaul_87f037_.  Each function takes one borrowed json_t *args
 * object and returns a NEW json_t * (caller frees).  The vault-value
 * registry is process-global in the C port: one profile per oracle process,
 * matching the live module's profile-scoped registry. */
json_t *w11_redactvaul_87f037_register(json_t *a);
json_t *w11_redactvaul_87f037_clear(json_t *a);
json_t *w11_redactvaul_87f037_redact(json_t *a);

#endif /* NOC_W11_REDACTVAUL_87F037_H */
