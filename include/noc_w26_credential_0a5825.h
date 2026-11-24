/*
 * noc_w26_credential_0a5825.h - C surface for upstream
 * plugins/config_backends/remote/credentials.py (gate w26_credential_0a5825).
 *
 * Which plane credential a Remote Config request sends, and what it is made of:
 * the ``GATEWAY_RELAY_IDP_*`` view, the idp/nous switch, the OAuth2
 * client-credentials form, the cache TTL the response's ``expires_in`` earns,
 * and the env-name blocklist boot refuses a secret source for.  The environment,
 * the OAuth2 HTTP call, the token cache and the clock are seams: the caller
 * passes the environment in as a value.
 */
#ifndef NOC_W26_CREDENTIAL_0A5825_H
#define NOC_W26_CREDENTIAL_0A5825_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* IDP_ENV_PREFIX and the four keys it prefixes. */
#define W26_CREDENTIAL_0A5825_IDP_ENV_PREFIX "GATEWAY_RELAY_IDP_"
#define W26_CREDENTIAL_0A5825_HTTP_TIMEOUT_S 15.0

/* _idp_env(env) -> {"token_url","client_id","client_secret","scope"} with every
 * value stripped (absent -> ""). */
json_t *w26_credential_0a5825_idp_env(const json_t *env);

/* credential_kind(env) -> "idp" when a token_url is configured, else "nous". */
json_t *w26_credential_0a5825_credential_kind(const json_t *env);

/* idp_form(env) -> the client-credentials form body (scope only when set). */
json_t *w26_credential_0a5825_idp_form(const json_t *env);

/* cache_ttl_offset(expires_in) -> max(ttl - 30, 0) where
 * ttl = float(expires_in) when it is a positive int/float else 60.0. */
json_t *w26_credential_0a5825_cache_ttl_offset(const json_t *expires_in);

/* PLANE_CREDENTIAL_ENV_NAMES, the prefix, the key list and the HTTP timeout. */
json_t *w26_credential_0a5825_constants(void);

#ifdef __cplusplus
}
#endif
#endif
