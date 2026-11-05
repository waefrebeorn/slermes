#ifndef PORT_HERMES_AUTH_NOUS_H
#define PORT_HERMES_AUTH_NOUS_H

#include "hermes_json.h"

/* Port of the pure core of Python: hermes_cli/auth_nous.py.
 *
 * The device-code flow, token refresh, shared-store mirroring and JWT selection
 * are runtime/network and are NOT ported. The pure helpers below are,
 * reproducing exact Python semantics. Env/profile-secret lookup
 * (_nous_inference_env_override) and JWT claim decoding (_decode_jwt_claims)
 * are the two impure seams: the former arrives as a parameter, the latter is
 * out of scope (the fingerprint/expiry helpers that need a decoded token take
 * the claim dict instead).
 */

#define NOUS_DEFAULT_PORTAL_URL "https://portal.nousresearch.com"

/* hashlib.sha256(token.strip()).hexdigest()[:12]; "" -> NULL. malloc'd. */
char *authn_token_fingerprint(const char *token);

/* datetime.fromtimestamp(now + ttl, UTC).isoformat() — microsecond, "+00:00".
 * malloc'd. */
char *authn_iso_after(double now_epoch, long long ttl_seconds);

/* Membership in the production allowlist, else override's urlparse hostname
 * must equal hostname (env _optional_base_url is the runtime seam). */
int authn_inference_host_allowed(const char *hostname, const char *override_url);

/* _scope_values: whitespace/comma-separated tokens from a string, recursing
 * into arrays of strings; deduped, order = first seen. JSON array (or NULL). */
json_t *authn_scope_values(const char *value_json);

/* _unusable_invoke_jwt_error: the message string (malloc'd); *code_out gets
 * "nous_auth_missing_refresh_token" or `reason`. */
char *authn_unusable_invoke_jwt_error(const char *reason, int no_refresh_token,
                                      const char **code_out);

/* _migrate_stale_nous_portal_url: if nous.portal_base_url hostname is
 * api.nousresearch.com, rewrite it to the default portal URL. Operates on a
 * parsed copy, returns the resulting object. */
json_t *authn_migrate_stale_portal_url(const char *providers_json);

/* _nous_effective_provider_state: the state dict minus the derived keys
 * {expires_in, agent_key_expires_in}. */
json_t *authn_effective_provider_state(const char *state_json);

/* _portal_host: urlparse hostname of the portal URL, else the default. malloc'd. */
char *authn_portal_host(const char *portal_url);

#endif /* PORT_HERMES_AUTH_NOUS_H */
