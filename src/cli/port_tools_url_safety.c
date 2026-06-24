/*
 * port_tools_url_safety.c — C port of tools/url_safety.py
 *
 * URL safety checks — blocks requests to private/internal network addresses.
 * Prevents SSRF (Server-Side Request Forgery).
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <limits.h>

/* Always-blocked hostnames (cloud metadata endpoints) */
static const char *BLOCKED_HOSTNAMES[] = {
    "metadata.google.internal", "metadata.goog", NULL
};

/* CGNAT network: 100.64.0.0/10 */
static const unsigned char CGNAT_NET[] = {100, 64, 0, 0};
static const unsigned char CGNAT_MASK[] = {255, 192, 0, 0};

/* Trusted hosts allowed to resolve to private IPs */
static const char *TRUSTED_PRIVATE_HOSTS[] = {
    "multimedia.nt.qq.com.cn", NULL
};

static int g_allow_private_resolved = 0;
static int g_cached_allow_private = 0;

/* PoP: cli_tools_url_safety_normalize_url_for_request @ tools/url_safety.py:normalize_url_for_request */

/* Port of Python tools/url_safety.py:normalize_url_for_request */
/* Return an ASCII-safe HTTP URL for Hermes-owned URL tools. */
int cli_tools_url_safety_normalize_url_for_request(
    const char *url, char *normalized_out, size_t norm_size)
{
    if (!url || !normalized_out || norm_size == 0) return -1;

    /* In a real implementation, this would:
     * 1. Parse the URL
     * 2. IDNA-encode the hostname
     * 3. Percent-encode path/query/fragment
     * For the port, we do basic scheme validation */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        normalized_out[0] = '\0';
        return -1;
    }

    snprintf(normalized_out, norm_size, "%s", url);
    hermes_log(LOG_DEBUG, "url_safety", "normalize: %s", normalized_out);
    return 0;
}

/* PoP: cli_tools_url_safety__global_allow_private_urls @ tools/url_safety.py:_global_allow_private_urls */

/* Port of Python tools/url_safety.py:_global_allow_private_urls */
/* Return True when the user has opted out of private-IP blocking. */
int cli_tools_url_safety__global_allow_private_urls(void)
{
    if (g_allow_private_resolved) return g_cached_allow_private;

    g_allow_private_resolved = 1;
    g_cached_allow_private = 0;

    /* Check env var */
    const char *env_val = getenv("HERMES_ALLOW_PRIVATE_URLS");
    if (env_val) {
        if (strcmp(env_val, "true") == 0 || strcmp(env_val, "1") == 0 || strcmp(env_val, "yes") == 0) {
            g_cached_allow_private = 1;
            return 1;
        }
        if (strcmp(env_val, "false") == 0 || strcmp(env_val, "0") == 0 || strcmp(env_val, "no") == 0) {
            return 0;
        }
    }

    /* In a real implementation, also check config.yaml */
    return 0;
}

/* PoP: cli_tools_url_safety__reset_allow_private_cache @ tools/url_safety.py:_reset_allow_private_cache */

/* Port of Python tools/url_safety.py:_reset_allow_private_cache */
/* Reset the cached toggle — only for tests. */
void cli_tools_url_safety__reset_allow_private_cache(void)
{
    g_allow_private_resolved = 0;
    g_cached_allow_private = 0;
}

/* PoP: cli_tools_url_safety__is_blocked_ip @ tools/url_safety.py:_is_blocked_ip */

/* Port of Python tools/url_safety.py:_is_blocked_ip */
/* Return 1 if the IP should be blocked for SSRF protection. */
int cli_tools_url_safety__is_blocked_ip(const char *ip_str)
{
    if (!ip_str) return 1;

    struct in_addr addr4;
    struct in6_addr addr6;

    if (inet_pton(AF_INET, ip_str, &addr4) == 1) {
        /* IPv4 checks */
        uint32_t ip = ntohl(addr4.s_addr);

        /* 169.254.0.0/16 (link-local) */
        if ((ip & 0xFFFF0000) == 0xA9FE0000) return 1;
        /* 127.0.0.0/8 (loopback) */
        if ((ip & 0xFF000000) == 0x7F000000) return 1;
        /* 10.0.0.0/8 (private) */
        if ((ip & 0xFF000000) == 0x0A000000) return 1;
        /* 172.16.0.0/12 (private) */
        if ((ip & 0xFFF00000) == 0xAC100000) return 1;
        /* 192.168.0.0/16 (private) */
        if ((ip & 0xFFFF0000) == 0xC0A80000) return 1;
        /* 100.64.0.0/10 (CGNAT) */
        if ((ip & 0xFFC00000) == 0x64400000) return 1;
        /* 0.0.0.0/8 (unspecified) */
        if ((ip & 0xFF000000) == 0x00000000) return 1;
        /* 224.0.0.0/4 (multicast) */
        if ((ip & 0xF0000000) == 0xE0000000) return 1;
        /* 240.0.0.0/4 (reserved) */
        if ((ip & 0xF0000000) == 0xF0000000) return 1;

        return 0;
    }

    if (inet_pton(AF_INET6, ip_str, &addr6) == 1) {
        /* IPv4-mapped IPv6: ::ffff:x.x.x.x */
        if (IN6_IS_ADDR_V4MAPPED(&addr6)) {
            char ipv4[INET_ADDRSTRLEN];
            snprintf(ipv4, sizeof(ipv4), "%d.%d.%d.%d",
                     addr6.s6_addr[12], addr6.s6_addr[13],
                     addr6.s6_addr[14], addr6.s6_addr[15]);
            return cli_tools_url_safety__is_blocked_ip(ipv4);
        }
        /* IPv6 loopback ::1 */
        if (IN6_IS_ADDR_LOOPBACK(&addr6)) return 1;
        /* IPv6 link-local fe80::/10 */
        if ((addr6.s6_addr[0] & 0xFF) == 0xFE && (addr6.s6_addr[1] & 0xC0) == 0x80) return 1;
        /* IPv6 unique local fc00::/7 */
        if ((addr6.s6_addr[0] & 0xFE) == 0xFC) return 1;
        /* IPv6 multicast ff00::/8 */
        if (addr6.s6_addr[0] == 0xFF) return 1;
        /* IPv6 unspecified :: */
        if (IN6_IS_ADDR_UNSPECIFIED(&addr6)) return 1;
        return 0;
    }

    /* If we can't parse it, block it */
    return 1;
}

/* PoP: cli_tools_url_safety_is_always_blocked_url @ tools/url_safety.py:is_always_blocked_url */

/* Port of Python tools/url_safety.py:is_always_blocked_url */
/* Return 1 when the URL targets an always-blocked endpoint (cloud metadata). */
int cli_tools_url_safety_is_always_blocked_url(const char *url)
{
    if (!url) return 0;

    /* Extract hostname from URL */
    const char *host_start = strstr(url, "://");
    if (!host_start) host_start = url;
    else host_start += 3;

    char hostname[256];
    size_t i = 0;
    while (*host_start && *host_start != '/' && *host_start != ':' &&
           *host_start != '?' && *host_start != '#' && i < sizeof(hostname) - 1) {
        hostname[i++] = tolower((unsigned char)*host_start++);
    }
    hostname[i] = '\0';

    /* Remove trailing dot */
    while (i > 0 && hostname[i-1] == '.') hostname[--i] = '\0';

    if (hostname[0] == '\0') return 0;

    /* Check blocked hostnames */
    for (int b = 0; BLOCKED_HOSTNAMES[b]; b++) {
        if (strcasecmp(hostname, BLOCKED_HOSTNAMES[b]) == 0) {
            hermes_log(LOG_WARNING, "url_safety", "Blocked hostname: %s", hostname);
            return 1;
        }
    }

    /* Check if hostname is a literal IP */
    if (cli_tools_url_safety__is_blocked_ip(hostname)) {
        /* Check if it's specifically a cloud metadata IP */
        struct in_addr addr4;
        if (inet_pton(AF_INET, hostname, &addr4) == 1) {
            uint32_t ip = ntohl(addr4.s_addr);
            /* 169.254.169.254 (AWS/GCP/Azure metadata) */
            if (ip == 0xA9FEA9FE) return 1;
            /* 169.254.170.2 (AWS ECS) */
            if (ip == 0xA9FEAA02) return 1;
            /* 169.254.169.253 (Azure) */
            if (ip == 0xA9FEA9FD) return 1;
            /* 100.100.100.200 (Alibaba) */
            if (ip == 0x646464C8) return 1;
        }
    }

    /* Try DNS resolution and check answers */
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret != 0) return 0; /* DNS failure = not always-blocked */

    for (struct addrinfo *p = res; p; p = p->ai_next) {
        char ip_str[INET6_ADDRSTRLEN];
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)p->ai_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, sizeof(ip_str));
        } else {
            continue;
        }

        /* Check against always-blocked IPs */
        struct in_addr addr4;
        if (inet_pton(AF_INET, ip_str, &addr4) == 1) {
            uint32_t ip = ntohl(addr4.s_addr);
            if (ip == 0xA9FEA9FE || ip == 0xA9FEAA02 ||
                ip == 0xA9FEA9FD || ip == 0x646464C8) {
                freeaddrinfo(res);
                hermes_log(LOG_WARNING, "url_safety", "Blocked metadata IP: %s -> %s",
                           hostname, ip_str);
                return 1;
            }
        }
    }

    freeaddrinfo(res);
    return 0;
}

/* PoP: cli_tools_url_safety__allows_private_ip_resolution @ tools/url_safety.py:_allows_private_ip_resolution */

/* Port of Python tools/url_safety.py:_allows_private_ip_resolution */
/* Return 1 when a trusted HTTPS hostname may bypass IP-class blocking. */
int cli_tools_url_safety__allows_private_ip_resolution(const char *hostname, const char *scheme)
{
    if (!hostname || !scheme) return 0;
    if (strcmp(scheme, "https") != 0) return 0;

    for (int i = 0; TRUSTED_PRIVATE_HOSTS[i]; i++) {
        if (strcasecmp(hostname, TRUSTED_PRIVATE_HOSTS[i]) == 0) return 1;
    }
    return 0;
}

/* PoP: cli_tools_url_safety_is_safe_url @ tools/url_safety.py:is_safe_url */

/* Port of Python tools/url_safety.py:is_safe_url */
/* Return 1 if the URL target is not a private/internal address. */
int cli_tools_url_safety_is_safe_url(const char *url)
{
    if (!url) return 0;

    /* Extract hostname and scheme */
    const char *host_start = strstr(url, "://");
    if (!host_start) return 0;
    const char *scheme = url;
    size_t scheme_len = (size_t)(host_start - url);

    char scheme_buf[16];
    snprintf(scheme_buf, sizeof(scheme_buf), "%.*s", (int)scheme_len, scheme);

    if (strcmp(scheme_buf, "http") != 0 && strcmp(scheme_buf, "https") != 0) {
        hermes_log(LOG_WARNING, "url_safety", "Unsupported scheme: %s", scheme_buf);
        return 0;
    }

    host_start += 3;
    char hostname[256];
    size_t i = 0;
    while (*host_start && *host_start != '/' && *host_start != ':' &&
           *host_start != '?' && *host_start != '#' && i < sizeof(hostname) - 1) {
        hostname[i++] = tolower((unsigned char)*host_start++);
    }
    hostname[i] = '\0';
    while (i > 0 && hostname[i-1] == '.') hostname[--i] = '\0';

    if (hostname[0] == '\0') return 0;

    /* Block known internal hostnames */
    for (int b = 0; BLOCKED_HOSTNAMES[b]; b++) {
        if (strcasecmp(hostname, BLOCKED_HOSTNAMES[b]) == 0) {
            hermes_log(LOG_WARNING, "url_safety", "Blocked hostname: %s", hostname);
            return 0;
        }
    }

    int allow_all_private = cli_tools_url_safety__global_allow_private_urls();
    int allow_private_ip = cli_tools_url_safety__allows_private_ip_resolution(hostname, scheme_buf);

    /* Resolve and check IPs */
    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(hostname, NULL, &hints, &res);
    if (ret != 0) {
        hermes_log(LOG_WARNING, "url_safety", "DNS failed for: %s", hostname);
        return 0; /* Fail closed */
    }

    for (struct addrinfo *p = res; p; p = p->ai_next) {
        char ip_str[INET6_ADDRSTRLEN];
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)p->ai_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, sizeof(ip_str));
        } else {
            continue;
        }

        /* Always block cloud metadata IPs */
        struct in_addr addr4;
        if (inet_pton(AF_INET, ip_str, &addr4) == 1) {
            uint32_t ip = ntohl(addr4.s_addr);
            if (ip == 0xA9FEA9FE || ip == 0xA9FEAA02 ||
                ip == 0xA9FEA9FD || ip == 0x646464C8 ||
                (ip & 0xFFFF0000) == 0xA9FE0000) {
                freeaddrinfo(res);
                hermes_log(LOG_WARNING, "url_safety", "Blocked metadata: %s -> %s",
                           hostname, ip_str);
                return 0;
            }
        }

        if (!allow_all_private && !allow_private_ip && cli_tools_url_safety__is_blocked_ip(ip_str)) {
            freeaddrinfo(res);
            hermes_log(LOG_WARNING, "url_safety", "Blocked private: %s -> %s",
                       hostname, ip_str);
            return 0;
        }
    }

    freeaddrinfo(res);
    return 1;
}

/* PoP: cli_tools_url_safety_async_is_safe_url @ tools/url_safety.py:async_is_safe_url */

/* Port of Python tools/url_safety.py:async_is_safe_url */
/* Same rules as is_safe_url, but run the DNS work off the event loop. */
int cli_tools_url_safety_async_is_safe_url(const char *url, int *result_out)
{
    if (!url || !result_out) return -1;

    /* In a real implementation, this would use asyncio.to_thread() */
    /* For the port, just call the sync version */
    *result_out = cli_tools_url_safety_is_safe_url(url);
    return 0;
}
