#ifndef NOC_W7_AUTHXAI_4EAC9A_H
#define NOC_W7_AUTHXAI_4EAC9A_H
/*
 * Public C API for the port of hermes_cli/auth_xai.py (gate w7_authxai_4eac9a).
 * Only the deterministic helpers are exported; network/auth-store functions
 * (discovery, refresh, device login) are out of scope for this differential gate.
 * Strings passed as const char * may be NULL (Python None) where documented.
 */
#include "hermes_json.h"

/* _clean(value): str(value or "").strip() */
json_t *w7_authxai_clean(const json_t *value);
/* _token_pair(tokens): [access_token, refresh_token] */
json_t *w7_authxai_token_pair(const json_t *tokens);
/* _xai_oauth_state_has_usable_tokens(state) */
json_t *w7_authxai_state_has_usable_tokens(const json_t *state);
/* _is_xai_origin_host(host) */
json_t *w7_authxai_is_xai_origin_host(const char *host);
/* _xai_url_problem(url): [problem|null, host] */
json_t *w7_authxai_url_problem(const char *url);
/* _xai_jwt_exp(token): number|null */
json_t *w7_authxai_jwt_exp(const char *access_token);
/* _xai_tokens_from_payload(payload, access_token, fallback_refresh) */
json_t *w7_authxai_tokens_from_payload(const json_t *payload,
        const json_t *access_token, const char *fallback_refresh);
/* _xai_validate_inference_base_url(value, fallback) */
json_t *w7_authxai_validate_inference_base_url(const char *value, const char *fallback);

#endif /* NOC_W7_AUTHXAI_4EAC9A_H */
