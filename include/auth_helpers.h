/*
 * auth_helpers.h — Pure auth/secret helpers (faithful C11 port of the
 * testable core of hermes_cli/auth.py).
 *
 * Ports the self-contained logic only: secret validation, provider base-URL
 * resolution, retry-after parsing, ISO-timestamp / expiry / TTL logic,
 * OAuth scope parsing, JWT claim decoding (base64url, no network), token
 * fingerprinting, and structured AuthError formatting. Network/JWT-invoke
 * and persistent auth-store logic are intentionally out of scope here.
 *
 * All functions are pure and unit-testable against an isolated environment.
 */

#ifndef AUTH_HELPERS_H
#define AUTH_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── secret validation ── */

bool auth_has_usable_secret(const char *value, int min_length);

/* ── provider base URL resolution ── */

/* Kimi: sk-kimi- prefixed keys route to api.kimi.com/coding; env override wins. */
char *auth_resolve_kimi_base_url(const char *api_key, const char *default_url, const char *env_override);

/* LM Studio: strip /api/v1, /api, /v1 suffixes, append /v1. */
char *auth_normalize_lmstudio_runtime_base_url(const char *base_url);

/* ── token fingerprint (sha256 prefix, no token bytes leaked) ── */
char *auth_token_fingerprint(const char *token);

/* ── retry-after parsing (delta-seconds form; HTTP-date/garbage -> NULL) ── */
int auth_parse_retry_after(const char *raw_value);  /* returns seconds or -1 */

/* ── ISO timestamp / expiry / TTL ── */
double auth_parse_iso_timestamp(const char *value);  /* epoch seconds or -1 */
bool   auth_is_expiring(const char *expires_at_iso, int skew_seconds);
int    auth_coerce_ttl_seconds(const char *expires_in);  /* >=0 */
char  *auth_optional_base_url(const char *value);  /* trimmed, no trailing '/', or NULL */

/* ── OAuth scope parsing ── */
/* Caller passes a comma/space-separated scope string (or NULL). Returns a
 * dynamically-allocated, NUL-separated token list with *out_count entries.
 * Free with auth_free_scope(). */
char **auth_scope_values(const char *raw_scope, int *out_count);
void   auth_free_scope(char **scopes, int n);

/* ── JWT claim decode (base64url payload, no network) ── */
/* Returns a malloc'd JSON string of the payload, or NULL on failure. The
 * caller can query fields with auth_jwt_get_str / auth_jwt_get_num. */
char *auth_decode_jwt_payload(const char *token);
/* Extract a string claim (e.g. "scope"/"scp"/"sub"); returns malloc'd string or NULL. */
char *auth_jwt_get_str(const char *payload_json, const char *key);
/* Extract a numeric claim (e.g. "exp"); returns parsed value or -1 if absent/non-numeric. */
double auth_jwt_get_num(const char *payload_json, const char *key);

/* ── structured auth error ── */
typedef struct {
    char *message;
    char *provider;
    char *code;
    bool  relogin_required;
} auth_error_t;

auth_error_t *auth_error_new(const char *message, const char *provider, const char *code, bool relogin_required);
void auth_error_free(auth_error_t *e);

/* True for upstream rate-limit / quota exhaustion (transient, not cred error). */
bool auth_is_rate_limited_error(const auth_error_t *e);

/* Map auth failures to concise user-facing guidance. Returns malloc'd string. */
char *auth_format_error(const auth_error_t *e);

/* ── Nous invoke-JWT status (pure; uses the helpers above) ── */
#define AUTH_NOUS_INFERENCE_INVOKE_SCOPE "nous.inference.invoke"
#define AUTH_NOUS_INVOKE_JWT_MIN_TTL_SECONDS 300
/* Returns NULL when usable; otherwise a malloc'd reason string. */
char *auth_nous_invoke_jwt_status(const char *token, const char *scope, const char *expires_at, int min_ttl_seconds);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_HELPERS_H */
