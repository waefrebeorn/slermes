/* Slermes C port — tools/url_safety.py (pure URL-safety helpers). */

#include "url_safety_helpers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Faithful copy of _TRUSTED_PRIVATE_IP_HOSTS (url_safety.py:174). */
static const char *URL_SAFETY_TRUSTED_HOSTS[] = {
    "multimedia.nt.qq.com.cn",
    NULL,
};

/* PoP: tools_url_safety_allows_private_ip_resolution @ tools/url_safety.py:_allows_private_ip_resolution */
bool tools_url_safety_allows_private_ip_resolution(const char *hostname, const char *scheme)
{
    if (!scheme || strcmp(scheme, "https") != 0) return false;
    if (!hostname) return false;
    for (int i = 0; URL_SAFETY_TRUSTED_HOSTS[i]; i++)
        if (strcmp(hostname, URL_SAFETY_TRUSTED_HOSTS[i]) == 0) return true;
    return false;
}

/* ── Sensitive query-param detection ───────────────────────────────────────
 * PoP: sensitive_query_param_name / has_sensitive_query_params @ tools/url_safety.py
 * The credential-bearing param names are kept deliberately narrow (see the
 * module docstring). Compared case-insensitively against the unquoted key. */

static const char *URL_SAFETY_SENSITIVE_PARAMS[] = {
    "access_token", "api_key", "apikey", "auth_token", "authorization",
    "awsaccesskeyid", "client_secret", "credential", "credentials", "jwt",
    "password", "passwd", "secret", "session_id", "signature", "token",
    "x_amz_security_token", "x_amz_signature", "x-amz-security-token",
    "x-amz-signature", NULL,
};

/* percent-decode in place (e.g. %20 -> space, %2C -> comma). Returns length. */
static size_t url_pct_decode(char *s)
{
    size_t j = 0, i = 0;
    while (s[i]) {
        if (s[i] == '%' && s[i+1] && s[i+2]) {
            int hi = s[i+1], lo = s[i+2];
            int h = (hi >= '0' && hi <= '9') ? hi - '0'
                  : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                  : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int l = (lo >= '0' && lo <= '9') ? lo - '0'
                  : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                  : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (h >= 0 && l >= 0) { s[j++] = (char)((h << 4) | l); i += 3; continue; }
        }
        s[j++] = s[i++];
    }
    s[j] = '\0';
    return j;
}

static int is_sensitive_param(const char *key_lower)
{
    for (int i = 0; URL_SAFETY_SENSITIVE_PARAMS[i]; i++)
        if (strcmp(key_lower, URL_SAFETY_SENSITIVE_PARAMS[i]) == 0) return 1;
    return 0;
}

/* PoP: sensitive_query_param_name @ tools/url_safety.py:sensitive_query_param_name
 * Wrapper — canonical implementation is in url_safety.c */
char *sensitive_query_param_name(const char *url);  /* decl only, impl in url_safety.c */

/* PoP: has_sensitive_query_params */
bool tools_url_safety_has_sensitive_query_params(const char *url)
{
    char *r = sensitive_query_param_name(url);
    if (r) { free(r); return true; }
    return false;
}
