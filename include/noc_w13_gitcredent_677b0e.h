#ifndef NOC_W13_GITCREDENT_677B0E_H
#define NOC_W13_GITCREDENT_677B0E_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_gitcredent_677b0e_https_origin @ hermes_cli/git_credentials.py:_https_origin
 * "https://" + host ([ipv6] bracketed) + optional :port; NULL for non-HTTPS/no host. Caller frees. */
char *w13_gitcredent_677b0e_https_origin(const char *url);
/* PoP: w13_gitcredent_677b0e_token_pair @ hermes_cli/git_credentials.py:_token_pair
 * ("x-access-token", token) for a non-empty token, else JSON null. */
struct json_t *w13_gitcredent_677b0e_token_pair(const struct json_t *token);
/* PoP: w13_gitcredent_677b0e_auth_headers @ hermes_cli/git_credentials.py:_auth_headers
 * Sorted set of env values whose key starts GIT_CONFIG_VALUE_ and value starts "Authorization:". */
struct json_t *w13_gitcredent_677b0e_auth_headers(const struct json_t *env);
/* PoP: w13_gitcredent_677b0e_credential_required @ hermes_cli/git_credentials.py:is_credential_required_error
 * Regex classification of stderr+"\n"+stdout for the credential-required error class. */
struct json_t *w13_gitcredent_677b0e_credential_required(const struct json_t *err, const struct json_t *out);
/* PoP: w13_gitcredent_677b0e_with_git_auth @ hermes_cli/git_credentials.py:with_git_auth
 * Copy of env with http.<origin>/.extraheader added from the given [user,password] pair. */
struct json_t *w13_gitcredent_677b0e_with_git_auth(const struct json_t *env,
                                                   const struct json_t *url,
                                                   const struct json_t *auth);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_GITCREDENT_677B0E_H */
