#ifndef HERMES_DASHBOARD_AUTH_COOKIES_H
#define HERMES_DASHBOARD_AUTH_COOKIES_H

/*
 * dashboard_auth_cookies.h — C11 port of hermes_cli/dashboard_auth/cookies.py.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bare cookie names. */
extern const char *DASH_SESSION_AT_COOKIE;
extern const char *DASH_SESSION_RT_COOKIE;
extern const char *DASH_PKCE_COOKIE;
extern const char *DASH_SSO_ATTEMPT_COOKIE;

typedef struct {
    int httponly;
    const char *samesite;
    const char *path;
    int secure;
} dash_cookie_attrs_t;

/* Pure helpers. */
void dash_resolved_name(const char *bare, int use_https, const char *prefix,
                         char *out, size_t out_cap);
const char *dash_cookie_path(const char *prefix);
void dash_common_attrs(int use_https, const char *prefix,
                        dash_cookie_attrs_t *attrs);
int dash_detect_https(const char *scheme);
const char *dash_read_with_fallback(const char *bare,
                                     const char *(*cookies_get)(const char *name));

/* set/clear/read builders (emit Set-Cookie directives). Caller frees strings. */
int dash_set_session_cookies(char *out[], int out_cap,
                              const char *access_token,
                              const char *refresh_token,
                              int access_token_expires_in,
                              int use_https, const char *prefix);
int dash_clear_session_cookies(char *out[], int out_cap, const char *prefix);
char *dash_set_pkce_cookie(const char *payload, int use_https, const char *prefix);
int dash_clear_pkce_cookie(char *out[], int out_cap, const char *prefix);
char *dash_set_sso_attempt_cookie(int use_https, const char *prefix);
int dash_clear_sso_attempt_cookie(char *out[], int out_cap, const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_DASHBOARD_AUTH_COOKIES_H */
