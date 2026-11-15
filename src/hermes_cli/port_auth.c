/* port_auth.c -- pure helpers ported from hermes_cli/auth.py.
 *
 * PoP: st_nonempty_str               @ hermes_cli/auth.py:_nonempty_str
 * PoP: st_is_placeholder_shape       @ hermes_cli/auth.py:_is_placeholder_shape
 * PoP: st_has_usable_secret          @ hermes_cli/auth.py:has_usable_secret
 * PoP: st_matches_key_prefix         @ hermes_cli/auth.py:_matches_key_prefix
 * PoP: st_looks_like_openrouter_key  @ hermes_cli/auth.py:looks_like_openrouter_key
 * PoP: st_usable_declared_secret     @ hermes_cli/auth.py:_usable_declared_secret
 * PoP: st_is_actual_local_base_url   @ hermes_cli/auth.py:is_actual_local_base_url
 * PoP: st_normalize_actual_base_url  @ hermes_cli/auth.py:normalize_actual_base_url
 *
 * Placeholder tables verbatim from hermes_cli/auth.py:278-286; the default base
 * URL is a parameter (locales/en-style seam) rather than a shipped literal.
 * `logger.warning` in _usable_declared_secret is a seam: it sets *warned.
 */
#include "port_auth.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void str_lower(char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static void str_rstrip_dot(char *s)
{
    size_t n = strlen(s);
    while (n && s[n - 1] == '.')
        s[--n] = '\0';
}

/* ── _nonempty_str ──────────────────────────────────────────────────────── */
int st_nonempty_str(const char *value)
{
    if (!value)
        return 0;
    for (; *value == ' ' || *value == '\t' || *value == '\n' || *value == '\r'; value++)
        ;
    return *value != '\0';
}

/* ── _is_placeholder_shape ──────────────────────────────────────────────── */
static const char *const PLACEHOLDER_KEY_PREFIXES[] = {"sk-", "ghp_", "hf_"};

int st_is_placeholder_shape(const char *value)
{
    if (!value)
        return 0;
    char lowered[1024];
    snprintf(lowered, sizeof(lowered), "%s", value);
    str_lower(lowered);

    size_t n = strlen(lowered);
    if (strncmp(lowered, "your_", 5) == 0 && n >= 10 &&
        strcmp(lowered + n - 5, "_here") == 0)
        return 1;

    for (size_t k = 0; k < sizeof(PLACEHOLDER_KEY_PREFIXES)
                            / sizeof(PLACEHOLDER_KEY_PREFIXES[0]); k++) {
        const char *prefix = PLACEHOLDER_KEY_PREFIXES[k];
        if (strncmp(lowered, prefix, strlen(prefix)) != 0)
            continue;
        const char *tail = lowered + strlen(prefix);
        if (!*tail)
            continue;
        int all_x = 1;
        for (const char *p = tail; *p; p++) {
            if (*p != 'x') {
                all_x = 0;
                break;
            }
        }
        if (all_x)
            return 1;
    }

    /* stripped of spaces, dashes and underscores; non-empty and all 'x' */
    char stripped[1024];
    size_t w = 0;
    for (const char *p = lowered; *p && w + 1 < sizeof(stripped); p++) {
        if (*p == ' ' || *p == '-' || *p == '_')
            continue;
        stripped[w++] = *p;
    }
    stripped[w] = '\0';
    if (!w)
        return 0;
    for (size_t i = 0; i < w; i++) {
        if (stripped[i] != 'x')
            return 0;
    }
    return 1;
}

/* ── has_usable_secret ──────────────────────────────────────────────────── */
static const char *const PLACEHOLDER_SECRET_VALUES[] = {
    "*", "**", "***", "changeme", "your_api_key", "your_api_key_here",
    "your-api-key", "placeholder", "example", "dummy", "null", "none",
};

int st_has_usable_secret(const char *value, int min_length)
{
    if (!value)
        return 0;
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value);
    /* strip() */
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    /* len() in Python counts characters; secrets here are ASCII. */
    if ((int)strlen(s) < min_length)
        return 0;

    char lowered[2048];
    snprintf(lowered, sizeof(lowered), "%s", s);
    str_lower(lowered);
    for (size_t k = 0; k < sizeof(PLACEHOLDER_SECRET_VALUES)
                            / sizeof(PLACEHOLDER_SECRET_VALUES[0]); k++) {
        if (strcmp(lowered, PLACEHOLDER_SECRET_VALUES[k]) == 0)
            return 0;
    }
    return !st_is_placeholder_shape(s);
}

/* ── _matches_key_prefix / looks_like_openrouter_key ────────────────────── */
static const char *const KNOWN_PREFIX_OPENROUTER[] = {"sk-or-"};

int st_matches_key_prefix(const char *provider_id, const char *val)
{
    if (!provider_id || !val)
        return 0;
    if (strcmp(provider_id, "openrouter") != 0)
        return 0; /* the only provider with a declared prefix; rest are fail-open */
    for (size_t k = 0; k < sizeof(KNOWN_PREFIX_OPENROUTER)
                            / sizeof(KNOWN_PREFIX_OPENROUTER[0]); k++) {
        if (strncmp(val, KNOWN_PREFIX_OPENROUTER[k],
                    strlen(KNOWN_PREFIX_OPENROUTER[k])) == 0)
            return 1;
    }
    return 0;
}

int st_looks_like_openrouter_key(const char *value)
{
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value ? value : "");
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';
    return st_matches_key_prefix("openrouter", s);
}

/* ── _usable_declared_secret ────────────────────────────────────────────── */
int st_usable_declared_secret(const char *provider_id, const char *value,
                              const char *source, char *out, size_t cap,
                              int *warned)
{
    (void)source; /* only used by the warning seam */
    if (out && cap)
        out[0] = '\0'; /* defined on every path */
    char cleaned[2048];
    snprintf(cleaned, sizeof(cleaned), "%s", value ? value : "");
    char *s = cleaned;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    if (!st_has_usable_secret(s, 4))
        return 0;
    if (strcmp(provider_id, "openrouter") == 0 &&
        !st_matches_key_prefix("openrouter", s)) {
        if (warned)
            *warned = 1;
        return 0;
    }
    if (out && cap)
        snprintf(out, cap, "%s", s);
    return 1;
}

/* ── urlparse hostname ──────────────────────────────────────────────────── */
/* Minimal netloc host extractor matching urllib.parse.urlparse().hostname:
 * lowercase, brackets stripped for IPv6, userinfo and port removed. Returns an
 * empty string when the URL has no netloc (urlparse().hostname is None). */
static void url_hostname(const char *url, char *out, size_t cap)
{
    if (!cap) {
        return;
    }
    out[0] = '\0';
    const char *p = url ? url : "";
    const char *authority = NULL;

    const char *scheme_end = strstr(p, "://");
    if (scheme_end) {
        authority = scheme_end + 3;
    } else if (p[0] == '/' && p[1] == '/') {
        authority = p + 2;
    } else {
        return; /* no netloc -> hostname is None */
    }

    size_t n = 0;
    for (const char *q = authority; *q && *q != '/' && *q != '?' && *q != '#'; q++)
        n++;

    /* strip userinfo: everything up to the last '@' in the authority */
    const char *host = authority;
    for (size_t i = 0; i < n; i++) {
        if (authority[i] == '@')
            host = authority + i + 1;
    }
    n = (size_t)(authority + n - host);

    if (host[0] == '[') {
        const char *close = memchr(host, ']', n);
        if (!close)
            return;
        size_t hn = (size_t)(close - host - 1);
        if (hn >= cap)
            hn = cap - 1;
        memcpy(out, host + 1, hn);
        out[hn] = '\0';
    } else {
        size_t hn = 0;
        while (hn < n && host[hn] != ':')
            hn++;
        if (hn >= cap)
            hn = cap - 1;
        memcpy(out, host, hn);
        out[hn] = '\0';
    }
    str_lower(out);
    str_rstrip_dot(out);
}

/* urlparse().path, or "" when there is no netloc and no path separator. */
static void url_path(const char *url, char *out, size_t cap)
{
    if (!cap)
        return;
    out[0] = '\0';
    const char *p = url ? url : "";
    const char *scheme_end = strstr(p, "://");
    const char *start;
    if (scheme_end) {
        start = scheme_end + 3;
        while (*start && *start != '/' && *start != '?' && *start != '#')
            start++;
    } else if (p[0] == '/' && p[1] == '/') {
        start = p + 2;
        while (*start && *start != '/' && *start != '?' && *start != '#')
            start++;
    } else {
        start = p;
    }
    const char *q = start;
    while (*q && *q != '?' && *q != '#')
        q++;
    size_t n = (size_t)(q - start);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, start, n);
    out[n] = '\0';
}

/* ── is_actual_local_base_url ───────────────────────────────────────────── */
int st_is_actual_local_base_url(const char *base_url)
{
    char host[512];
    url_hostname(base_url, host, sizeof(host));
    return strcmp(host, "localhost") == 0 || strcmp(host, "127.0.0.1") == 0
        || strcmp(host, "::1") == 0 || strcmp(host, "0.0.0.0") == 0;
}

/* ── normalize_actual_base_url ──────────────────────────────────────────── */
void st_normalize_actual_base_url(const char *base_url, const char *default_url,
                                  char *out, size_t cap)
{
    char url[2048];
    snprintf(url, sizeof(url), "%s", base_url ? base_url : "");
    char *s = url;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;
    size_t n = strlen(s);
    while (n && s[n - 1] == '/')
        s[--n] = '\0';
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n'
                 || s[n - 1] == '\r'))
        s[--n] = '\0';

    if (!*s) {
        if (cap)
            snprintf(out, cap, "%s", default_url ? default_url : "");
        return;
    }

    char host[512], path[1024];
    url_hostname(s, host, sizeof(host));
    url_path(s, path, sizeof(path));
    size_t pn = strlen(path);
    while (pn && path[pn - 1] == '/')
        path[--pn] = '\0';

    int bare = (pn == 0);
    if (bare && (strcmp(host, "api.actual.inc") == 0
                 || st_is_actual_local_base_url(s))) {
        if (cap)
            snprintf(out, cap, "%s/v1", s);
        return;
    }
    if (cap)
        snprintf(out, cap, "%s", s);
}
