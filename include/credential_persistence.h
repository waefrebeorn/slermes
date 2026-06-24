/* credential_persistence.h — Credential-pool disk-boundary sanitization.
 * Port of Python agent/credential_persistence.py.
 */
#ifndef CREDENTIAL_PERSISTENCE_H
#define CREDENTIAL_PERSISTENCE_H

#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Check if a credential source is borrowed (reference-only). */
bool is_borrowed_credential_source(const char *source, const char *provider_id);

/* Check if a key is a secret payload field. */
bool is_secret_payload_key(const char *key);

/* Generate a fingerprint for a value. Caller must free. */
char *fingerprint_value(const char *value);

/* Walk a JSON payload object for secret keys and return fingerprint. Caller must free. */
char *credential_secret_fingerprint(const json_t *payload);

/* Sanitize a credential payload for disk storage. Caller must free. */
json_t *sanitize_borrowed_credential_payload(json_t *payload, const char *provider_id);

#ifdef __cplusplus
}
#endif

#endif /* CREDENTIAL_PERSISTENCE_H */
