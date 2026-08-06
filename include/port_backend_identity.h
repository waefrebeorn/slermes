#ifndef PORT_BACKEND_IDENTITY_H
#define PORT_BACKEND_IDENTITY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FailureScope enum — matches Python agent/backend_identity.py:FailureScope.
 * 0 = MODEL      (timeout, rate limit, connection blip, ...)
 * 1 = CREDENTIAL (auth 401, payment 402)
 * 2 = ENDPOINT   (DNS failure, connection refused) */
enum { FAILURE_SCOPE_MODEL = 0,
       FAILURE_SCOPE_CREDENTIAL = 1,
       FAILURE_SCOPE_ENDPOINT = 2 };

/* Opaque BackendIdentity — normalized identity of one (provider, model, endpoint).
 * Empty fields mean "unknown" — comparisons treat unknown axes as non-distinguishing. */
typedef struct backend_identity_s BackendIdentity;

/* --- Normalization helpers (pure) --- */
/* Python: _norm_provider(value) — strip + lowercase */
char *agent_backend_identity_norm_provider(const char *value);
/* Python: _norm_model(value) — strip + lowercase (identical to _norm_provider) */
char *agent_backend_identity_norm_model(const char *value);
/* Python: _norm_base_url(value) — strip + rstrip('/') + lowercase */
char *agent_backend_identity_norm_base_url(const char *value);

/* --- classify_failure_scope --- */
/* Python: classify_failure_scope(reason) → FailureScope enum int.
 * Returns: 0=MODEL, 1=CREDENTIAL, 2=ENDPOINT. Default=0 (MODEL). */
int agent_backend_identity_classify_failure_scope(const char *reason);

/* --- BackendIdentity lifecycle --- */
/* Python: BackendIdentity.build(provider, model, base_url) — normalizes each field. */
BackendIdentity *agent_backend_identity_build(const char *provider,
                                               const char *model,
                                               const char *base_url);
void agent_backend_identity_free(BackendIdentity *bi);
const char *agent_backend_identity_provider(const BackendIdentity *bi);
const char *agent_backend_identity_model(const BackendIdentity *bi);
const char *agent_backend_identity_base_url(const BackendIdentity *bi);

/* --- Comparison helpers --- */
/* Python: _both_first_class(a, b) — both providers are distinct registered providers */
bool agent_backend_identity_both_first_class(const BackendIdentity *a, const BackendIdentity *b);
/* Python: same_credential_surface(a, b) — do they share the credential? */
bool agent_backend_identity_same_credential_surface(const BackendIdentity *a, const BackendIdentity *b);
/* Python: same_endpoint(a, b) — do they sit behind the same endpoint? */
bool agent_backend_identity_same_endpoint(const BackendIdentity *a, const BackendIdentity *b);
/* Python: same_deployment(a, b) — exact same model deployment? */
bool agent_backend_identity_same_deployment(const BackendIdentity *a, const BackendIdentity *b);
/* Python: should_skip_candidate(candidate, failed, scope) — THE skip predicate.
 * scope: 0=MODEL, 1=CREDENTIAL, 2=ENDPOINT */
bool agent_backend_identity_should_skip_candidate(const BackendIdentity *candidate,
                                                   const BackendIdentity *failed,
                                                   int scope);

#ifdef __cplusplus
}
#endif

#endif /* PORT_BACKEND_IDENTITY_H */
