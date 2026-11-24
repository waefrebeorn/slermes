/*
 * cli_browser_gateway.c — C home for upstream hermes_cli/cli_browser_gateway.py.
 *
 * PoP: w18_clibrowser_296fc7_gateway_is_remote @ hermes_cli/cli_browser_gateway.py:_gateway_is_remote
 * PoP: w18_clibrowser_296fc7_normalize_cdp_url @ hermes_cli/cli_browser_gateway.py:_normalize_cdp_url
 *
 * Self-contained, libjson-only.  A small urlparse-compatible parser covers the
 * scheme/host/port/path pieces these two functions inspect.  No I/O: the
 * gateway URL arrives as an explicit seam argument rather than the process env.
 */
#include "noc_w18_clibrowser_296fc7.h"
#include "hermes_json.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- small helpers (NEVER name a helper `dup`) ---------- */
static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *substrdup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}
static char *ascii_lower(const char *s) {
    char *p = dupstr(s ? s : "");
    if (p) for (char *q = p; *q; ++q) *q = (char)tolower((unsigned char)*q);
    return p;
}
/* Python str.strip() over the ASCII whitespace domain. */
static char *py_strip(const char *s) {
    const char *a = s ? s : "";
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    return substrdup(a, (size_t)(b - a));
}

typedef struct { char *p; size_t n, cap; } buf_t;
static void bput(buf_t *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->n + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n; b->p[b->n] = '\0';
}
static void bputs(buf_t *b, const char *s) { bput(b, s, strlen(s ? s : "")); }

/* ---------- urlparse-compatible subset ---------- */
typedef struct {
    char *scheme;    /* lowercased */
    char *netloc;    /* verbatim authority (userinfo@host:port) */
    char *hostname;  /* lowercased, no brackets */
    char *path;      /* verbatim (starts at '/', ends before '?'/'#') */
    int  has_port;
    int  port;
    int  port_error; /* parsed.port raised ValueError */
} url_t;

static void url_free(url_t *u) {
    free(u->scheme); free(u->netloc); free(u->hostname); free(u->path);
    memset(u, 0, sizeof(*u));
}

/* Find the authority start: after "://" (if present) or after "//". */
static const char *authority_start(const char *s) {
    const char *d = strstr(s, "://");
    if (d) return d + 3;
    if (s[0] == '/' && s[1] == '/') return s + 2;
    return NULL;
}

static int parse_port_text(const char *t) {
    /* urlsplit .port: digits only, 0..65535; empty -> no port. */
    if (!t || !*t) return -2;            /* no port present */
    for (const char *p = t; *p; ++p) if (!isdigit((unsigned char)*p)) return -1;
    errno = 0;
    long v = strtol(t, NULL, 10);
    if (errno || v < 0 || v > 65535) return -1;
    return (int)v;
}

static void parse_url(const char *input, url_t *u) {
    memset(u, 0, sizeof(*u));
    const char *auth = authority_start(input);
    if (!auth) { u->scheme = dupstr(""); u->hostname = dupstr(""); u->path = dupstr(""); return; }

    const char *d = strstr(input, "://");
    if (d) {
        char *raw = substrdup(input, (size_t)(d - input));
        u->scheme = ascii_lower(raw);
        free(raw);
    } else {
        u->scheme = dupstr("");
    }

    const char *aend = auth + strcspn(auth, "/?#");
    u->netloc = substrdup(auth, (size_t)(aend - auth));

    const char *pend = aend + strcspn(aend, "?#");
    u->path = substrdup(aend, (size_t)(pend - aend));

    const char *hostport = u->netloc;
    const char *at = strrchr(hostport, '@');
    if (at) hostport = at + 1;
    char *host = NULL;
    const char *porttxt = NULL;
    if (*hostport == '[') {
        const char *close = strchr(hostport, ']');
        if (close) {
            host = substrdup(hostport + 1, (size_t)(close - hostport - 1));
            if (close[1] == ':' && close[2]) porttxt = close + 2;
        } else {
            host = substrdup(hostport, strlen(hostport));
        }
    } else {
        const char *colon = strrchr(hostport, ':');
        if (colon) { host = substrdup(hostport, (size_t)(colon - hostport)); porttxt = colon + 1; }
        else host = substrdup(hostport, strlen(hostport));
    }
    u->hostname = ascii_lower(host ? host : "");
    free(host);

    int pv = parse_port_text(porttxt);
    if (pv == -1) u->port_error = 1;
    else if (pv >= 0) { u->has_port = 1; u->port = pv; }
}

static int scheme_ok(const char *scheme) {
    return !strcmp(scheme, "http") || !strcmp(scheme, "https") ||
           !strcmp(scheme, "ws") || !strcmp(scheme, "wss");
}
static int default_port(const char *scheme) {
    return (!strcmp(scheme, "https") || !strcmp(scheme, "wss")) ? 443 : 80;
}

int w18_clibrowser_296fc7_gateway_is_remote(const char *env_value) {
    char *raw = py_strip(env_value);
    if (!raw || !*raw) { free(raw); return 0; }
    url_t u; parse_url(raw, &u);
    int remote = strcmp(u.hostname ? u.hostname : "", "127.0.0.1") != 0 &&
                 strcmp(u.hostname ? u.hostname : "", "localhost") != 0 &&
                 strcmp(u.hostname ? u.hostname : "", "::1") != 0;
    url_free(&u);
    free(raw);
    return remote;
}

struct json_t *w18_clibrowser_296fc7_normalize_cdp_url(const char *cdp_url) {
    const char *in = cdp_url ? cdp_url : "";
    char *full;
    if (!strstr(in, "://")) {
        buf_t b = {0}; bputs(&b, "http://"); bputs(&b, in);
        full = b.p ? b.p : dupstr("");
    } else {
        full = dupstr(in);
    }

    url_t u; parse_url(full, &u);
    struct json_t *out = NULL;

    if (!scheme_ok(u.scheme ? u.scheme : "")) goto done;
    if (u.port_error) goto done;
    if (!u.hostname || !*u.hostname) goto done;

    int port = u.has_port ? u.port : default_port(u.scheme);
    buf_t urlb = {0};
    if (u.path && strncmp(u.path, "/devtools/browser/", 18) == 0) {
        bputs(&urlb, full);
    } else {
        bputs(&urlb, u.scheme ? u.scheme : "");
        bputs(&urlb, "://");
        bputs(&urlb, u.netloc ? u.netloc : "");
    }
    out = json_array();
    json_append(out, json_string(urlb.p ? urlb.p : ""));
    json_append(out, json_number((double)port));
    free(urlb.p);

done:
    url_free(&u);
    free(full);
    return out;  /* NULL == Python None */
}
