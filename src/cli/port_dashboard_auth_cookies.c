/*
 * port_dashboard_auth_cookies.c — Faithful C11 port of
 * hermes_cli/dashboard_auth/cookies.py
 *
 * Pure cookie-name / cookie-attribute helpers for dashboard auth.
 *
 * The Python module operates on FastAPI Request/Response objects.  In C we
 * model the pure logic (name resolution, path/attribute computation, variant
 * emission) directly; the set/read/clear operations are expressed as
 * cookie-directive builders so they are testable without an HTTP stack.
 *
 * Angel-coder note: detect_https() takes the scheme string directly (the
 * Python reads request.url.scheme).  All other helpers are pure string logic.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dashboard_auth_cookies.h"

/* Bare cookie names — single source of truth for naming. */
const char *DASH_SESSION_AT_COOKIE = "hermes_session_at";
const char *DASH_SESSION_RT_COOKIE = "hermes_session_rt";
const char *DASH_PKCE_COOKIE = "hermes_session_pkce";
const char *DASH_SSO_ATTEMPT_COOKIE = "hermes_sso_attempt";

/* Name variants, sorted most-strict first. */
static const char *G_NAME_VARIANTS[] = {"__Host-", "__Secure-", ""};
static const int G_NAME_VARIANTS_N = 3;

/* Max-Age constants. */
#define DASH_RT_MAX_AGE 2592000       /* 30 days */
#define DASH_PKCE_MAX_AGE 600         /* 10 minutes */
#define DASH_SSO_ATTEMPT_MAX_AGE 60   /* 1 minute */

/* PoP: dash_resolved_name @ hermes_cli/dashboard_auth/cookies.py:_resolved_name */
void dash_resolved_name(const char *bare, int use_https, const char *prefix,
                         char *out, size_t out_cap)
{
    if (!use_https) {
        snprintf(out, out_cap, "%s", bare ? bare : "");
        return;
    }
    if (prefix && prefix[0] != '\0') {
        snprintf(out, out_cap, "__Secure-%s", bare ? bare : "");
    } else {
        snprintf(out, out_cap, "__Host-%s", bare ? bare : "");
    }
}

/* PoP: dash_cookie_path @ hermes_cli/dashboard_auth/cookies.py:_cookie_path */
const char *dash_cookie_path(const char *prefix)
{
    return (prefix && prefix[0] != '\0') ? prefix : "/";
}

/* PoP: dash_common_attrs @ hermes_cli/dashboard_auth/cookies.py:_common_attrs */
void dash_common_attrs(int use_https, const char *prefix,
                        dash_cookie_attrs_t *attrs)
{
    attrs->httponly = 1;
    attrs->samesite = "lax";
    attrs->path = dash_cookie_path(prefix);
    attrs->secure = use_https ? 1 : 0;
}

/* PoP: dash_detect_https @ hermes_cli/dashboard_auth/cookies.py:detect_https */
int dash_detect_https(const char *scheme)
{
    return scheme && strcmp(scheme, "https") == 0;
}

/* PoP: dash_read_with_fallback @ hermes_cli/dashboard_auth/cookies.py:_read_with_fallback */
/* Tries each name variant; returns the first non-NULL value found.
 * cookies_get(variant_name) must return the cookie value or NULL. */
const char *dash_read_with_fallback(const char *bare,
                                     const char *(*cookies_get)(const char *name))
{
    if (!bare || !cookies_get) return NULL;
    for (int i = 0; i < G_NAME_VARIANTS_N; i++) {
        char name[128];
        snprintf(name, sizeof(name), "%s%s", G_NAME_VARIANTS[i], bare);
        const char *v = cookies_get(name);
        if (v) return v;
    }
    return NULL;
}

/* ---- set/clear/read builders (emit cookie directives as strings) ---- */

/* PoP: dash_set_session_cookies @ hermes_cli/dashboard_auth/cookies.py:set_session_cookies */
/* Appends Set-Cookie directives to out (caller frees). Returns count. */
int dash_set_session_cookies(char *out[], int out_cap,
                              const char *access_token,
                              const char *refresh_token,
                              int access_token_expires_in,
                              int use_https, const char *prefix)
{
    int n = 0;
    dash_cookie_attrs_t attrs;
    dash_common_attrs(use_https, prefix, &attrs);
    char name[128];

    /* AT cookie */
    dash_resolved_name(DASH_SESSION_AT_COOKIE, use_https, prefix, name, sizeof(name));
    if (n < out_cap) {
        char *s = NULL;
        asprintf(&s, "%s=%s; Path=%s; HttpOnly; SameSite=Lax%s; Max-Age=%d",
                 name, access_token ? access_token : "",
                 attrs.path, attrs.secure ? "; Secure" : "",
                 access_token_expires_in);
        out[n++] = s;
    }

    /* RT cookie only when refresh_token is non-empty (Contract v1). */
    if (refresh_token && refresh_token[0] != '\0') {
        dash_resolved_name(DASH_SESSION_RT_COOKIE, use_https, prefix, name, sizeof(name));
        if (n < out_cap) {
            char *s = NULL;
            asprintf(&s, "%s=%s; Path=%s; HttpOnly; SameSite=Lax%s; Max-Age=%d",
                     name, refresh_token, attrs.path, attrs.secure ? "; Secure" : "",
                     DASH_RT_MAX_AGE);
            out[n++] = s;
        }
    }
    return n;
}

/* PoP: dash_clear_session_cookies @ hermes_cli/dashboard_auth/cookies.py:clear_session_cookies */
int dash_clear_session_cookies(char *out[], int out_cap, const char *prefix)
{
    int n = 0;
    const char *path = dash_cookie_path(prefix);
    const char *cookies[] = {DASH_SESSION_AT_COOKIE, DASH_SESSION_RT_COOKIE};
    for (int c = 0; c < 2 && n < out_cap; c++) {
        for (int i = 0; i < G_NAME_VARIANTS_N && n < out_cap; i++) {
            char *s = NULL;
            asprintf(&s, "%s%s=; Path=%s; HttpOnly; SameSite=Lax; Max-Age=0",
                     G_NAME_VARIANTS[i], cookies[c], path);
            out[n++] = s;
        }
    }
    return n;
}

/* PoP: dash_set_pkce_cookie @ hermes_cli/dashboard_auth/cookies.py:set_pkce_cookie */
char *dash_set_pkce_cookie(const char *payload, int use_https, const char *prefix)
{
    dash_cookie_attrs_t attrs;
    dash_common_attrs(use_https, prefix, &attrs);
    char name[128];
    dash_resolved_name(DASH_PKCE_COOKIE, use_https, prefix, name, sizeof(name));
    char *s = NULL;
    asprintf(&s, "%s=%s; Path=%s; HttpOnly; SameSite=Lax%s; Max-Age=%d",
             name, payload ? payload : "", attrs.path,
             attrs.secure ? "; Secure" : "", DASH_PKCE_MAX_AGE);
    return s;
}

/* PoP: dash_clear_pkce_cookie @ hermes_cli/dashboard_auth/cookies.py:clear_pkce_cookie */
int dash_clear_pkce_cookie(char *out[], int out_cap, const char *prefix)
{
    int n = 0;
    const char *path = dash_cookie_path(prefix);
    for (int i = 0; i < G_NAME_VARIANTS_N && n < out_cap; i++) {
        char *s = NULL;
        asprintf(&s, "%s%s=; Path=%s; HttpOnly; SameSite=Lax; Max-Age=0",
                 G_NAME_VARIANTS[i], DASH_PKCE_COOKIE, path);
        out[n++] = s;
    }
    return n;
}

/* PoP: dash_set_sso_attempt_cookie @ hermes_cli/dashboard_auth/cookies.py:set_sso_attempt_cookie */
char *dash_set_sso_attempt_cookie(int use_https, const char *prefix)
{
    dash_cookie_attrs_t attrs;
    dash_common_attrs(use_https, prefix, &attrs);
    char name[128];
    dash_resolved_name(DASH_SSO_ATTEMPT_COOKIE, use_https, prefix, name, sizeof(name));
    char *s = NULL;
    asprintf(&s, "%s=1; Path=%s; HttpOnly; SameSite=Lax%s; Max-Age=%d",
             name, attrs.path, attrs.secure ? "; Secure" : "", DASH_SSO_ATTEMPT_MAX_AGE);
    return s;
}

/* PoP: dash_clear_sso_attempt_cookie @ hermes_cli/dashboard_auth/cookies.py:clear_sso_attempt_cookie */
int dash_clear_sso_attempt_cookie(char *out[], int out_cap, const char *prefix)
{
    int n = 0;
    const char *path = dash_cookie_path(prefix);
    for (int i = 0; i < G_NAME_VARIANTS_N && n < out_cap; i++) {
        char *s = NULL;
        asprintf(&s, "%s%s=; Path=%s; HttpOnly; SameSite=Lax; Max-Age=0",
                 G_NAME_VARIANTS[i], DASH_SSO_ATTEMPT_COOKIE, path);
        out[n++] = s;
    }
    return n;
}
