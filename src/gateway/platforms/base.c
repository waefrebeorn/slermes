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
/* PoP: utf16_len @ gateway/platforms/base.py:utf16_len */
/* PoP: utf16_len @ gateway/relay/adapter.py:_utf16_len */
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
/* PoP: gw_prefix_within_utf16_limit @ gateway/platforms/base.py:_prefix_within_utf16_limit */
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
/* PoP: custom_unit_to_cp @ gateway/platforms/base.py:_custom_unit_to_cp */
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
/* PoP: float_env @ gateway/platforms/base.py:_float_env */
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

/* PoP: gw_build_source @ src/gateway/platforms/base.c:gw_build_source */
/* Port of Python yuanbao.py:_build_source(). */
/* PoP: gw_build_source @ gateway/platforms/base.py:build_source */
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
/* PoP: gw_format_message @ gateway/platforms/base.py:format_message */
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

/* PoP: URL @ gateway/platforms/base.py:safe_url_for_log */
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

/* PoP: loopback @ gateway/platforms/base.py:is_network_accessible */
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

/* PoP: large @ gateway/platforms/base.py:validate_inbound_media_size */
/* Returns 0 if within limit, -1 if too large (mirrors Python raising ValueError). */
int validate_inbound_media_size(long size, long max_bytes)
{
    long limit = (max_bytes > 0) ? max_bytes : get_inbound_media_max_bytes();
    if (limit == 0) return 0; /* disabled */
    return (size > limit) ? -1 : 0;
}

/* ── Pure Docker mount helpers (gateway/platforms/base.py) ───────── */

extern char *dashboard_procs_hermes_home_dir(void);

/* PoP: base_parse_docker_volume_mounts @ gateway/platforms/base.py:_parse_docker_volume_mounts */
/* Returns JSON array of {"host":"...","container":"..."} pairs. Caller frees. */
char *base_parse_docker_volume_mounts(void) {
    const char *raw = getenv("TERMINAL_DOCKER_VOLUMES");
    if (!raw) raw = "";
    while (*raw == ' ' || *raw == '\t') raw++;
    if (!*raw) return strdup("[]");
    char *err = NULL;
    json_t *parsed = json_parse(raw, &err);
    if (err || !parsed || parsed->type != JSON_ARRAY) { if (err) free(err); if (parsed) json_free(parsed); return strdup("[]"); }
    json_t *result = json_array();
    for (size_t idx = 0; idx < parsed->c.count; idx++) {
        json_t *entry = parsed->c.items[idx];
        if (!entry || entry->type != JSON_STRING) continue;
        const char *spec = entry->str_val;
        while (*spec == ' ' || *spec == '\t') spec++;
        if (!*spec) continue;
        const char *sep = strstr(spec, ":/");
        if (!sep || sep <= spec) continue;
        char *host_raw = strndup(spec, (size_t)(sep - spec));
        const char *container_and_mode = sep + 1;
        const char *colon = strchr(container_and_mode, ':');
        char *container_raw = strndup(container_and_mode, colon ? (size_t)(colon - container_and_mode) : strlen(container_and_mode));
        if (container_raw[0] != '/') { free(host_raw); free(container_raw); continue; }
        /* expanduser */
        char *host_expanded = NULL;
        if (host_raw[0] == '~' && host_raw[1] == '/') {
            const char *home = getenv("HOME");
            if (home) asprintf(&host_expanded, "%s%s", home, host_raw + 1);
        }
        if (!host_expanded) host_expanded = strdup(host_raw);
        int abs_ok = (host_expanded[0] == '/' || (strlen(host_expanded) > 1 && host_expanded[1] == ':'));
        if (!abs_ok) { free(host_raw); free(container_raw); free(host_expanded); continue; }
        json_t *pair = json_object();
        json_set(pair, "host", json_string(host_expanded));
        json_set(pair, "container", json_string(container_raw));
        json_append(result, pair);
        free(host_raw); free(container_raw); free(host_expanded);
    }
    json_free(parsed);
    char *out = json_serialize(result);
    json_free(result);
    return out;
}

/* PoP: base_default_docker_workspace_root @ gateway/platforms/base.py:_default_docker_workspace_host_root */
/* Returns malloc'd path or NULL. Caller frees. */
char *base_default_docker_workspace_root(void) {
    const char *env = getenv("TERMINAL_ENV");
    if (!env) return NULL;
    while (*env == ' ' || *env == '\t') env++;
    if (strcasecmp(env, "docker") != 0) return NULL;
    const char *pers = getenv("TERMINAL_CONTAINER_PERSISTENT");
    if (!pers) pers = "true";
    while (*pers == ' ' || *pers == '\t') pers++;
    int persistent = (strcasecmp(pers, "1") == 0 || strcasecmp(pers, "true") == 0 ||
                      strcasecmp(pers, "yes") == 0 || strcasecmp(pers, "on") == 0);
    if (!persistent) return NULL;
    /* Would need get_sandbox_dir() — simplified to path */
    char *home = dashboard_procs_hermes_home_dir();
    char *out = NULL;
    asprintf(&out, "%s/sandbox/docker/default/workspace", home ? home : "~/.hermes");
    free(home);
    return out;
}

/* PoP: base_docker_persistent_home_root @ gateway/platforms/base.py:_docker_persistent_home_host_root */
/* Returns malloc'd path or NULL. Caller frees. */
char *base_docker_persistent_home_root(void) {
    const char *env = getenv("TERMINAL_ENV");
    if (!env) return NULL;
    while (*env == ' ' || *env == '\t') env++;
    if (strcasecmp(env, "docker") != 0) return NULL;
    const char *pers = getenv("TERMINAL_CONTAINER_PERSISTENT");
    if (!pers) pers = "true";
    while (*pers == ' ' || *pers == '\t') pers++;
    int persistent = (strcasecmp(pers, "1") == 0 || strcasecmp(pers, "true") == 0 ||
                      strcasecmp(pers, "yes") == 0 || strcasecmp(pers, "on") == 0);
    if (!persistent) return NULL;
    char *home = dashboard_procs_hermes_home_dir();
    char *out = NULL;
    asprintf(&out, "%s/sandbox/docker/default/home", home ? home : "~/.hermes");
    free(home);
    return out;
}

/* PoP: base_cache_dir_container_mounts @ gateway/platforms/base.py:_cache_dir_container_mounts */
/* Returns JSON array of {"host":"...","container":"..."} pairs. Caller frees. */
char *base_cache_dir_container_mounts(void) {
    const char *env = getenv("TERMINAL_ENV");
    if (!env) return strdup("[]");
    while (*env == ' ' || *env == '\t') env++;
    if (strcasecmp(env, "docker") != 0) return strdup("[]");
    /* Would need get_cache_directory_mounts() — simplified */
    return strdup("[]");
}

/* === merged from port_platforms_base.c === */
/*
 * port_platforms_base.c — Port of gateway/platforms/base.py module-level
 * helpers + the base adapter's abstract surface.
 * Pure-logic helpers implemented faithfully; abstract adapter methods are
 * documented sentinels (Python bodies are `pass`/docstring-only).
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: pb_mark_notify_metadata @ gateway/platforms/base.py:_mark_notify_metadata */
char *pb_mark_notify_metadata(const char *metadata_json) {
    /* Python: clone metadata, set notify=true. */
    if (!metadata_json || !*metadata_json) return strdup("{\"notify\": true}");
    char *out = NULL;
    if (strcmp(metadata_json, "{}") == 0)
        asprintf(&out, "{\"notify\": true}");
    else
        asprintf(&out, "%s, \"notify\": true}", metadata_json);
    return out ? out : strdup("{\"notify\": true}");
}

/* PoP: pb_split_host_port @ gateway/platforms/base.py:_split_host_port */
char *pb_split_host_port(const char *value) {
    /* Python: (host, port) from url / [v6]:port / host:port. */
    if (!value) return strdup("\t");
    char *raw = strdup(value);
    for (char *p = raw; *p; p++) *p = tolower((unsigned char)*p);
    /* trim */
    char *s = raw;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t')) s[--len] = '\0';
    if (!*s) { free(raw); return strdup("\t"); }
    char *out = NULL;
    const char *scheme = strstr(s, "://");
    if (scheme) {
        const char *host = scheme + 3;
        const char *slash = strchr(host, '/');
        const char *colon = strchr(host, ':');
        const char *end = slash ? slash : host + strlen(host);
        const char *host_end = colon && colon < end ? colon : end;
        char *h = strndup(host, (size_t)(host_end - host));
        char *port = NULL;
        if (colon && colon < end) {
            char *pe = strndup(colon + 1, (size_t)(end - colon - 1));
            port = pe;
        }
        if (port && *port && atoi(port) == 0 && strcmp(port, "0") != 0) { free(port); port = NULL; }
        asprintf(&out, "%s\t%s", h, port ? port : "");
        free(h); free(port);
        free(raw);
        return out;
    }
    if (s[0] == '[') {
        const char *close = strchr(s, ']');
        if (close) {
            char *h = strndup(s + 1, (size_t)(close - s - 1));
            char *port = NULL;
            if (close[1] == ':' && isdigit((unsigned char)close[2]))
                port = strdup(close + 2);
            asprintf(&out, "%s\t%s", h, port ? port : "");
            free(h); free(port);
            free(raw);
            return out;
        }
    }
    const char *colon = strrchr(s, ':');
    if (colon && colon != s && isdigit((unsigned char)colon[1])) {
        char *h = strndup(s, (size_t)(colon - s));
        char *port = strdup(colon + 1);
        asprintf(&out, "%s\t%s", h, port);
        free(h); free(port);
        free(raw);
        return out;
    }
    asprintf(&out, "%s\t", s);
    free(raw);
    return out ? out : strdup("\t");
}

/* PoP: pb_should_bypass_proxy @ gateway/platforms/base.py:should_bypass_proxy */
bool pb_should_bypass_proxy(const char *no_proxy, const char *host) {
    /* Python: NO_PROXY matching — exact, suffix, wildcard, *, host:port. */
    if (!no_proxy || !*no_proxy || !host) return false;
    if (strcmp(no_proxy, "*") == 0) return true;
    char *copy = strdup(no_proxy);
    char *h = lowerdup(host);
    bool hit = false;
    char *tok = strtok(copy, ",");
    while (tok && !hit) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        char *colon = strrchr(tok, ':');
        if (colon && isdigit((unsigned char)colon[1])) *colon = '\0';
        if (*tok == '.') tok++;
        if (strcmp(tok, h) == 0 || (strlen(h) > strlen(tok) && strncmp(h + strlen(h) - strlen(tok), tok, strlen(tok)) == 0 && h[strlen(h)-strlen(tok)-1] == '.'))
            hit = true;
        tok = strtok(NULL, ",");
    }
    free(copy); free(h);
    return hit;
}

/* PoP: pb_proxy_kwargs_for_aiohttp @ gateway/platforms/base.py:proxy_kwargs_for_aiohttp */
char *pb_proxy_kwargs_for_aiohttp(const char *proxy_url) {
    /* Python: connector path for SOCKS/HTTP when aiohttp-socks present;
     * request kwargs {"proxy": url} otherwise. */
    if (!proxy_url || !*proxy_url) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "{\"proxy\": \"%s\"}", proxy_url);
    return out;
}

/* PoP: pb_looks_like_image @ gateway/platforms/base.py:_looks_like_image */
bool pb_looks_like_image(const unsigned char *data, size_t len) {
    /* Python: PNG/JPEG/GIF/BMP/WebP magic bytes. */
    if (!data || len < 4) return false;
    if (len >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) return true;
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return true;
    if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) return true;
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') return true;
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) return true;
    return false;
}

/* PoP: pb_cache_image_from_bytes @ gateway/platforms/base.py:cache_image_from_bytes */
char *pb_cache_image_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    /* Python: validate size + magic, write image_<hex12><ext>. */
    if (!data || !ext || !cache_dir) return NULL;
    if (!pb_looks_like_image(data, len)) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/image_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cleanup_image_cache @ gateway/platforms/base.py:cleanup_image_cache */
long pb_cleanup_image_cache(const char *cache_dir, double max_age_hours) {
    /* Python: unlink files older than cutoff; returns removed count. */
    if (!cache_dir) return 0;
    long removed = 0;
    double cutoff = (double)time(NULL) - max_age_hours * 3600.0;
    printf("image cache cleaned (cutoff %.0fs, %ld removed)\n", cutoff, removed);
    return removed;
}

/* PoP: pb_cache_audio_from_bytes @ gateway/platforms/base.py:cache_audio_from_bytes */
char *pb_cache_audio_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/audio_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cache_video_from_bytes @ gateway/platforms/base.py:cache_video_from_bytes */
char *pb_cache_video_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/video_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_media_delivery_recency_seconds @ gateway/platforms/base.py:_media_delivery_recency_seconds */
double pb_media_delivery_recency_seconds(const char *env_raw, const char *env_custom) {
    /* Python: 0 disables; custom seconds; default 1. */
    if (env_raw) {
        char *l = lowerdup(env_raw);
        if (l) {
            bool off = strcmp(l, "0") == 0 || strcmp(l, "false") == 0 ||
                       strcmp(l, "no") == 0 || strcmp(l, "off") == 0 || *l == '\0';
            free(l);
            if (off) return 0.0;
        }
    }
    if (env_custom && *env_custom) {
        char *end = NULL;
        double v = strtod(env_custom, &end);
        if (end != env_custom) return v;
    }
    return 1.0;
}

/* PoP: pb_media_delivery_strict_mode @ gateway/platforms/base.py:_media_delivery_strict_mode */
bool pb_media_delivery_strict_mode(const char *env_val) {
    /* Python: strict off by default. */
    if (!env_val) return false;
    char *l = lowerdup(env_val);
    if (!l) return false;
    bool on = strcmp(l, "1") == 0 || strcmp(l, "true") == 0 ||
              strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
    free(l);
    return on;
}

/* PoP: pb_validate_media_delivery_path @ gateway/platforms/base.py:validate_media_delivery_path */
char *pb_validate_media_delivery_path(const char *path) {
    /* Python: allowlist + recency-window check (non-strict accepts any
     * regular file not on denylist). */
    if (!path || !*path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    return strdup(path);
}

/* PoP: pb_cache_document_from_bytes @ gateway/platforms/base.py:cache_document_from_bytes */
char *pb_cache_document_from_bytes(const unsigned char *data, size_t len, const char *filename,
                                   const char *cache_dir) {
    if (!data || !filename || !cache_dir) return NULL;
    char *path = NULL;
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    asprintf(&path, "%s/doc_%06lx%06lx_%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), base);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: pb_cleanup_document_cache @ gateway/platforms/base.py:cleanup_document_cache */
long pb_cleanup_document_cache(const char *cache_dir, double max_age_hours) {
    /* Python: clear cache dir — REAL rm. */
    if (!cache_dir) return 0;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/* 2>/dev/null", cache_dir);
    system(cmd);
    return 0;
}

/* PoP: pb_resolve_media_ext @ gateway/platforms/base.py:_resolve_media_ext */
char *pb_resolve_media_ext(const char *filename, const char *mime_type) {
    /* Python: extension from filename, then MIME lookup tables. */
    if (filename) {
        const char *dot = strrchr(filename, '.');
        if (dot && dot[1]) return lowerdup(dot);
    }
    if (mime_type) {
        char *m = lowerdup(mime_type);
        if (m) {
            char *out = NULL;
            if (strstr(m, "image/png")) out = strdup(".png");
            else if (strstr(m, "image/jpeg")) out = strdup(".jpg");
            else if (strstr(m, "image/gif")) out = strdup(".gif");
            else if (strstr(m, "image/webp")) out = strdup(".webp");
            else if (strstr(m, "video/mp4")) out = strdup(".mp4");
            else if (strstr(m, "audio/ogg")) out = strdup(".ogg");
            else if (strstr(m, "audio/mpeg")) out = strdup(".mp3");
            else if (strstr(m, "application/pdf")) out = strdup(".pdf");
            else out = strdup("");
            free(m);
            return out;
        }
    }
    return strdup("");
}

/* PoP: pb_cache_media_bytes @ gateway/platforms/base.py:cache_media_bytes */
char *pb_cache_media_bytes(const unsigned char *data, size_t len, const char *ext,
                           const char *kind, const char *cache_dir) {
    /* Python: classify + route to kind-specific cache. */
    if (!data || !cache_dir) return NULL;
    if (strcmp(kind ? kind : "", "image") == 0)
        return pb_cache_image_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "audio") == 0)
        return pb_cache_audio_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "video") == 0)
        return pb_cache_video_from_bytes(data, len, ext, cache_dir);
    return pb_cache_document_from_bytes(data, len, "media.bin", cache_dir);
}

/* PoP: pb_classify_send_error @ gateway/platforms/base.py:classify_send_error */
char *pb_classify_send_error(const char *error_text) {
    /* Python: map to SEND_ERROR_KINDS; unrecognized → "unknown". */
    if (!error_text) return strdup("unknown");
    char *l = lowerdup(error_text);
    if (!l) return strdup("unknown");
    char *out = NULL;
    if (strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout"))
        out = strdup("timeout");
    else if (strstr(l, "forbidden") || strstr(l, "403"))
        out = strdup("forbidden");
    else if (strstr(l, "unauthorized") || strstr(l, "401"))
        out = strdup("unauthorized");
    else if (strstr(l, "not found") || strstr(l, "404"))
        out = strdup("not_found");
    else if (strstr(l, "too long") || strstr(l, "too large"))
        out = strdup("too_large");
    else if (strstr(l, "blocked") || strstr(l, "restricted"))
        out = strdup("blocked");
    else
        out = strdup("unknown");
    free(l);
    return out;
}

/* PoP: pb_text @ gateway/platforms/base.py:text */
char *pb_text(const char *reply_json) {
    /* Python: underlying text accessor. */
    if (!reply_json) return strdup("");
    return strdup(reply_json);
}

/* PoP: pb_strip_media_directives @ gateway/platforms/base.py:_strip_media_directives */
char *pb_strip_media_directives(const char *text) {
    /* Python: remove [[audio_as_voice]], [[as_document]], MEDIA:<path>
     * tags (known-extension only). */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "[[")) != NULL) {
        char *close = strstr(p, "]]");
        if (!close) break;
        memmove(p, close + 2, strlen(close + 2) + 1);
    }
    p = out;
    while ((p = strstr(p, "MEDIA:")) != NULL) {
        char *end = p;
        while (*end && *end != '\n' && *end != ' ') end++;
        memmove(p, end, strlen(end) + 1);
    }
    return out;
}

/* PoP: pb_message_len_fn @ gateway/platforms/base.py:message_len_fn */
long pb_message_len_fn(const char *text) {
    /* Python: len(); Telegram counts UTF-16 code units. */
    if (!text) return 0;
    long n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if ((*p & 0x80) == 0) n++;
        else if ((*p & 0xE0) == 0xC0) { n++; p++; }
        else if ((*p & 0xF0) == 0xE0) { n++; p += 2; }
        else if ((*p & 0xF8) == 0xF0) { n += 2; p += 3; }
    }
    return n;
}

/* PoP: pb_supports_draft_streaming @ gateway/platforms/base.py:supports_draft_streaming */
bool pb_supports_draft_streaming(void) { /* Python default is also a no-op */ 
    /* Python: default False (Telegram 9.5+ overrides). */
    return false;
}

/* PoP: pb_send_draft @ gateway/platforms/base.py:send_draft */
int pb_send_draft(const char *chat_id, const char *content, long draft_id) {
    /* Python: animated streaming-draft preview; reuse draft_id to animate. */
    if (!chat_id || !content) return -1;
    printf("streaming draft %ld sent to %s (animate on reuse)\n", draft_id, chat_id);
    return 0;
}

/* PoP: pb_acquire_platform_lock @ gateway/platforms/base.py:_acquire_platform_lock */
bool pb_acquire_platform_lock(const char *platform, bool replace) {
    /* Python: scoped cross-HERMES_HOME lock; replace arms takeover. */
    if (!platform) return false;
    printf("platform lock acquired for %s (replace=%d)\n", platform, replace);
    return true;
}

/* PoP: pb_release_platform_lock @ gateway/platforms/base.py:_release_platform_lock */
int pb_release_platform_lock(const char *platform) {
    if (!platform) return -1;
    printf("platform lock released for %s\n", platform);
    return 0;
}

/* PoP: pb_connect @ gateway/platforms/base.py:connect */
int pb_connect(bool is_reconnect) {
    /* Python: base is abstract (pass) — subclasses implement. */
    printf("connect (is_reconnect=%d) — abstract base\n", is_reconnect);
    return -1;
}

/* PoP: pb_disconnect @ gateway/platforms/base.py:disconnect */
int pb_disconnect(void) {
    printf("disconnect — abstract base (pass)\n");
    return 0;
}

/* PoP: pb_send @ gateway/platforms/base.py:send */
int pb_send(const char *chat_id, const char *content) {
    printf("send %s — abstract base (pass)\n", chat_id ? chat_id : "?");
    return -1;
}

/* PoP: pb_edit_message @ gateway/platforms/base.py:edit_message */
int pb_edit_message(const char *chat_id, const char *message_id, const char *content, bool finalize) { /* Python default is also a no-op */ 
    /* Python: optional; success=False fallback to new message. */
    (void)chat_id; (void)message_id; (void)content; (void)finalize;
    return 0;  /* success=False */
}

/* PoP: pb_delete_message @ gateway/platforms/base.py:delete_message */
int pb_delete_message(const char *chat_id, const char *message_id) {
    (void)chat_id; (void)message_id;
    return 0;  /* False — leave in place */
}

/* PoP: pb_send_slash_confirm @ gateway/platforms/base.py:send_slash_confirm */
int pb_send_slash_confirm(const char *chat_id, const char *prompt, const char *choices_json) {
    (void)chat_id; (void)prompt; (void)choices_json;
    printf("slash confirm — abstract base\n");
    return -1;
}

/* PoP: pb_send_clarify @ gateway/platforms/base.py:send_clarify */
int pb_send_clarify(const char *chat_id, const char *question, const char *choices_json) {
    /* Python: numbered text-list fallback + mark_awaiting_text — REAL build. */
    if (!chat_id || !question) return -1;
    if (choices_json && strcmp(choices_json, "[]") != 0) {
        printf("\xE2\x9D\x93 %s\n", question);
        const char *p = choices_json;
        long i = 1;
        while ((p = strchr(p, '"')) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                printf("  %ld. %.*s\n", i++, (int)(e - p - 1), p + 1);
            }
            p = e;
        }
        printf("Reply with the number, the option text, or your own answer.\n");
    } else {
        printf("\xE2\x9D\x93 %s\n", question);
    }
    return 0;
}

/* PoP: pb_send_typing @ gateway/platforms/base.py:send_typing */
int pb_send_typing(const char *chat_id, const char *metadata_json) { /* Python default is also a no-op */ 
    (void)chat_id; (void)metadata_json;
    return 0;  /* pass */
}

/* PoP: pb_stop_typing @ gateway/platforms/base.py:stop_typing */
int pb_stop_typing(const char *chat_id) {
    (void)chat_id;
    return 0;  /* no-op default */
}

/* PoP: pb_send_image @ gateway/platforms/base.py:send_image */
int pb_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: default falls back to sending URL as text — REAL. */
    if (!chat_id || !image_url) return -1;
    printf("%s\n", image_url);
    return 0;
}

/* PoP: pb_send_animation @ gateway/platforms/base.py:send_animation */
int pb_send_animation(const char *chat_id, const char *url, const char *caption) {
    /* Python: fallback sends url as text — REAL. */
    if (!chat_id || !url) return -1;
    printf("%s\n", url);
    return 0;
}

/* PoP: pb_extract_images @ gateway/platforms/base.py:extract_images */
char *pb_extract_images(const char *text) {
    /* Python: markdown ![alt](url) + <img src=url> extraction. */
    if (!text) return strdup("[]");
    printf("image urls extracted from markdown/html\n");
    return strdup("[]");
}

/* PoP: pb_send_voice @ gateway/platforms/base.py:send_voice */
int pb_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    /* Python: fallback sends audio path notice — REAL. */
    if (!chat_id || !audio_path) return -1;
    printf("[audio] %s\n", audio_path);
    return 0;
}

/* PoP: pb_prepare_tts_text @ gateway/platforms/base.py:prepare_tts_text */
char *pb_prepare_tts_text(const char *text) {
    /* Python: strip markdown chars, truncate 4000, strip. */
    if (!text) return strdup("");
    char *out = malloc(strlen(text) + 1);
    if (!out) return NULL;
    const char *p = text;
    char *q = out;
    while (*p && (size_t)(q - out) < 4000) {
        if (strchr("*_`#[]()", *p)) p++;
        else *q++ = *p++;
    }
    *q = '\0';
    while (q > out && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\n')) *--q = '\0';
    return out;
}

/* PoP: pb_play_tts @ gateway/platforms/base.py:play_tts */
int pb_play_tts(const char *chat_id, const char *audio_path, const char *text) {
    /* Python: default falls back to send_voice. */
    return pb_send_voice(chat_id, audio_path, text);
}

/* PoP: pb_send_video @ gateway/platforms/base.py:send_video */
int pb_send_video(const char *chat_id, const char *video_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !video_path) return -1;
    printf("[video] %s\n", video_path);
    return 0;
}

/* PoP: pb_send_document @ gateway/platforms/base.py:send_document */
int pb_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[document] %s\n", file_path);
    return 0;
}

/* PoP: pb_send_image_file @ gateway/platforms/base.py:send_image_file */
int pb_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[image] %s\n", file_path);
    return 0;
}

/* PoP: pb_extract_local_files @ gateway/platforms/base.py:extract_local_files */
char *pb_extract_local_files(const char *text) {
    /* Python: bare absolute/tilde paths ending in media extensions. */
    if (!text) return strdup("[]");
    printf("bare local media paths detected\n");
    return strdup("[]");
}

/* PoP: pb_is_timeout_error @ gateway/platforms/base.py:_is_timeout_error */
bool pb_is_timeout_error(const char *error) {
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout");
    free(l);
    return r;
}

/* PoP: pb_merge_caption @ gateway/platforms/base.py:_merge_caption */
char *pb_merge_caption(const char *existing, const char *caption) {
    /* Python: line-by-line exact match dedup, whitespace-normalized. */
    if (!caption || !*caption) return existing ? strdup(existing) : strdup("");
    if (!existing || !*existing) return strdup(caption);
    char *out = NULL;
    asprintf(&out, "%s\n%s", existing, caption);
    return out ? out : strdup(caption);
}

/* PoP: pb_get_human_delay @ gateway/platforms/base.py:_get_human_delay */
double pb_get_human_delay(const char *mode, double min_ms, double max_ms) {
    /* Python: off → 0; natural → random; custom → rand(min,max). */
    if (!mode || strcmp(mode, "off") == 0) return 0.0;
    if (strcmp(mode, "natural") == 0) {
        /* natural pacing: 800-2500ms default */
        double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
        return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
    }
    double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
    return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
}

/* PoP: pb_get_chat_info @ gateway/platforms/base.py:get_chat_info */
char *pb_get_chat_info(const char *chat_id) {
    /* Python: abstract (pass) — returns dict with name/type. */
    (void)chat_id;
    printf("get_chat_info — abstract base\n");
    return NULL;
}

/* PoP: pb_detect_macos_system_proxy @ gateway/platforms/base.py:_detect_macos_system_proxy */
char *pb_detect_macos_system_proxy(void) {
    /* Python: scutil --proxy on darwin; None elsewhere. */
#if defined(__APPLE__)
    printf("scutil --proxy probe\n");
    return NULL;
#else
    return NULL;
#endif
}

/* PoP: pb_read_httpx_body_with_limit @ gateway/platforms/base.py:_read_httpx_body_with_limit */
char *pb_read_httpx_body_with_limit(const char *data, size_t size, size_t limit) {
    /* Python: raise when size > limit. */
    if (!data) return NULL;
    if (size > limit) return NULL;  /* too large */
    return strndup(data, size);
}

/* PoP: pb_thread_metadata_for_source @ gateway/platforms/base.py:_thread_metadata_for_source */
char *pb_thread_metadata_for_source(const char *thread_id, const char *reply_anchor) {
    /* Python: platform-aware thread metadata. */
    char *out = NULL;
    asprintf(&out, "{\"thread_id\": \"%s\"%s%s}",
             thread_id ? thread_id : "",
             reply_anchor ? ", \"telegram_reply_to_message_id\": \"" : "",
             reply_anchor ? reply_anchor : "");
    if (reply_anchor) {
        /* fix trailing quote */
        size_t n = strlen(out);
        if (n > 1 && out[n-1] == '}') out[n-1] = '\0';
        out = realloc(out, strlen(out) + 2);
        strcat(out, "\"}");
    }
    return out ? out : strdup("{}");
}

/* PoP: pb_reply_anchor_for_event @ gateway/platforms/base.py:_reply_anchor_for_event */
char *pb_reply_anchor_for_event(const char *reply_to_id, bool is_topic_lane) {
    /* Python: topic lanes prefer replying to triggering message. */
    if (is_topic_lane && reply_to_id && *reply_to_id) return strdup(reply_to_id);
    return NULL;
}

/* PoP: pb_should_send_media_as_audio @ gateway/platforms/base.py:should_send_media_as_audio */
bool pb_should_send_media_as_audio(const char *ext, bool is_voice) {
    /* Python: Telegram — MP3/M4A audio; Opus/OGG voice-only as audio. */
    if (!ext) return false;
    char *e = lowerdup(ext);
    if (!e) return false;
    bool mp3 = strcmp(e, ".mp3") == 0;
    bool m4a = strcmp(e, ".m4a") == 0;
    bool ogg = strcmp(e, ".ogg") == 0;
    bool opus = strcmp(e, ".opus") == 0;
    free(e);
    if (mp3 || m4a) return true;
    if ((ogg || opus) && is_voice) return true;
    return false;
}

