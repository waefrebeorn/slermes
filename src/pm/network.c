/*
 * network.c — C port of upstream Hermes pm/network.py (gate w5_network_5fdf96).
 *
 * PoP: w5_network_5fdf96_is_transient @ pm/network.py:is_transient
 * PoP: w5_network_5fdf96_edge_block  @ pm/network.py:edge_block
 * PoP: w5_network_5fdf96_delay       @ pm/network.py:_delay
 */
#include "noc_w5_network_5fdf96.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void w5_network_5fdf96_free(char *s) { free(s); }

static char *dupn(const char *s, size_t n) {
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    if (n) memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static int http_retry(int code) {
    return code == 408 || code == 429 || code == 500
        || code == 502 || code == 503 || code == 504;
}

bool w5_network_5fdf96_is_transient(const w5_network_5fdf96_exc_t *exc) {
    if (!exc || !exc->kind) return false;
    const char *k = exc->kind;
    if (!strcmp(k, "http")) return http_retry(exc->code);
    if (!strcmp(k, "url")) return exc->reason && w5_network_5fdf96_is_transient(exc->reason);
    if (!strcmp(k, "ssl_eof") || !strcmp(k, "ssl_zero_return")) return true;
    if (!strcmp(k, "ssl")) return false;
    if (!strcmp(k, "gaierror")) return exc->eai_again;
    if (!strcmp(k, "timeout") || !strcmp(k, "connection")
        || !strcmp(k, "incomplete_read")) return true;
    if (!strcmp(k, "os")) {
        static const char *const net_errnos[] = {
            "ECONNABORTED", "ECONNREFUSED", "ECONNRESET", "ENETDOWN",
            "ENETRESET", "ENETUNREACH", "EHOSTUNREACH", "EPIPE", "ETIMEDOUT"
        };
        for (size_t i = 0; i < sizeof(net_errnos) / sizeof(net_errnos[0]); i++)
            if (exc->errno_name && !strcmp(exc->errno_name, net_errnos[i])) return true;
        return false;
    }
    return false;
}

/* urllib.parse.urlsplit(url).hostname, for the ordinary http(s) URLs here. */
static void hostname(const char *url, char *out, size_t outsz) {
    out[0] = '\0';
    if (!url) return;
    const char *p = strstr(url, "://");
    if (!p) return;             /* relative / opaque: hostname is None */
    p += 3;
    const char *at = strrchr(p, '@');
    if (at) p = at + 1;
    const char *end = p;
    while (*end && *end != '/' && *end != '?' && *end != '#') end++;
    size_t n = (size_t)(end - p);
    if (*p == '[') {
        const char *close = memchr(p + 1, ']', n ? n - 1 : 0);
        if (close) n = (size_t)(close - (p + 1)), p = p + 1;
    } else {
        const char *colon = memchr(p, ':', n);
        if (colon) n = (size_t)(colon - p);
    }
    if (n >= outsz) n = outsz - 1;
    memcpy(out, p, n);
    out[n] = '\0';
}

static const char *header_get(const char *const *keys, const char *const *vals,
                              size_t n, const char *want) {
    for (size_t i = 0; i < n; i++)
        if (keys[i] && !strcmp(keys[i], want)) return vals[i] ? vals[i] : "";
    return NULL;
}

char *w5_network_5fdf96_edge_block(int code,
                                   const char *const *header_keys,
                                   const char *const *header_vals,
                                   size_t n_headers,
                                   const char *filename) {
    if (code != 403 && code != 429) return NULL;
    char host[512];
    hostname(filename, host, sizeof(host));
    const char *vercel = header_get(header_keys, header_vals, n_headers, "x-vercel-mitigated");
    const char *cf = header_get(header_keys, header_vals, n_headers, "cf-mitigated");
    const char *server = header_get(header_keys, header_vals, n_headers, "server");
    const char *cache = header_get(header_keys, header_vals, n_headers, "cf-cache-status");
    const char *cdn = NULL;
    if (vercel && *vercel) cdn = "Vercel";
    else {
        int lower_cf = (cf && *cf);
        int r2 = !strcmp(host, "hermes-assets.nousresearch.com");
        int srv_cf = 0;
        if (server) {
            char tmp[128]; size_t n = strlen(server);
            if (n >= sizeof(tmp)) n = sizeof(tmp) - 1;
            for (size_t i = 0; i < n; i++) tmp[i] = (char)tolower((unsigned char)server[i]);
            tmp[n] = '\0';
            srv_cf = !strcmp(tmp, "cloudflare");
        }
        if (lower_cf || (r2 && srv_cf && !(cache && *cache))) cdn = "Cloudflare";
    }
    if (!cdn) return NULL;
    const char *h = host[0] ? host : "the server";
    size_t sz = strlen(h) + strlen(cdn) + 128;
    char *out = (char *)malloc(sz);
    if (!out) return NULL;
    snprintf(out, sz,
        "%s's CDN firewall (%s) refused this network. Your install is fine; retry from another network",
        h, cdn);
    return out;
}

double w5_network_5fdf96_delay(int attempt, const char *retry_after) {
    double delay = ldexp(1.0, attempt);
    if (retry_after && *retry_after) {
        const char *s = retry_after;
        while (isspace((unsigned char)*s)) s++;
        char *end = NULL;
        errno = 0;
        long v = strtol(s, &end, 10);
        while (end && isspace((unsigned char)*end)) end++;
        if (errno == 0 && end && *end == '\0' && end != s && (double)v > delay)
            delay = (double)v;
        /* Non-integer Retry-After values are HTTP dates; those are not used
         * by the fixture domain and would only ever lower backoff to 0. */
    }
    return delay;
}
