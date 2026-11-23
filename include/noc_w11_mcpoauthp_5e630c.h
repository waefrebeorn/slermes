#ifndef NOC_W11_MCPOAUTHP_5E630C_H
#define NOC_W11_MCPOAUTHP_5E630C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_mcpoauthp_5e630c_metadata_issuer @ tools/mcp_oauth_provider.py:_metadata_issuer
 * context is {"oauth_metadata":{"issuer":...}|null}. Returns string or null. */
struct json_t *w11_mcpoauthp_5e630c_metadata_issuer(const struct json_t *context);

/* PoP: w11_mcpoauthp_5e630c_asm_discovery_failure @ tools/mcp_oauth_provider.py:_asm_discovery_failure
 * response is {"request":{"url":...}|null,"status_code":<int>|null,"url":...}. */
struct json_t *w11_mcpoauthp_5e630c_asm_discovery_failure(const struct json_t *response);

/* PoP: w11_mcpoauthp_5e630c_with_discovery_context @ tools/mcp_oauth_provider.py:_with_discovery_context */
struct json_t *w11_mcpoauthp_5e630c_with_discovery_context(const char *exc,
                                                           const struct json_t *failures);

/* PoP: w11_mcpoauthp_5e630c_metadata_issued_by_origin @ tools/mcp_oauth_provider.py:metadata_issued_by_origin */
struct json_t *w11_mcpoauthp_5e630c_metadata_issued_by_origin(const struct json_t *metadata,
                                                              const char *auth_server_url,
                                                              const struct json_t *response);

/* PoP: w11_mcpoauthp_5e630c_google_offline_access_params @ tools/mcp_oauth_provider.py:google_offline_access_params */
struct json_t *w11_mcpoauthp_5e630c_google_offline_access_params(const struct json_t *context);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_MCPOAUTHP_5E630C_H */
