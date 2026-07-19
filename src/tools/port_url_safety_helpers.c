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

/* PoP: _allows_private_ip_resolution @ tools/url_safety.py:_allows_private_ip_resolution */
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

/* Return a malloc'd copy of the first sensitive query-param NAME (the
 * percent-decoded key, matching Python's unquote) if url carries one with a
 * non-empty value; otherwise NULL. Caller frees. */
char *tools_url_safety_sensitive_query_param_name(const char *url)
{
    if (!url || !*url) return NULL;
    if (!strchr(url, '?')) return NULL;

    /* must be http/https */
    const char *colon = strchr(url, ':');
    if (colon && (size_t)(colon - url) < 7) {
        size_t sl = (size_t)(colon - url);
        if (sl != 4 && sl != 5) return NULL;
        if ((sl == 4 && strncmp(url, "http", 4) != 0) ||
            (sl == 5 && strncmp(url, "https", 5) != 0)) return NULL;
    } else {
        return NULL;
    }

    const char *q = strchr(url, '?');
    if (!q[1]) return NULL;
    const char *query = q + 1;
    size_t qlen = strlen(query);
    char *buf = malloc(qlen + 1);
    memcpy(buf, query, qlen + 1);

    char *found = NULL;
    char *pair = strtok(buf, "&");
    while (pair && !found) {
        char *eq = strchr(pair, '=');
        char *key = pair;
        char *val = NULL;
        if (eq) { *eq = '\0'; val = eq + 1; }
        /* decode key, lowercase */
        url_pct_decode(key);
        size_t kl = strlen(key);
        char *klow = malloc(kl + 1);
        for (size_t i = 0; i <= kl; i++)
            klow[i] = (char)tolower((unsigned char)key[i]);
        int val_nonempty = val && val[0] != '\0';
        if (is_sensitive_param(klow) && val_nonempty) {
            found = strdup(key); /* percent-decoded key, matches Python unquote */
        }
        free(klow);
        pair = strtok(NULL, "&");
    }
    free(buf);
    return found;
}

/* PoP: has_sensitive_query_params */
bool tools_url_safety_has_sensitive_query_params(const char *url)
{
    char *r = tools_url_safety_sensitive_query_param_name(url);
    if (r) { free(r); return true; }
    return false;
}
