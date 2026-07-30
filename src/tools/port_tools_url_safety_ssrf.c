/*
 * port_tools_url_safety_ssrf.c — C port of the SSRF-safe HTTP transport layer
 * from tools/url_safety.py.
 *
 * Python monkeypatches httpx's network backend so that at TCP-connect time the
 * target host is re-resolved and every candidate IP is validated against the
 * SSRF policy; the connection is then dialed by a vetted IP while the original
 * hostname is preserved for the Host header, SNI, and certificate verification.
 * That closes the DNS-rebinding gap between pre-flight is_safe_url() validation
 * and the actual socket connect.
 *
 * In C there is no httpx to monkeypatch — the faithful analogue is the async
 * HTTP client's connect-time SSRF guard hook (async_http_set_ssrf_guard). This
 * module implements the guard and the client factories that install it:
 *
 *   Python                                   C (this module)
 *   ------------------------------------     -------------------------------------
 *   _proxy_is_configured()                   ssrf_proxy_is_configured()
 *   _safe_connect_scheme(h,p,map)            ssrf_safe_connect_scheme(h,p,map)
 *   _resolved_http_connect_ips(h,p,scheme)   ssrf_resolved_http_connect_ips(...)
 *   _is_blocked_ip(ip)                       ssrf_is_blocked_ip(sockaddr)
 *   _SSRFGuarded[Async]NetworkBackend        ssrf_connect_guard() (the hook)
 *   _origin_scheme_context(request)          ssrf_origin_scheme_context(...)
 *   ssrf_safe_[async_]http_transport()       ssrf_safe_http_transport()
 *   _install_ssrf_guard_on_[async_]transport ssrf_install_guard_on_transport()
 *   _install_ssrf_guard_on_[async_]client    ssrf_install_guard_on_client()
 *   create_ssrf_safe_[async_]client()        ssrf_create_safe_client()
 *
 * The async/sync split in Python is an asyncio.to_thread wrapper over identical
 * validation logic; in C both map to the same non-blocking event-loop transport,
 * so the "async" and "sync" C entry points are the same faithful function.
 */

#define _GNU_SOURCE
#include "async_runtime.h"
#include "hermes_url_safety.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* PoP: _MAX_SSRF_CONNECT_IPS @ tools/url_safety.py:_MAX_SSRF_CONNECT_IPS */
#define SSRF_MAX_CONNECT_IPS 8

/* Proxy environment variables (order matches Python _PROXY_ENV_VARS). */
static const char *SSRF_PROXY_ENV_VARS[] = {
    "HTTPS_PROXY", "https_proxy",
    "HTTP_PROXY", "http_proxy",
    "ALL_PROXY", "all_proxy",
    NULL,
};

/* PoP: ssrf_proxy_is_configured @ tools/url_safety.py:_proxy_is_configured */
/* Return true when at least one HTTP proxy env var is set (non-empty). */
bool ssrf_proxy_is_configured(void)
{
    for (int i = 0; SSRF_PROXY_ENV_VARS[i]; i++) {
        const char *v = getenv(SSRF_PROXY_ENV_VARS[i]);
        if (v && *v) return true;
    }
    return false;
}

/* An origin->scheme mapping entry (faithful to Python's dict[(host,port)]->scheme). */
typedef struct {
    char host[256];
    int  port;
    char scheme[8];   /* "http" or "https" */
} ssrf_origin_scheme_t;

/* PoP: ssrf_safe_connect_scheme @ tools/url_safety.py:_safe_connect_scheme */
/* Return schemes_by_origin.get((host,port)) or ("https" if port==443 else "http").
 * `map`/`n` may be NULL/0 (empty mapping). Result is a static-lifetime literal
 * or a pointer into `map`; caller must not free. */
const char *ssrf_safe_connect_scheme(const char *host, int port,
                                     const ssrf_origin_scheme_t *map, int n)
{
    if (map && host) {
        for (int i = 0; i < n; i++) {
            if (map[i].port == port && strcmp(map[i].host, host) == 0)
                return map[i].scheme;
        }
    }
    return (port == 443) ? "https" : "http";
}

/* PoP: ssrf_is_blocked_ip @ tools/url_safety.py:_is_blocked_ip */
/* Return true if a resolved sockaddr should be blocked for SSRF protection.
 * Faithful to Python _is_blocked_ip: private / loopback / link-local /
 * reserved / multicast / unspecified / CGNAT(100.64/10), and IPv4-mapped
 * IPv6 (::ffff:x.x.x.x) checked by the embedded IPv4 address. Also enforces
 * the always-blocked cloud-metadata floor (169.254.169.254 & friends). */
bool ssrf_is_blocked_ip(const struct sockaddr *sa)
{
    if (!sa) return true;

    if (sa->sa_family == AF_INET) {
        uint32_t h = ntohl(((const struct sockaddr_in *)sa)->sin_addr.s_addr);
        /* Exact IPv4 network set that Python ipaddress marks
         * is_private/is_reserved/is_multicast/is_loopback/is_link_local,
         * plus CGNAT 100.64.0.0/10 (not covered by is_private). Each entry is
         * {network base (host order), prefix length}. Faithful to
         * IPv4Address._constants._private_networks + _reserved/_multicast +
         * _CGNAT_NETWORK in tools/url_safety.py:_is_blocked_ip. */
        static const struct { uint32_t base; int bits; } V4[] = {
            {0x00000000u,  8},  /* 0.0.0.0/8        */
            {0x0A000000u,  8},  /* 10.0.0.0/8       */
            {0x7F000000u,  8},  /* 127.0.0.0/8      */
            {0xA9FE0000u, 16},  /* 169.254.0.0/16   */
            {0xAC100000u, 12},  /* 172.16.0.0/12    */
            {0xC0000000u, 24},  /* 192.0.0.0/24     */
            {0xC0000200u, 24},  /* 192.0.2.0/24     */
            {0xC0A80000u, 16},  /* 192.168.0.0/16   */
            {0xC6120000u, 15},  /* 198.18.0.0/15    */
            {0xC6336400u, 24},  /* 198.51.100.0/24  */
            {0xCB007100u, 24},  /* 203.0.113.0/24   */
            {0xF0000000u,  4},  /* 240.0.0.0/4      */
            {0xFFFFFFFFu, 32},  /* 255.255.255.255/32 */
            {0xE0000000u,  4},  /* 224.0.0.0/4 multicast */
            {0x64400000u, 10},  /* 100.64.0.0/10 CGNAT */
        };
        for (size_t i = 0; i < sizeof(V4)/sizeof(V4[0]); i++) {
            uint32_t mask = V4[i].bits == 0 ? 0 : (0xFFFFFFFFu << (32 - V4[i].bits));
            if ((h & mask) == (V4[i].base & mask)) return true;
        }
        return false;
    }

    if (sa->sa_family == AF_INET6) {
        const uint8_t *b = ((const struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;

        /* IPv4-mapped IPv6 (::ffff:x.x.x.x): check embedded IPv4. */
        static const uint8_t v4map[12] = {0,0,0,0,0,0,0,0,0,0,0xFF,0xFF};
        if (memcmp(b, v4map, 12) == 0) {
            struct sockaddr_in embedded;
            memset(&embedded, 0, sizeof(embedded));
            embedded.sin_family = AF_INET;
            memcpy(&embedded.sin_addr.s_addr, b + 12, 4);
            return ssrf_is_blocked_ip((const struct sockaddr *)&embedded);
        }

        /* Exact IPv6 network set Python marks private/reserved/multicast/etc.
         * (IPv6Address._constants._private_networks + _multicast). Each entry:
         * {byte prefix, prefix length in bits}. ::ffff:0.0.0.0/96 is handled
         * above via the v4-mapped path. */
        static const struct { uint8_t pfx[16]; int bits; } V6[] = {
            {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}, 128}, /* ::1/128 */
            {{0}, 128},                                /* ::/128  */
            {{0,0x64,0xFF,0x9B,0,1}, 48},              /* 64:ff9b:1::/48 */
            {{0x01,0}, 64},                            /* 100::/64 */
            {{0x20,0x01,0}, 23},                       /* 2001::/23 */
            {{0x20,0x01,0x0D,0xB8}, 32},               /* 2001:db8::/32 */
            {{0x20,0x02}, 16},                         /* 2002::/16 */
            {{0x3F,0xFF}, 20},                         /* 3fff::/20 */
            {{0xFC}, 7},                               /* fc00::/7 */
            {{0xFE,0x80}, 10},                         /* fe80::/10 */
            {{0xFF}, 8},                               /* ff00::/8 multicast */
            /* IPv6Address._constants._reserved_networks (is_reserved). */
            {{0x00}, 8},                               /* ::/8     */
            {{0x01}, 8},                               /* 100::/8  */
            {{0x02}, 7},                               /* 200::/7  */
            {{0x04}, 6},                               /* 400::/6  */
            {{0x08}, 5},                               /* 800::/5  */
            {{0x10}, 4},                               /* 1000::/4 */
            {{0x40}, 3},                               /* 4000::/3 */
            {{0x60}, 3},                               /* 6000::/3 */
            {{0x80}, 3},                               /* 8000::/3 */
            {{0xA0}, 3},                               /* a000::/3 */
            {{0xC0}, 3},                               /* c000::/3 */
            {{0xE0}, 4},                               /* e000::/4 */
            {{0xF0}, 5},                               /* f000::/5 */
            {{0xF8}, 6},                               /* f800::/6 */
            {{0xFE,0x00}, 9},                          /* fe00::/9 */
        };
        for (size_t i = 0; i < sizeof(V6)/sizeof(V6[0]); i++) {
            int bits = V6[i].bits, ok = 1;
            int full = bits / 8, rem = bits % 8;
            for (int k = 0; k < full && ok; k++)
                if (b[k] != V6[i].pfx[k]) ok = 0;
            if (ok && rem) {
                uint8_t mask = (uint8_t)(0xFF << (8 - rem));
                if ((b[full] & mask) != (V6[i].pfx[full] & mask)) ok = 0;
            }
            if (ok) return true;
        }
        return false;
    }

    return true; /* unknown family — fail closed */
}

/* PoP: ssrf_resolved_http_connect_ips @ tools/url_safety.py:_resolved_http_connect_ips */
/* Resolve + validate `host` for one HTTP connect attempt. Returns a malloc'd,
 * NULL-terminated array of vetted IP strings (at most SSRF_MAX_CONNECT_IPS,
 * de-duplicated, preserving getaddrinfo order), or NULL on any policy
 * violation / DNS failure / empty result (fail-closed, mirroring the Python
 * SSRFConnectionBlocked raises). `scheme` currently informs only trusted-host
 * exceptions; global allow-private honours url_set_allow_private(). */
char **ssrf_resolved_http_connect_ips(const char *host, int port, const char *scheme)
{
    (void)scheme;
    if (!host) return NULL;

    /* Normalise: strip, lower, strip trailing dots (Python .strip().lower().rstrip(".")). */
    char hostname[256];
    size_t j = 0;
    for (const char *p = host; *p && j + 1 < sizeof(hostname); p++) {
        if (*p == ' ' || *p == '\t') continue;  /* leading/inner ws is invalid in a host anyway */
        char c = *p;
        hostname[j++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    hostname[j] = '\0';
    while (j > 0 && hostname[j - 1] == '.') hostname[--j] = '\0';
    if (j == 0) return NULL;  /* empty hostname -> blocked */

    /* Always-blocked internal hostnames (metadata.google.internal, metadata.goog). */
    if (strcmp(hostname, "metadata.google.internal") == 0 ||
        strcmp(hostname, "metadata.goog") == 0) {
        return NULL;
    }

    struct addrinfo hints, *res0 = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", port);
    if (getaddrinfo(hostname, portstr, &hints, &res0) != 0 || !res0) {
        if (res0) freeaddrinfo(res0);
        return NULL;  /* DNS resolution failed -> blocked */
    }

    char **out = calloc(SSRF_MAX_CONNECT_IPS + 1, sizeof(char *));
    if (!out) { freeaddrinfo(res0); return NULL; }
    int count = 0;

    for (struct addrinfo *ai = res0; ai; ai = ai->ai_next) {
        char ipbuf[INET6_ADDRSTRLEN];
        const void *addr = NULL;
        if (ai->ai_family == AF_INET)
            addr = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
        else if (ai->ai_family == AF_INET6)
            addr = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
        else
            continue;
        if (!inet_ntop(ai->ai_family, addr, ipbuf, sizeof(ipbuf))) {
            /* unparseable IP -> blocked (Python raises SSRFConnectionBlocked) */
            for (int i = 0; i < count; i++) free(out[i]);
            free(out); freeaddrinfo(res0);
            return NULL;
        }

        /* Policy: unless private is globally allowed, block private/internal. */
        if (!url_allow_private_enabled() && ssrf_is_blocked_ip(ai->ai_addr)) {
            for (int i = 0; i < count; i++) free(out[i]);
            free(out); freeaddrinfo(res0);
            return NULL;
        }

        /* De-dup + cap. */
        int dup = 0;
        for (int i = 0; i < count; i++)
            if (strcmp(out[i], ipbuf) == 0) { dup = 1; break; }
        if (!dup && count < SSRF_MAX_CONNECT_IPS) {
            out[count++] = strdup(ipbuf);
        }
    }
    freeaddrinfo(res0);

    if (count == 0) {  /* DNS returned no usable results -> blocked */
        free(out);
        return NULL;
    }
    out[count] = NULL;
    return out;
}

/* The connect-time guard installed on an async_http client. Resolves the
 * effective scheme and delegates to ssrf_resolved_http_connect_ips. `ctx` may
 * carry an origin->scheme map (from ssrf_origin_scheme_context); NULL means the
 * default port-based scheme is used. Signature matches async_http_ssrf_guard_t. */
/* PoP: ssrf_connect_guard @ tools/url_safety.py:connect_tcp */
char **ssrf_connect_guard(const char *host, int port, const char *scheme, void *ctx)
{
    const ssrf_origin_scheme_t *map = (const ssrf_origin_scheme_t *)ctx;
    const char *eff_scheme = scheme;
    if (map) {
        /* One-entry map convention (ctx points at a single origin). */
        eff_scheme = ssrf_safe_connect_scheme(host, port, map, 1);
    } else if (!eff_scheme || !*eff_scheme) {
        eff_scheme = ssrf_safe_connect_scheme(host, port, NULL, 0);
    }
    return ssrf_resolved_http_connect_ips(host, port, eff_scheme);
}

/* PoP: ssrf_origin_scheme_context @ tools/url_safety.py:_origin_scheme_context */
/* Build a one-entry origin->scheme mapping for a request's URL. Returns 1 and
 * fills *out when host/port/scheme are valid (scheme in {http,https}); returns
 * 0 (empty context) otherwise, faithful to Python returning {} . */
int ssrf_origin_scheme_context(const char *host, int port, const char *scheme,
                               ssrf_origin_scheme_t *out)
{
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    if (!host || !*host || port < 0 || !scheme) return 0;
    if (strcmp(scheme, "http") != 0 && strcmp(scheme, "https") != 0) return 0;
    snprintf(out->host, sizeof(out->host), "%s", host);
    out->port = port;
    snprintf(out->scheme, sizeof(out->scheme), "%s", scheme);
    return 1;
}

/* PoP: ssrf_install_guard_on_client @ tools/url_safety.py:_install_ssrf_guard_on_client */
/* PoP: ssrf_install_guard_on_client @ tools/url_safety.py:_install_ssrf_guard_on_async_client */
/* PoP: ssrf_install_guard_on_transport @ tools/url_safety.py:_install_ssrf_guard_on_transport */
/* PoP: ssrf_install_guard_on_transport @ tools/url_safety.py:_install_ssrf_guard_on_async_transport */
/* Install the SSRF guard on an async_http client. In C there is one client
 * type and one (non-blocking) transport, so the Python async/sync transport and
 * client installers all collapse to this single faithful operation: point the
 * client's connect-time hook at ssrf_connect_guard. Returns 0 on success, -1 if
 * the client is NULL ("unsupported transport cannot be made SSRF-safe"). */
int ssrf_install_guard_on_transport(async_http_client_t *client)
{
    if (!client) return -1;
    async_http_set_ssrf_guard(client, ssrf_connect_guard, NULL);
    return 0;
}

int ssrf_install_guard_on_client(async_http_client_t *client)
{
    return ssrf_install_guard_on_transport(client);
}

/* PoP: ssrf_safe_http_transport @ tools/url_safety.py:ssrf_safe_http_transport */
/* PoP: ssrf_safe_http_transport @ tools/url_safety.py:ssrf_safe_async_http_transport */
/* Return a new async_http client whose transport pins direct TCP connects to
 * vetted IPs (the Python "transport" object with a guarded network backend).
 * timeout_ms<=0 uses the client default. Caller frees with async_http_client_free. */
async_http_client_t *ssrf_safe_http_transport(int timeout_ms)
{
    async_http_client_t *c = async_http_client_new(timeout_ms);
    if (!c) return NULL;
    async_http_set_ssrf_guard(c, ssrf_connect_guard, NULL);
    return c;
}

/* PoP: ssrf_create_safe_client @ tools/url_safety.py:create_ssrf_safe_client */
/* PoP: ssrf_create_safe_client @ tools/url_safety.py:create_ssrf_safe_async_client */
/* Create an async_http client with connect-time SSRF validation — the
 * public factory (create_ssrf_safe_client / create_ssrf_safe_async_client).
 * Direct connections are resolved, validated, and dialed by IP while the
 * request hostname is preserved for Host/SNI/cert verification. */
async_http_client_t *ssrf_create_safe_client(int timeout_ms)
{
    async_http_client_t *c = async_http_client_new(timeout_ms);
    if (!c) return NULL;
    ssrf_install_guard_on_client(c);
    return c;
}

/* PoP: ssrf_connect_unix_socket @ tools/url_safety.py:connect_unix_socket */
/* Unix socket connections are ALWAYS blocked in the SSRF-safe transport
 * (both _SSRFGuardedNetworkBackend.connect_unix_socket and the async
 * variant raise SSRFConnectionBlocked unconditionally). Returns -1 and,
 * when errbuf is provided, the exact Python error message. */
int ssrf_connect_unix_socket(const char *path, char *errbuf, size_t errsz)
{
    (void)path;
    if (errbuf && errsz)
        snprintf(errbuf, errsz,
                 "Blocked Unix socket connection in SSRF-safe transport");
    return -1;
}

/* PoP: ssrf_sleep @ tools/url_safety.py:sleep */
/* Delegate sleep to the underlying backend (Python: self._backend.sleep).
 * In the C runtime the backend is the poll event loop; a plain nanosleep is
 * the faithful single-threaded equivalent (the loop is not pumping while a
 * guarded transport sleeps in Python either). */
int ssrf_sleep(double seconds)
{
    if (seconds <= 0) return 0;
    struct timespec ts;
    ts.tv_sec  = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) { /* resume */ }
    return 0;
}
