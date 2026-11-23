#ifndef NOC_W8_GITHUBAPI_45460F_H
#define NOC_W8_GITHUBAPI_45460F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_githubapi_45460f_token_from_env @ hermes_cli/github_api.py:github_token_from_env
 * First non-blank env token (GITHUB_TOKEN then GH_TOKEN), trimmed.
 * Returns a malloc'd string or NULL (None); caller frees. */
char *w8_githubapi_45460f_token_from_env(const struct json_t *env);

/* PoP: w8_githubapi_45460f_header_int @ hermes_cli/github_api.py:_header_int
 * int(str(headers.get(name, "")).strip()), or JSON null on failure. */
struct json_t *w8_githubapi_45460f_header_int(const struct json_t *headers, const char *name);

/* PoP: w8_githubapi_45460f_describe_failure @ hermes_cli/github_api.py:describe_github_failure
 * `spec` describes the exception: kind=http|url|url_timeout|timeout|other,
 * status, headers, authenticated, now, reason, message. Returns a malloc'd
 * one-line message; caller frees. `now` must be supplied for rate-limit cases. */
char *w8_githubapi_45460f_describe_failure(const struct json_t *spec);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_GITHUBAPI_45460F_H */
