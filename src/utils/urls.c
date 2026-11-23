/*
 * urls.c — C port of upstream Hermes utils/urls.py.
 *
 * PoP: w5urls_hostname     @ utils/urls.py:base_url_hostname
 * PoP: w5urls_path         @ utils/urls.py:base_url_path
 * PoP: w5urls_origin       @ utils/urls.py:base_url_origin
 * PoP: w5urls_same_origin  @ utils/urls.py:same_base_url_origin
 * PoP: w5urls_host_matches @ utils/urls.py:base_url_host_matches
 *
 * Mirrors the tolerant urlparse wrapper (_parse_base_url): a bare
 * host[:port][/path] is parsed by prepending "//". Hostnames/paths are
 * lowercased and trailing dots/slashes stripped exactly as upstream.
 */
#include "noc_w5_urls_e1571a.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char scheme[64];
    char host[1024];
    char path[4096];
    int has_host;
    int has_port;
    int port;
    int port_ok;
} w5urls_parsed_t;

static void lower_ascii(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}
static void rstrip_char(char *s, char c) {
    size_t n = strlen(s);
    while (n && s[n - 1] == c) s[--n] = '\0';
}

/* Trim leading/trailing ASCII whitespace (Python str.strip on our inputs). */
static const char *trim(const char *s, char *buf, size_t cap) {
    if (!s) s = "";
    while (*s && isspace((unsigned char)*s)) s++;
    const char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    if (n >= cap) n = cap - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return buf;
}

static void parse(const char *base_url, w5urls_parsed_t *p) {
    memset(p, 0, sizeof(*p));
    char raw[8192];
    trim(base_url, raw, sizeof(raw));
    if (!raw[0]) return; /* urlparse wrapper returns None for empty input */

    char work[8200];
    if (strstr(raw, "://")) snprintf(work, sizeof(work), "%s", raw);
    else snprintf(work, sizeof(work), "//%s", raw);

    const char *auth;
    const char *sep = strstr(work, "://");
    if (sep) {
        size_t sl = (size_t)(sep - work);
        if (sl > 0 && sl < sizeof(p->scheme)) {
            memcpy(p->scheme, work, sl);
            p->scheme[sl] = '\0';
            lower_ascii(p->scheme);
        }
        auth = sep + 3;
    } else if (work[0] == '/' && work[1] == '/') {
        auth = work + 2;
    } else {
        auth = work + strlen(work);
    }

    const char *aend = auth;
    while (*aend && *aend != '/' && *aend != '?' && *aend != '#') aend++;
    size_t an = (size_t)(aend - auth);
    char netloc[2048];
    if (an >= sizeof(netloc)) an = sizeof(netloc) - 1;
    memcpy(netloc, auth, an);
    netloc[an] = '\0';

    /* path runs from end-of-authority up to query/fragment, lowercased,
     * trailing slashes removed (rstrip("/")). */
    const char *pend = aend;
    while (*pend && *pend != '?' && *pend != '#') pend++;
    size_t pn = (size_t)(pend - aend);
    if (pn >= sizeof(p->path)) pn = sizeof(p->path) - 1;
    memcpy(p->path, aend, pn);
    p->path[pn] = '\0';
    lower_ascii(p->path);
    rstrip_char(p->path, '/');

    if (netloc[0]) {
        const char *hp = netloc;
        const char *at = strrchr(netloc, '@');
        if (at) hp = at + 1; /* drop userinfo */
        const char *portstr = NULL;
        char hbuf[1024];
        if (*hp == '[') {
            const char *close = strchr(hp, ']');
            if (close) {
                size_t hl = (size_t)(close - (hp + 1));
                if (hl >= sizeof(hbuf)) hl = sizeof(hbuf) - 1;
                memcpy(hbuf, hp + 1, hl);
                hbuf[hl] = '\0';
                const char *after = close + 1;
                if (*after == ':') portstr = after + 1;
            } else {
                snprintf(hbuf, sizeof(hbuf), "%s", hp);
            }
        } else {
            const char *col = strrchr(hp, ':');
            if (col) {
                size_t hl = (size_t)(col - hp);
                if (hl >= sizeof(hbuf)) hl = sizeof(hbuf) - 1;
                memcpy(hbuf, hp, hl);
                hbuf[hl] = '\0';
                portstr = col + 1;
            } else {
                snprintf(hbuf, sizeof(hbuf), "%s", hp);
            }
        }
        lower_ascii(hbuf);
        rstrip_char(hbuf, '.');
        if (hbuf[0]) {
            snprintf(p->host, sizeof(p->host), "%s", hbuf);
            p->has_host = 1;
        }
        if (portstr && *portstr) {
            p->has_port = 1;
            char *endp = NULL;
            long v = strtol(portstr, &endp, 10);
            if (endp && *endp == '\0' && v >= 0 && v <= 65535) {
                p->port = (int)v;
                p->port_ok = 1;
            }
        }
    }
}

int w5urls_hostname(const char *base_url, char *out, size_t cap) {
    w5urls_parsed_t p;
    parse(base_url, &p);
    if (out && cap) snprintf(out, cap, "%s", p.has_host ? p.host : "");
    return p.has_host;
}

int w5urls_path(const char *base_url, char *out, size_t cap) {
    w5urls_parsed_t p;
    parse(base_url, &p);
    if (out && cap) snprintf(out, cap, "%s", p.path);
    return 1;
}

int w5urls_origin(const char *base_url, char *scheme, size_t scap,
                  char *host, size_t hcap, int *port) {
    w5urls_parsed_t p;
    parse(base_url, &p);
    if (scheme && scap) scheme[0] = '\0';
    if (host && hcap) host[0] = '\0';
    if (port) *port = 0;
    if (!p.has_host) return 0;
    int effective;
    if (p.has_port) {
        if (!p.port_ok) return 0; /* ValueError -> ("", "", 0) */
        effective = p.port;
    } else if (!strcmp(p.scheme, "https")) {
        effective = 443;
    } else if (!strcmp(p.scheme, "http")) {
        effective = 80;
    } else {
        effective = 0;
    }
    if (scheme && scap) snprintf(scheme, scap, "%s", p.scheme);
    if (host && hcap) snprintf(host, hcap, "%s", p.host);
    if (port) *port = effective;
    return 1;
}

int w5urls_same_origin(const char *a, const char *b) {
    w5urls_parsed_t pa, pb;
    parse(a, &pa);
    parse(b, &pb);
    int oka = pa.has_host && (!pa.has_port || pa.port_ok);
    int okb = pb.has_host && (!pb.has_port || pb.port_ok);
    if (!oka || !okb) return 0;
    int ea = pa.has_port ? pa.port
                         : (!strcmp(pa.scheme, "https") ? 443
                           : !strcmp(pa.scheme, "http") ? 80 : 0);
    int eb = pb.has_port ? pb.port
                         : (!strcmp(pb.scheme, "https") ? 443
                           : !strcmp(pb.scheme, "http") ? 80 : 0);
    return !strcmp(pa.scheme, pb.scheme) && !strcmp(pa.host, pb.host) && ea == eb;
}

int w5urls_host_matches(const char *base_url, const char *domain) {
    char host[1024];
    w5urls_hostname(base_url, host, sizeof(host));
    char dom[1024];
    const char *ret = trim(domain, dom, sizeof(dom));
    lower_ascii(dom);
    rstrip_char(dom, '.');
    (void)ret;
    if (!host[0] || !dom[0]) return 0;
    if (!strcmp(host, dom)) return 1;
    size_t dl = strlen(dom), hl = strlen(host);
    return hl > dl + 1 && host[hl - dl - 1] == '.' &&
           !strcmp(host + hl - dl, dom);
}
