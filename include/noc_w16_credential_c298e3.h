#ifndef NOC_W16_CREDENTIAL_C298E3_H
#define NOC_W16_CREDENTIAL_C298E3_H

#include "hermes_json.h"

/* Faithful C11 port of upstream agent/credential_pool_errors.py.
 * The delegated pattern redactor (agent.redact.redact_sensitive_text) is a
 * seam: the harness/oracle pin it to identity so the module's own known-secret
 * masking and context normalisation are compared directly. */
json_t *w16_credential_c298e3_parse_absolute_timestamp(const json_t *value);
json_t *w16_credential_c298e3_redact_error_text(const char *text, const json_t *known_secrets);
json_t *w16_credential_c298e3_normalize_error_context(const json_t *error_context,
                                                      const json_t *known_secrets);

#endif /* NOC_W16_CREDENTIAL_C298E3_H */
