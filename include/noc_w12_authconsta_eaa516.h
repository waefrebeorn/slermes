#ifndef NOC_W12_AUTHCONSTA_EAA516_H
#define NOC_W12_AUTHCONSTA_EAA516_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_authconsta_eaa516_decode_jwt_claims @ hermes_cli/auth_constants.py:_decode_jwt_claims */
struct json_t *w12_authconsta_eaa516_decode_jwt_claims(const char *token);

/* PoP: w12_authconsta_eaa516_error @ hermes_cli/auth_constants.py:_provider_error_factory
 * Builds the AuthError a provider factory would return, projected as a dict. */
struct json_t *w12_authconsta_eaa516_error(const char *provider, const char *message,
                                           const struct json_t *code, int relogin);

/* PoP: w12_authconsta_eaa516_constants @ hermes_cli/auth_constants.py:<module constants> */
struct json_t *w12_authconsta_eaa516_constants(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_AUTHCONSTA_EAA516_H */
