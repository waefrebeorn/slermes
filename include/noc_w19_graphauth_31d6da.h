/* noc_w19_graphauth_31d6da.h - public C API for plugins/teams_pipeline/graph_auth.py port.
 * Token acquisition (httpx POST) stays Python-side; every credential/token decision function is here. */
#ifndef NOC_W19_GRAPHAUTH_31D6DA_H
#define NOC_W19_GRAPHAUTH_31D6DA_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_graphauth_31d6da_format_graph_error @ plugins/teams_pipeline/graph_auth.py:format_graph_error
 * Render Graph's {"error": {"code", "message"}} (or a bare-string error) as "code: message";
 * NULL when unusable. Returns a malloc'd string (caller frees). */
char *w19_graphauth_31d6da_format_graph_error(const json_t *error);

/* PoP: w19_graphauth_31d6da_credentials_from_env @ plugins/teams_pipeline/graph_auth.py:GraphCredentials.from_env
 * `env_map` is the env mapping; answers null when `required` is false and credentials are missing,
 * {"error": "Missing Microsoft Graph configuration: ..."} when they are missing and required,
 * else the normalized credential object. */
json_t *w19_graphauth_31d6da_credentials_from_env(const json_t *env_map, int required);

/* PoP: w19_graphauth_31d6da_token_url @ plugins/teams_pipeline/graph_auth.py:GraphCredentials.token_url
 * f"{authority_url.rstrip('/')}/{tenant_id.strip().strip('/')}/oauth2/v2.0/token". Malloc'd. */
char *w19_graphauth_31d6da_token_url(const char *authority_url, const char *tenant_id);

/* PoP: w19_graphauth_31d6da_token_is_expired @ plugins/teams_pipeline/graph_auth.py:CachedAccessToken.is_expired
 * expires_at <= now + max(0, int(skew_seconds)). `now` is the injected clock. */
int w19_graphauth_31d6da_token_is_expired(double expires_at, double now, int skew_seconds);

/* PoP: w19_graphauth_31d6da_token_expires_in @ plugins/teams_pipeline/graph_auth.py:CachedAccessToken.expires_in_seconds
 * max(0, int(expires_at - now)). */
json_t *w19_graphauth_31d6da_token_expires_in(double expires_at, double now);

/* PoP: w19_graphauth_31d6da_fresh_cached @ plugins/teams_pipeline/graph_auth.py:MicrosoftGraphTokenProvider._fresh_cached
 * The cached token is usable unless it expires within the provider's skew. */
int w19_graphauth_31d6da_fresh_cached(int has_cached, double expires_at, double now, int skew_seconds);

/* PoP: w19_graphauth_31d6da_inspect_health @ plugins/teams_pipeline/graph_auth.py:MicrosoftGraphTokenProvider.inspect_token_health
 * The health report for the given credentials and (optional) cached token, with `now` injected. */
json_t *w19_graphauth_31d6da_inspect_health(const json_t *credentials, int has_cached,
                                            double expires_at, double now, int skew_seconds);

/* PoP: w19_graphauth_31d6da_extract_error_detail @ plugins/teams_pipeline/graph_auth.py:_extract_error_detail
 * Best detail from a token-endpoint error body: error_description, then the Graph-style error
 * object/string, then a bare code, then raw text. `is_json` is False when response.json() raised.
 * Returns a malloc'd string (caller frees). */
char *w19_graphauth_31d6da_extract_error_detail(int is_json, const json_t *payload, const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_GRAPHAUTH_31D6DA_H */
