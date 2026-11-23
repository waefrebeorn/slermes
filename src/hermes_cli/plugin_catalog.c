/*
 * plugin_catalog.c — C home for upstream Hermes hermes_cli/plugin_catalog.py.
 * PoP: w12_plugincata_a970ad_is_allowed_image_url @ hermes_cli/plugin_catalog.py:is_allowed_image_url
 * PoP: w12_plugincata_a970ad_is_https_url        @ hermes_cli/plugin_catalog.py:is_https_url
 * PoP: w12_plugincata_a970ad_normalize_repo      @ hermes_cli/plugin_catalog.py:_normalize_repo
 */
#include "noc_w12_plugincata_a970ad.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void lower_ascii(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}

/* Python str.strip() over ASCII whitespace, in place; returns new start. */
static char *py_strip(char *s) {
    size_t n;
    while (*s && isspace((unsigned char)*s)) s++;
    n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    return s;
}

/* Extract urlsplit-style scheme (lowercased) and hostname (lowercased) from a
 * URL. Sets *malformed when an IPv6 authority is unterminated. */
static void parse_scheme_host(const char *url, char *scheme, size_t sch_cap,
                              char *host, size_t host_cap, int *malformed)
{
    const char *colon, *auth, *end;
    size_t sl, n;
    char tmp[600], *at, *h, *port;
    scheme[0] = '\0'; host[0] = '\0'; *malformed = 0;
    colon = strchr(url, ':');
    if (!colon) return;
    sl = (size_t)(colon - url);
    if (sl == 0 || sl >= sch_cap) return;
    memcpy(scheme, url, sl); scheme[sl] = '\0'; lower_ascii(scheme);
    auth = colon + 1;
    if (!(auth[0] == '/' && auth[1] == '/')) return;
    auth += 2;
    end = auth;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    n = 0;
    while (auth < end && n + 1 < sizeof(tmp)) tmp[n++] = *auth++;
    tmp[n] = '\0';
    if (strchr(tmp, '[') && !strchr(tmp, ']')) { *malformed = 1; return; }
    at = strrchr(tmp, '@');
    h = at ? at + 1 : tmp;
    port = strchr(h, ':');
    if (port) *port = '\0';
    n = strlen(h);
    if (n >= host_cap) n = host_cap - 1;
    memcpy(host, h, n); host[n] = '\0'; lower_ascii(host);
}

bool w12_plugincata_a970ad_is_allowed_image_url(const char *url)
{
    char scheme[32], host[600];
    int malformed;
    const char *suffix = ".githubusercontent.com";
    size_t hl, sl;
    if (!url) url = "";
    parse_scheme_host(url, scheme, sizeof(scheme), host, sizeof(host), &malformed);
    if (malformed || strcmp(scheme, "https") || !host[0]) return false;
    if (!strcmp(host, "raw.githubusercontent.com") || !strcmp(host, "github.com"))
        return true;
    hl = strlen(host); sl = strlen(suffix);
    return hl > sl && !strcmp(host + hl - sl, suffix);
}

bool w12_plugincata_a970ad_is_https_url(const char *url)
{
    char scheme[32], host[600];
    int malformed;
    char *h;
    if (!url) url = "";
    parse_scheme_host(url, scheme, sizeof(scheme), host, sizeof(host), &malformed);
    if (malformed || strcmp(scheme, "https")) return false;
    h = py_strip(host);
    return h[0] != '\0';
}

/* _SCP_URL_RE: ^(?:[^@/\s]+@)?([^:/\s]+):(?!//)(.+)$ */
static int scp_match(const char *text, char *host, size_t host_cap,
                     const char **path_out)
{
    size_t n = strlen(text), pos = 0, hstart, hl;
    const char *at = strchr(text, '@');
    if (at) {
        size_t a = (size_t)(at - text);
        size_t i;
        int ok = (a > 0);
        for (i = 0; ok && i < a; i++)
            if (text[i] == '/' || isspace((unsigned char)text[i])) ok = 0;
        if (ok) pos = a + 1;
    }
    hstart = pos;
    while (pos < n && text[pos] != ':' && text[pos] != '/' &&
           !isspace((unsigned char)text[pos])) pos++;
    if (pos == hstart || pos >= n || text[pos] != ':') return 0;
    if (pos + 1 >= n) return 0;                     /* .+ after the colon */
    if (text[pos + 1] == '/' && pos + 2 < n && text[pos + 2] == '/') return 0;
    hl = pos - hstart;
    if (hl >= host_cap) hl = host_cap - 1;
    memcpy(host, text + hstart, hl); host[hl] = '\0';
    *path_out = text + pos + 1;
    return 1;
}

json_t *w12_plugincata_a970ad_normalize_repo(const char *url)
{
    char text[600], host[600] = "", p[600];
    char *t, *ps;
    const char *path = NULL;
    size_t plen;
    json_t *res;

    snprintf(text, sizeof(text), "%s", url ? url : "");
    t = py_strip(text);
    if (scp_match(t, host, sizeof(host), &path)) {
        /* host/path populated */
    } else if (strstr(t, "://")) {
        const char *sep = strstr(t, "://");
        const char *auth = sep + 3, *end = auth;
        char tmp[600], *at, *h, *port;
        size_t k = 0, hl;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        while (auth < end && k + 1 < sizeof(tmp)) tmp[k++] = *auth++;
        tmp[k] = '\0';
        at = strrchr(tmp, '@');
        h = at ? at + 1 : tmp;
        port = strchr(h, ':');
        if (port) *port = '\0';
        hl = strlen(h);
        if (hl >= sizeof(host)) hl = sizeof(host) - 1;
        memcpy(host, h, hl); host[hl] = '\0';
        path = (*end == '/') ? end : "";
    } else {
        host[0] = '\0';
        path = t;
    }

    lower_ascii(host);
    if (!strncmp(host, "www.", 4)) memmove(host, host + 4, strlen(host + 4) + 1);

    snprintf(p, sizeof(p), "%s", path ? path : "");
    ps = p;
    while (*ps == '/') ps++;
    plen = strlen(ps);
    while (plen && ps[plen - 1] == '/') ps[--plen] = '\0';
    if (plen >= 4 && !strcmp(ps + plen - 4, ".git")) ps[plen - 4] = '\0';
    plen = strlen(ps);
    while (plen && ps[plen - 1] == '/') ps[--plen] = '\0';
    lower_ascii(ps);

    if (host[0]) {
        char out[1200];
        snprintf(out, sizeof(out), "%s/%s", host, ps);
        res = json_string(out);
    } else {
        res = json_string(ps);
    }
    return res;
}
