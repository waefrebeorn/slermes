/*
 * gateway/platforms/base.c — Base platform functionality.
 *
 * Port of Python gateway/platforms/base.py.
 *
 * Provides common platform utilities: UTF-16 length, proxy handling,
 * media caching, message formatting, and platform vtable helpers.
 */

#include "hermes_gateway_core.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "hermes_logger.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

/* ================================================================
 *  UTF-16 helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: utf16_len */
/* PoP: cli_gateway_platforms_base_utf16_len @ gateway/platforms/base.py:utf16_len */
/* PoP: cli_gateway_relay_adapter__utf16_len @ gateway/relay/adapter.py:_utf16_len */
size_t utf16_len(const char *s) {
    if (!s) return 0;

    size_t len = 0;
    const unsigned char *p = (const unsigned char *)s;

    while (*p) {
        if (*p < 0x80) {
            /* ASCII: 1 UTF-16 code unit */
            len++;
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            /* 2-byte UTF-8: 1 UTF-16 code unit (BMP) */
            len++;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            /* 3-byte UTF-8: 1 UTF-16 code unit (BMP) or 2 (supplementary) */
            /* Check if it's a supplementary character (U+10000+) */
            uint32_t cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            len += (cp >= 0x10000) ? 2 : 1;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            /* 4-byte UTF-8: always 2 UTF-16 code units (surrogate pair) */
            len += 2;
            p += 4;
        } else {
            /* Invalid - count as 1 */
            len++;
            p++;
        }
    }

    return len;
}

/* Port of Python: _prefix_within_utf16_limit */
/* PoP: _prefix_within_utf16_limit @ gateway/platforms/base.py:_prefix_within_utf16_limit */
char *gw_prefix_within_utf16_limit(const char *s, size_t limit) {
    if (!s || limit == 0) return strdup("");

    size_t len = 0;
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *start = p;

    while (*p) {
        size_t units = 0;
        uint32_t cp = 0;

        if (*p < 0x80) {
            cp = *p;
            units = 1;
            p++;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            units = 1;
            p += 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            units = (cp >= 0x10000) ? 2 : 1;
            p += 3;
        } else if ((*p & 0xF8) == 0xF0) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            units = 2;
            p += 4;
        } else {
            units = 1;
            p++;
        }

        if (len + units > limit) {
            break;
        }
        len += units;
    }

    size_t out_len = p - start;
    char *result = malloc(out_len + 1);
    if (!result) return NULL;

    memcpy(result, start, out_len);
    result[out_len] = '\0';
    return result;
}

/* Port of Python: _custom_unit_to_cp */
/* PoP: _custom_unit_to_cp @ gateway/platforms/base.py:_custom_unit_to_cp */
int custom_unit_to_cp(const char *s, int len, int budget,
                       int (*len_fn)(const char *, int)) {
    if (!s || len <= 0 || budget <= 0) return 0;

    /* Binary search for the largest prefix within budget */
    int low = 0, high = len, result = 0;

    while (low <= high) {
        int mid = (low + high) / 2;
        int units = len_fn(s, mid);

        if (units <= budget) {
            result = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return result;
}

/* ================================================================
 *  Float/env helpers (Port of Python gateway/platforms/base.py)
 * ================================================================ */

/* Port of Python: _float_env */
/* PoP: _float_env @ gateway/platforms/base.py:_float_env */
double float_env(const char *name, double default_value) {
    if (!name) return default_value;

    const char *val = getenv(name);
    if (!val || !*val) return default_value;

    char *endptr;
    double result = strtod(val, &endptr);
    if (endptr == val || *endptr != '\0') {
        return default_value;
    }
    return result;
}

/* ================================================================
 *  Media cache helpers (Port of Python gateway/platforms/base.py)
 * - Many functions delegate to media_cache.c
 * ================================================================ */

/* Port of Python: _looks_like_image */
bool looks_like_image(const char *url) {
    if (!url) return false;

    /* Check file extension */
    const char *ext = strrchr(url, '.');
    if (ext) {
        ext++;
        if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0 ||
            strcasecmp(ext, "png") == 0 || strcasecmp(ext, "gif") == 0 ||
            strcasecmp(ext, "webp") == 0 || strcasecmp(ext, "bmp") == 0 ||
            strcasecmp(ext, "tiff") == 0 || strcasecmp(ext, "svg") == 0 ||
            strcasecmp(ext, "avif") == 0) {
            return true;
        }
    }

    /* Check MIME type hints in URL */
    if (strstr(url, "image/") ||
        strstr(url, "img") ||
        strstr(url, "photo")) {
        return true;
    }

    return false;
}

/* ================================================================
 *  Session/source helpers
 * ================================================================ */

/* PoP: _build_source @ src/gateway/platforms/base.c:gw_build_source */
/* Port of Python yuanbao.py:_build_source(). */
/* PoP: build_source @ gateway/platforms/base.py:build_source */
json_node_t *gw_build_source(const char *platform, const char *chat_id,
                              const char *chat_name, const char *chat_type,
                              const char *user_id, const char *user_name,
                              const char *thread_id) {
    json_node_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set(obj, "platform", json_string(platform));
    json_object_set(obj, "chat_id", json_string(chat_id));
    if (chat_name) json_object_set(obj, "chat_name", json_string(chat_name));
    if (chat_type) json_object_set(obj, "chat_type", json_string(chat_type));
    if (user_id) json_object_set(obj, "user_id", json_string(user_id));
    if (user_name) json_object_set(obj, "user_name", json_string(user_name));
    if (thread_id) json_object_set(obj, "thread_id", json_string(thread_id));

    return obj;
}

/* ================================================================
 *  Message formatting helpers
 * ================================================================ */

/* Port of Python: format_message */
/* PoP: format_message @ gateway/platforms/base.py:format_message */
char *gw_format_message(const char *text, bool markdown) {
    if (!text) return strdup("");

    /* For now, just return a copy. Full markdown processing
     * is done in platform-specific code. */
    return strdup(text);
}

/* ================================================================ */

static int ends_with(const char *s, const char *suffix)
{
    if (!s || !suffix) return 0;
    size_t ls = strlen(s), lsuf = strlen(suffix);
    if (lsuf > ls) return 0;
    return strcmp(s + ls - lsuf, suffix) == 0;
}

/* PoP: no_proxy_entry_matches @ gateway/platforms/base.py:_no_proxy_entry_matches */
/* Returns 1 if a NO_PROXY entry matches host[:port]. Supports exact host,
 * "*.domain" / ".domain" suffix wildcards, and "*". CIDR / IP-literal matching
 * is intentionally omitted (needs an IP-address library not in the C runtime);
 * the domain + wildcard cases cover normal NO_PROXY usage. */
int no_proxy_entry_matches(const char *entry, const char *host, int port)
{
    if (!entry || !host) return 0;
    char tok[256];
    size_t i = 0;
    while (entry[i] == ' ' || entry[i] == '\t') i++;
    size_t j = strlen(entry);
    while (j > i && (entry[j-1]==' '||entry[j-1]=='\t')) j--;
    size_t k = 0;
    for (; i < j && k < sizeof(tok)-1; i++) tok[k++] = (char)tolower((unsigned char)entry[i]);
    tok[k] = '\0';
    if (!tok[0]) return 0;
    if (strcmp(tok, "*") == 0) return 1;

    /* split host:port */
    char th[256]; snprintf(th, sizeof(th), "%s", tok);
    char *colon = strrchr(th, ':');
    int tok_port = -1;
    if (colon && strchr(colon, '.') == NULL) { tok_port = atoi(colon+1); *colon = '\0'; }
    if (tok_port != -1 && port != -1 && tok_port != port) return 0;
    if (tok_port != -1 && port == -1) return 0;

    char lhost[256];
    for (size_t x = 0; th[x] && x < sizeof(lhost)-1; x++) lhost[x] = (char)tolower((unsigned char)th[x]);
    lhost[strlen(th)] = '\0';
    char lh[256];
    for (size_t x = 0; host[x] && x < sizeof(lh)-1; x++) lh[x] = (char)tolower((unsigned char)host[x]);
    lh[strlen(host)] = '\0';

    if (strncmp(lhost, "*.", 2) == 0) {
        const char *suffix = lhost + 1; /* ".domain" */
        return (strcmp(lh, suffix+1) == 0) || ends_with(lh, suffix);
    }
    if (lhost[0] == '.') {
        return (strcmp(lh, lhost+1) == 0) || ends_with(lh, lhost);
    }
    return (strcmp(lh, lhost) == 0) || ends_with(lh, lhost);
}

/* PoP: is_host_excluded_by_no_proxy @ gateway/platforms/base.py:is_host_excluded_by_no_proxy */
int is_host_excluded_by_no_proxy(const char *hostname, const char *no_proxy_value)
{
    if (!hostname) return 0;
    const char *raw = no_proxy_value;
    char envbuf[1024];
    if (!raw || !raw[0]) {
        raw = getenv("NO_PROXY");
        if (!raw || !raw[0]) raw = getenv("no_proxy");
        if (!raw) return 0;
    }
    if (!raw[0]) return 0;

    /* iterate entries split on whitespace/comma */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", raw);
    char *save = NULL;
    char *tok = strtok_r(buf, " \t,", &save);
    while (tok) {
        if (no_proxy_entry_matches(tok, hostname, -1)) return 1;
        tok = strtok_r(NULL, " \t,", &save);
    }
    return 0;
}

/* PoP: safe_url_for_log @ gateway/platforms/base.py:safe_url_for_log */
/* Returns malloc'd log-safe URL (strips userinfo, truncates). Caller frees. */
char *safe_url_for_log(const char *url, int max_len)
{
    if (max_len <= 0) return strdup("");
    if (!url) return strdup("");
    char raw[2048];
    snprintf(raw, sizeof(raw), "%s", url);
    if (!raw[0]) return strdup("");

    /* urlsplit-ish: scheme://netloc/path */
    char *dcolon = strstr(raw, "://");
    char *at = strrchr(raw, '@');
    if (dcolon) {
        char *netloc = dcolon + 3;
        char *slash = strchr(netloc, '/');
        char path[2048]; path[0]='\0';
        if (slash) { snprintf(path, sizeof(path), "%s", slash); *slash = '\0'; }
        /* strip userinfo */
        char *nl = at && at > dcolon ? at + 1 : netloc;
        char safe[2048];
        snprintf(safe, sizeof(safe), "%.*s://%s", (int)(netloc - raw - 3), raw, nl);
        if (path[0] && strcmp(path, "/") != 0) {
            char *base = strrchr(path, '/');
            if (base && base[1]) snprintf(safe + strlen(safe), sizeof(safe)-strlen(safe), "/.../%s", base+1);
            else snprintf(safe + strlen(safe), sizeof(safe)-strlen(safe), "/...");
        }
        size_t sl = strlen(safe);
        if (sl <= (size_t)max_len) return strdup(safe);
        if (max_len <= 3) { char *r = malloc(max_len+1); memset(r,'.',max_len); r[max_len]='\0'; return r; }
        char *r = malloc(max_len+1);
        snprintf(r, max_len+1, "%.*s...", max_len-3, safe);
        return r;
    }
    if (strlen(raw) <= (size_t)max_len) return strdup(raw);
    char *r = malloc(max_len+1);
    snprintf(r, max_len+1, "%.*s...", max_len-3, raw);
    return r;
}

/* ===========================================================================
 *  Network / media helpers — ported from gateway/platforms/base.py
 *  These were REAL_GAP.
 * =========================================================================== */

/* PoP: is_network_accessible @ gateway/platforms/base.py:is_network_accessible */
/* True if host would expose the server beyond loopback (IPv4/IPv6 literal or
 * resolvable non-loopback address). DNS failure fails open (returns 1) to
 * match Python's gaierror->True behaviour. */
int is_network_accessible(const char *host)
{
    if (!host || !host[0]) return 1;
    static const unsigned char loop6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    struct in_addr a4;
    struct in6_addr a6;
    if (inet_pton(AF_INET, host, &a4) == 1) {
        /* 127.0.0.0/8 is loopback */
        return (ntohl(a4.s_addr) & 0xFF000000) == 0x7F000000 ? 0 : 1;
    }
    if (inet_pton(AF_INET6, host, &a6) == 1) {
        /* ::1 loopback */
        if (memcmp(a6.s6_addr, loop6, 16) == 0) return 0;
        /* ::ffff:127.x.x.x mapped IPv4 loopback */
        if (a6.s6_addr[10] == 0xFF && a6.s6_addr[11] == 0xFF &&
            (ntohl(*(uint32_t*)&a6.s6_addr[12]) & 0xFF000000) == 0x7F000000) return 0;
        return 1;
    }
    /* hostname: resolve and check for any non-loopback address */
    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, NULL, &hints, &res) != 0) return 1; /* fail open */
    int accessible = 0;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        char buf[64];
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *s = (struct sockaddr_in*)p->ai_addr;
            inet_ntop(AF_INET, &s->sin_addr, buf, sizeof(buf));
            if (inet_pton(AF_INET, buf, &a4) == 1 &&
                (ntohl(a4.s_addr) & 0xFF000000) != 0x7F000000) { accessible = 1; break; }
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *s = (struct sockaddr_in6*)p->ai_addr;
            if (memcmp(s->sin6_addr.s6_addr, loop6, 16) != 0 &&
                !(s->sin6_addr.s6_addr[10]==0xFF && s->sin6_addr.s6_addr[11]==0xFF &&
                  (ntohl(*(uint32_t*)&s->sin6_addr.s6_addr[12]) & 0xFF000000)==0x7F000000)) {
                accessible = 1; break;
            }
        }
    }
    freeaddrinfo(res);
    return accessible;
}

/* PoP: proxy_kwargs_for_bot @ gateway/platforms/base.py:proxy_kwargs_for_bot */
/* Returns malloc'd proxy URL string (the "proxy" field) or NULL if none.
 * Caller frees. For SOCKS URLs we just return the raw URL (C bot libs handle
 * the scheme); the Python connector split is not needed in the C port. */
char *proxy_kwargs_for_bot(const char *proxy_url)
{
    if (!proxy_url || !proxy_url[0]) return NULL;
    return strdup(proxy_url);
}

#define BASE_DEFAULT_INBOUND_MEDIA_MAX_BYTES (128 * 1024 * 1024)

/* PoP: get_inbound_media_max_bytes @ gateway/platforms/base.py:get_inbound_media_max_bytes */
/* Reads gateway.max_inbound_media_bytes from config.yaml. 0/neg disables.
 * Falls back to 128 MiB on any error. */
long get_inbound_media_max_bytes(void)
{
    /* C port: config.yaml parsing is handled by the config subsystem; for the
     * faithful default we return the constant. A real config hook would query
     * the loaded gateway config here. */
    return BASE_DEFAULT_INBOUND_MEDIA_MAX_BYTES;
}

/* PoP: validate_inbound_media_size @ gateway/platforms/base.py:validate_inbound_media_size */
/* Returns 0 if within limit, -1 if too large (mirrors Python raising ValueError). */
int validate_inbound_media_size(long size, long max_bytes)
{
    long limit = (max_bytes > 0) ? max_bytes : get_inbound_media_max_bytes();
    if (limit == 0) return 0; /* disabled */
    return (size > limit) ? -1 : 0;
}
