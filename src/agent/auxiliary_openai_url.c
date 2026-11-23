/*
 * auxiliary_openai_url.c — real C11 port of upstream Hermes
 * agent/auxiliary_openai_url.py (gate w13_auxiliaryo_c4a08d).
 *
 * PoP: w13_auxiliaryo_c4a08d_is_dual_surface_anthropic_host @ agent/auxiliary_openai_url.py:_is_dual_surface_anthropic_host
 * PoP: w13_auxiliaryo_c4a08d_to_openai_base_url @ agent/auxiliary_openai_url.py:_to_openai_base_url
 *
 * Host/path extraction mirrors urllib.parse.urlparse plus the tolerant
 * base-url helpers in utils/urls.py (bare host -> "//host" prefix).
 */
#include "noc_w13_auxiliaryo_c4a08d.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { char *p; size_t len, cap; } au_sb_t;
static void au_init(au_sb_t *b) { b->cap = 256; b->len = 0; b->p = (char *)malloc(b->cap); if (b->p) b->p[0] = '\0'; }
static void au_reserve(au_sb_t *b, size_t need) {
    if (!b->p || b->len + need + 1 <= b->cap) return;
    size_t nc = b->cap; while (nc < b->len + need + 1) nc *= 2;
    char *np = (char *)realloc(b->p, nc); if (np) { b->p = np; b->cap = nc; }
}
static void au_addn(au_sb_t *b, const char *s, size_t n) {
    if (!b->p || !s || !n) return;
    au_reserve(b, n); memcpy(b->p + b->len, s, n); b->len += n; b->p[b->len] = '\0';
}
static void au_adds(au_sb_t *b, const char *s) { if (s) au_addn(b, s, strlen(s)); }
static char *au_done(au_sb_t *b) { if (!b->p) return (char *)calloc(1, 1); b->p[b->len] = '\0'; return b->p; }
static char *au_dup(const char *s) {
    if (!s) s = ""; size_t n = strlen(s); char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n); p[n] = '\0'; } return p;
}
static int au_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *au_strip(const char *s) {
    if (!s) return au_dup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && au_is_ws((unsigned char)*a)) a++;
    while (b > a && au_is_ws((unsigned char)*(b - 1))) b--;
    size_t n = (size_t)(b - a); char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, a, n); p[n] = '\0'; } return p;
}
static void au_lower(char *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32); }
static void au_rstrip_dot(char *s) { size_t n = strlen(s); while (n > 0 && s[n - 1] == '.') s[--n] = '\0'; }
static int au_endswith(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && strcmp(s + n - m, suf) == 0;
}
static int au_endswith_dot(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n > m && s[n - m - 1] == '.' && strcmp(s + n - m, suf) == 0;
}

/* Authority start. strict: only after "scheme://" or a leading "//".
 * tolerant (base_url_hostname): bare "host/path" also yields a netloc. */
static const char *au_netloc(const char *url, int tolerant) {
    const char *p = strstr(url, "://");
    if (p) return p + 3;
    if (url[0] == '/' && url[1] == '/') return url + 2;
    return tolerant ? url : NULL;
}
/* urlparse.hostname from an authority start. Returns malloc'd lower host or NULL. */
static char *au_host_from(const char *start) {
    if (!start) return NULL;
    const char *q = start;
    while (*q && *q != '/' && *q != '?' && *q != '#') q++;
    const char *at = NULL;
    for (const char *r = start; r < q; r++) if (*r == '@') at = r;
    const char *hs = at ? at + 1 : start;
    char *host;
    if (*hs == '[') {
        const char *rb = strchr(hs, ']');
        if (!rb || rb > q) return NULL;             /* invalid IPv6 -> ValueError */
        size_t n = (size_t)(rb - (hs + 1));
        host = (char *)malloc(n + 1);
        if (host) { memcpy(host, hs + 1, n); host[n] = '\0'; }
    } else {
        const char *colon = NULL;
        for (const char *r = hs; r < q; r++) if (*r == ':') { colon = r; break; }
        size_t n = (size_t)((colon ? colon : q) - hs);
        host = (char *)malloc(n + 1);
        if (host) { memcpy(host, hs, n); host[n] = '\0'; }
    }
    if (host) au_lower(host);
    return host;
}
/* Strict hostname (urlparse(url).hostname or ""). Lower, NO rstrip('.'). */
static char *au_strict_host(const char *url) {
    char *h = au_host_from(au_netloc(url, 0));
    return h ? h : au_dup("");
}
/* utils.base_url_hostname: tolerant parse, lower + rstrip('.'). */
static char *au_tolerant_host(const char *url) {
    if (!url || !*url) return au_dup("");
    char *h = au_host_from(au_netloc(url, 1));
    if (!h) return au_dup("");
    au_rstrip_dot(h);
    return h;
}
/* utils.base_url_host_matches. */
static int au_host_matches(const char *url, const char *domain) {
    char *h = au_tolerant_host(url);
    int ok = h[0] && ((strcmp(h, domain) == 0) || au_endswith_dot(h, domain));
    free(h);
    return ok;
}
/* urlparse(url).path, rstrip('/') (used by normalize_actual_base_url). */
static char *au_strict_path(const char *url) {
    const char *start;
    const char *p = strstr(url, "://");
    if (p) {
        const char *q = p + 3;
        while (*q && *q != '/' && *q != '?' && *q != '#') q++;
        start = q;
    } else if (url[0] == '/' && url[1] == '/') {
        const char *q = url + 2;
        while (*q && *q != '/' && *q != '?' && *q != '#') q++;
        start = q;
    } else {
        start = url;                     /* no authority: whole string is path */
    }
    const char *e = start;
    while (*e && *e != '?' && *e != '#') e++;
    size_t n = (size_t)(e - start);
    char *path = (char *)malloc(n + 1);
    if (path) { memcpy(path, start, n); path[n] = '\0'; }
    if (path) { size_t m = strlen(path); while (m > 0 && path[m - 1] == '/') path[--m] = '\0'; }
    return path;
}

/* ---- exported ------------------------------------------------------------ */

int w13_auxiliaryo_c4a08d_is_dual_surface_anthropic_host(const char *url) {
    char *h = au_strict_host(url ? url : "");
    static const char *const SUF[] = {"minimax.io", "minimax.chat", "minimaxi.com", NULL};
    int r = 0;
    for (int i = 0; SUF[i]; i++)
        if (!strcmp(h, SUF[i]) || au_endswith_dot(h, SUF[i])) { r = 1; break; }
    if (!r && strncmp(h, "api.minimax.", 12) == 0) r = 1;
    free(h);
    return r;
}

static char *au_normalize_actual(const char *url) {
    char *path = au_strict_path(url);
    char *host = au_strict_host(url);
    int local = !strcmp(host, "localhost") || !strcmp(host, "127.0.0.1") ||
                !strcmp(host, "::1") || !strcmp(host, "0.0.0.0");
    char *out;
    if ((path[0] == '\0' || !strcmp(path, "/")) &&
        (!strcmp(host, "api.actual.inc") || local)) {
        au_sb_t b; au_init(&b); au_adds(&b, url); au_adds(&b, "/v1"); out = au_done(&b);
    } else {
        out = au_dup(url);
    }
    free(path); free(host);
    return out;
}

char *w13_auxiliaryo_c4a08d_to_openai_base_url(const char *base_url) {
    char *url = au_strip(base_url);
    size_t n = strlen(url);
    while (n > 0 && url[n - 1] == '/') url[--n] = '\0';   /* rstrip("/") */
    char *hn = au_tolerant_host(url);
    char *out;
    if (!strcmp(hn, "api.actual.inc")) {
        out = au_normalize_actual(url);
    } else if (au_endswith(url, "/anthropic")) {
        if (au_host_matches(url, "open.bigmodel.cn") || au_host_matches(url, "api.z.ai")) {
            size_t cut = strlen(url) - strlen("/anthropic");
            au_sb_t b; au_init(&b); au_addn(&b, url, cut); au_adds(&b, "/coding/paas/v4"); out = au_done(&b);
        } else if (w13_auxiliaryo_c4a08d_is_dual_surface_anthropic_host(url)) {
            size_t cut = strlen(url) - strlen("/anthropic");
            au_sb_t b; au_init(&b); au_addn(&b, url, cut); au_adds(&b, "/v1"); out = au_done(&b);
        } else {
            out = au_dup(url);
        }
    } else if (au_host_matches(url, "api.kimi.com") && au_endswith(url, "/coding")) {
        au_sb_t b; au_init(&b); au_adds(&b, url); au_adds(&b, "/v1"); out = au_done(&b);
    } else {
        out = au_dup(url);
    }
    free(hn); free(url);
    return out;
}
