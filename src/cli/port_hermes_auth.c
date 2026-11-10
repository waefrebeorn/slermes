/*
 * port_hermes_auth.c — pure URL/key-shape predicates from hermes_cli/auth.py.
 */

#include "port_hermes_auth.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dup_(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* urlparse(url).hostname lowercased, trailing dot stripped, port dropped.
 * malloc'd; "" for none. */
static char *hostname_lower(const char *url)
{
    if (!url) return dup_("");
    const char *p = url;
    const char *scheme = strstr(p, "://");
    if (scheme) p = scheme + 3;
    size_t n = 0;
    if (p[0] == '[') {
        /* IPv6 in brackets: host runs to the ']' */
        const char *close = strchr(p, ']');
        size_t bail = close ? (size_t)(close - p) : 0;
        if (bail == 0) return dup_("");
        n = bail - 1;
        char *o = (char *)malloc(n + 1);
        if (!o) return NULL;
        for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)p[i + 1]);
        o[n] = '\0';
        return o;
    }
    while (p[n] && p[n] != '/' && p[n] != '?' && p[n] != '#' && p[n] != ':') n++;
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)p[i]);
    o[n] = '\0';
    /* rstrip('.') */
    while (n > 0 && o[n - 1] == '.') { o[n - 1] = '\0'; n--; }
    return o;
}

/* PoP: auth_is_actual_local_base_url @ hermes_cli/auth.py:is_actual_local_base_url */
int auth_is_actual_local_base_url(const char *base_url)
{
    char *h = hostname_lower(base_url ? base_url : "");
    int in = h && (strcmp(h, "localhost") == 0 || strcmp(h, "127.0.0.1") == 0 ||
                   strcmp(h, "::1") == 0 || strcmp(h, "0.0.0.0") == 0);
    free(h);
    return in;
}

/* PoP: auth_normalize_actual_base_url @ hermes_cli/auth.py:normalize_actual_base_url */
char *auth_normalize_actual_base_url(const char *base_url)
{
    const char *raw = base_url ? base_url : "";
    /* str().strip().rstrip('/') */
    size_t len = strlen(raw), s = 0, e = len;
    while (s < e && isspace((unsigned char)raw[s])) s++;
    while (e > s && isspace((unsigned char)raw[e - 1])) e--;
    while (e > s && (raw[e - 1] == '/')) e--;
    if (s == e)
        return dup_(AUTH_DEFAULT_ACTUAL_BASE_URL);

    char *url = (char *)malloc(e - s + 1);
    if (!url) return NULL;
    memcpy(url, raw + s, e - s);
    url[e - s] = '\0';

    char *host = hostname_lower(url);
    /* parsed.path.rstrip('/') */
    const char *scheme2 = strstr(url, "://");
    const char *path = scheme2 ? scheme2 + 3 : url;
    while (*path && *path != '/' && *path != '?' && *path != '#') path++;
    size_t plen = strlen(path ? path : "");
    while (plen > 0 && path[plen - 1] == '/') plen--;

    int path_bare = (plen == 0);
    int host_actual = host && strcmp(host, "api.actual.inc") == 0;
    int local = auth_is_actual_local_base_url(url);
    if (path_bare && (host_actual || local)) {
        char *out = (char *)malloc(strlen(url) + 4);
        if (!out) { free(url); free(host); return NULL; }
        snprintf(out, strlen(url) + 4, "%s/v1", url);
        free(url); free(host);
        return out;
    }
    free(host);
    return url;
}

/* PoP: auth_is_placeholder_shape @ hermes_cli/auth.py:_is_placeholder_shape */
int auth_is_placeholder_shape(const char *value)
{
    if (!value) return 0;
    size_t len = strlen(value);
    char *low = (char *)malloc(len + 1);
    if (!low) return 0;
    for (size_t i = 0; i < len; i++) low[i] = (char)tolower((unsigned char)value[i]);
    low[len] = '\0';

    if (len >= 6 && strncmp(low, "your_", 5) == 0 && len >= 5 + 5 &&
        strcmp(low + len - 5, "_here") == 0) {
        free(low);
        return 1;
    }
    static const char *const PREFIXES[] = { "sk-", "ghp_", "hf_" };
    for (size_t pi = 0; pi < sizeof(PREFIXES) / sizeof(PREFIXES[0]); pi++) {
        size_t pl = strlen(PREFIXES[pi]);
        if (len > pl && strncmp(low, PREFIXES[pi], pl) == 0) {
            int tail_x = 1;
            for (size_t i = pl; i < len; i++)
                if (low[i] != 'x') { tail_x = 0; break; }
            if (tail_x) { free(low); return 1; }
        }
    }
    /* stripped = replace(" ","").replace("-","").replace("_","") */
    int any = 0, allx = 1;
    for (size_t i = 0; i < len; i++) {
        char c = low[i];
        if (c == ' ' || c == '-' || c == '_') continue;
        any = 1;
        if (c != 'x') allx = 0;
    }
    free(low);
    return any && allx;
}

/* PoP: auth_matches_key_prefix @ hermes_cli/auth.py:_matches_key_prefix */
int auth_matches_key_prefix(const char *provider_id, const char *val)
{
    if (!provider_id || !val)
        return 0;
    if (strcmp(provider_id, "openrouter") == 0)
        return strncmp(val, "sk-or-", 6) == 0;
    return 0; /* providers with no declared prefix are fail-open False */
}

/* PoP: auth_looks_like_openrouter_key @ hermes_cli/auth.py:looks_like_openrouter_key */
int auth_looks_like_openrouter_key(const char *value)
{
    const char *v = value ? value : "";
    size_t len = strlen(v), s = 0, e = len;
    while (s < e && isspace((unsigned char)v[s])) s++;
    while (e > s && isspace((unsigned char)v[e - 1])) e--;
    char *stripped = (char *)malloc(e - s + 1);
    if (!stripped) return 0;
    memcpy(stripped, v + s, e - s);
    stripped[e - s] = '\0';
    int r = auth_matches_key_prefix("openrouter", stripped);
    free(stripped);
    return r;
}
