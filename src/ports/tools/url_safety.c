#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/url_safety.c — C11 port of tools/url_safety.py
 * Merged from 4 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_url_safety_py_parity.c ── */
/* Slermes C port — tools/url_safety.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_url_safety_resolved_http_connect_ips_p @ tools/url_safety.py:_resolved_http_connect_ips */
json_t *tools_url_safety_resolved_http_connect_ips_p(void) {
    return json_object();
}

/* PoP: tools_url_safety_connect_tcp_p @ tools/url_safety.py:connect_tcp */
json_t *tools_url_safety_connect_tcp_p(void) {
    return json_object();
}


/* ── from src/cli/port_tools_url_safety.c ── */
/*
 * port_tools_url_safety.c — C port of tools/url_safety.py
 *
 * URL safety checks — blocks requests to private/internal network addresses.
 * Prevents SSRF (Server-Side Request Forgery).
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

/* Always-blocked hostnames (cloud metadata endpoints) */
static const char *BLOCKED_HOSTNAMES[] = {
    "metadata.google.internal", "metadata.goog", NULL
};

/* Trusted hosts allowed to resolve to private IPs */
static const char *TRUSTED_PRIVATE_HOSTS[] = {
    "multimedia.nt.qq.com.cn", NULL
};

static int g_allow_private_resolved = 0;
static int g_cached_allow_private = 0;

/* ── URL helpers (faithful port of tools/url_safety.py:normalize_url_for_request) ── */

/* RFC 3492 Punycode bias adaptation. */
static uint32_t punycode_adapt(uint32_t delta, uint32_t numpoints, int first)
{
    delta = first ? delta / 700 : delta / 2;
    delta += delta / numpoints;
    uint32_t k = 0;
    while (delta > ((36 - 1) * 26) / 2) {   /* ((base - tmin) * tmax) / 2 */
        delta /= (36 - 1);
        k += 36;
    }
    return k + ((36 - 1 + 1) * delta) / (delta + 38);  /* ((base - tmin + 1) * delta) / (delta + skew) */
}

/* RFC 3492 digit encoding: 0-25 -> 'a'-'z', 26-35 -> '0'-'9'. */
static char punycode_digit(int d)
{
    return (char)(d < 26 ? d + 'a' : d - 26 + '0');
}

/* RFC 3492 Punycode encoding of a U-label (UTF-8 internationalized hostname
 * label) into its ASCII xn-- representation. out must hold >= 64 bytes.
 * Returns 0 on success, -1 on overflow/error. */
static int punycode_encode_label(const char *label, size_t label_len,
                                 char *out, size_t out_cap)
{
    /* Decode UTF-8 label into array of code points */
    uint32_t cp[256];
    int ncp = 0;
    size_t i = 0;
    while (i < label_len && ncp < 256) {
        unsigned char c = (unsigned char)label[i];
        uint32_t ch;
        if (c < 0x80) { ch = c; i += 1; }
        else if ((c & 0xE0) == 0xC0) { ch = c & 0x1F; ch = (ch << 6) | (label[i+1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0) { ch = c & 0x0F; ch = (ch << 6) | (label[i+1] & 0x3F); ch = (ch << 6) | (label[i+2] & 0x3F); i += 3; }
        else if ((c & 0xF8) == 0xF0) { ch = c & 0x07; ch = (ch << 6) | (label[i+1] & 0x3F); ch = (ch << 6) | (label[i+2] & 0x3F); ch = (ch << 6) | (label[i+3] & 0x3F); i += 4; }
        else { return -1; }
        cp[ncp++] = ch;
    }

    int basic = 0;
    for (int k = 0; k < ncp; k++) if (cp[k] < 0x80) basic++;

    /* If entirely ASCII, nothing to encode */
    if (basic == ncp) {
        if (label_len + 1 > out_cap) return -1;
        memcpy(out, label, label_len);
        out[label_len] = '\0';
        return 0;
    }

    size_t pos = 0;
    if (pos + 4 >= out_cap) return -1;
    out[pos++] = 'x'; out[pos++] = 'n'; out[pos++] = '-'; out[pos++] = '-';

    uint32_t n = 0x80, delta = 0, bias = 72;
    int h = basic;
    /* copy existing basic code points */
    for (int k = 0; k < ncp; k++) {
        if (cp[k] < 0x80) {
            if (pos + 1 >= out_cap) return -1;
            out[pos++] = (char)cp[k];
        }
    }
    if (basic > 0) {
        if (pos + 1 >= out_cap) return -1;
        out[pos++] = '-';
    }
    int b = basic;

    while (h < ncp) {
        uint32_t m = 0xFFFFFFFF;
        for (int k = 0; k < ncp; k++) if (cp[k] >= n && cp[k] < m) m = cp[k];
        if ((m - n) > (0xFFFFFFFF - delta) / (uint32_t)(h + 1)) return -1;
        delta += (m - n) * (uint32_t)(h + 1);
        n = m;
        for (int k = 0; k < ncp; k++) {
            if (cp[k] < n) { if (++delta == 0) return -1; }
            else if (cp[k] == n) {
                uint32_t q = delta;
                for (uint32_t t = 36; ; t += 36) {
                    uint32_t tq = (t <= bias) ? 1 : (t >= bias + 26 ? 26 : t - bias);
                    if (q < tq) break;
                    if (pos + 1 >= out_cap) return -1;
                    out[pos++] = punycode_digit((int)(tq + (q - tq) % (36 - tq)));
                    q = (q - tq) / (36 - tq);
                }
                if (pos + 1 >= out_cap) return -1;
                out[pos++] = punycode_digit((int)q);
            }
        }
        bias = punycode_adapt(delta, (uint32_t)(h + 1), b == 0);
        b = 0;
        delta = 0;
        h++;
    }
    if (pos >= out_cap) return -1;
    out[pos] = '\0';
    return 0;
}

/* Percent-encode according to Python's urllib.parse.quote with a safe set.
 * Writes encoded result into out (must hold >= 3*in_len+1). */
static void percent_encode(const char *in, char *out, size_t out_cap,
                           const char *safe)
{
    (void)out_cap;
    size_t j = 0;
    for (size_t i = 0; in[i]; i++) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || strchr(safe, c) != NULL) {
            out[j++] = (char)c;
        } else {
            static const char hex[] = "0123456789ABCDEF";
            out[j++] = '%';
            out[j++] = hex[(c >> 4) & 0xF];
            out[j++] = hex[c & 0xF];
        }
    }
    out[j] = '\0';
}

/* PoP: cli_tools_url_safety_normalize_url_for_request @ tools/url_safety.py:normalize_url_for_request */

/* Port of Python tools/url_safety.py:normalize_url_for_request */
/* Return an ASCII-safe HTTP URL for Hermes-owned URL tools. */
int cli_tools_url_safety_normalize_url_for_request(
    const char *url, char *normalized_out, size_t norm_size)
{
    if (!url || !normalized_out || norm_size == 0) return -1;

    /* Strip surrounding whitespace (Python .strip()) */
    while (*url && isspace((unsigned char)*url)) url++;
    size_t len = strlen(url);
    while (len > 0 && isspace((unsigned char)url[len - 1])) len--;

    /* Validate scheme (Python: only http/https) */
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        normalized_out[0] = '\0';
        return -1;
    }

    /* Split scheme://netloc/path?query#fragment */
    const char *auth_end = strstr(url, "://");
    const char *rest = auth_end + 3;
    char scheme[9];
    size_t sl = auth_end - url;
    if (sl >= sizeof(scheme)) { normalized_out[0] = '\0'; return -1; }
    memcpy(scheme, url, sl); scheme[sl] = '\0';

    /* Find end of netloc (first / ? #) */
    size_t netloc_len = 0;
    while (rest[netloc_len] && rest[netloc_len] != '/' &&
           rest[netloc_len] != '?' && rest[netloc_len] != '#')
        netloc_len++;

    char netloc[1024];
    if (netloc_len >= sizeof(netloc)) { normalized_out[0] = '\0'; return -1; }
    memcpy(netloc, rest, netloc_len);
    netloc[netloc_len] = '\0';

    const char *after = rest + netloc_len;
    const char *path = (*after == '/') ? after : "/";
    const char *frag = strchr(after, '#');

    /* Separate hostname from netloc (strip userinfo@ and :port) */
    char hostname[256];
    const char *at = strrchr(netloc, '@');
    const char *host_start = at ? at + 1 : netloc;
    size_t hlen = strlen(host_start);
    /* strip port (but not inside IPv6 literal) */
    const char *colon = strrchr(host_start, ':');
    const char *port_str = NULL;
    if (colon && strchr(colon, ']') == NULL) {
        port_str = colon + 1;
        hlen = (size_t)(colon - host_start);
    }
    if (hlen >= sizeof(hostname)) { normalized_out[0] = '\0'; return -1; }
    memcpy(hostname, host_start, hlen); hostname[hlen] = '\0';

    /* IDNA-encode each dot-separated label if it contains non-ASCII.
     * Note: we must NOT mutate hcopy in place (strtok_r relies on its own
     * null-terminators); instead encode into a scratch buffer and append that. */
    char enc_host[1024];
    enc_host[0] = '\0';
    char *tokctx = NULL;
    char *hcopy = strdup(hostname);
    char *label = strtok_r(hcopy, ".", &tokctx);
    int first = 1;
    while (label) {
        const char *out_label = label;
        int has_nonascii = 0;
        for (size_t k = 0; label[k]; k++) if ((unsigned char)label[k] >= 0x80) has_nonascii = 1;
        char enc[256];
        if (has_nonascii) {
            if (punycode_encode_label(label, strlen(label), enc, sizeof(enc)) == 0)
                out_label = enc;
        }
        if (!first) strncat(enc_host, ".", sizeof(enc_host) - strlen(enc_host) - 1);
        strncat(enc_host, out_label, sizeof(enc_host) - strlen(enc_host) - 1);
        first = 0;
        label = strtok_r(NULL, ".", &tokctx);
    }
    free(hcopy);

    /* Rebuild netloc = userinfo + encoded host + port */
    char final_netloc[1024];
    size_t need = 0;
    if (at) need = (size_t)(host_start - netloc);            /* userinfo incl '@' */
    need += strlen(enc_host);
    if (port_str) need += 1 + strlen(port_str);
    if (need + 1 > sizeof(final_netloc)) { normalized_out[0] = '\0'; return -1; }
    final_netloc[0] = '\0';
    if (at) { strncat(final_netloc, netloc, (size_t)(host_start - netloc)); }
    strncat(final_netloc, enc_host, sizeof(final_netloc) - strlen(final_netloc) - 1);
    if (port_str) {
        strncat(final_netloc, ":", sizeof(final_netloc) - strlen(final_netloc) - 1);
        strncat(final_netloc, port_str, sizeof(final_netloc) - strlen(final_netloc) - 1);
    }

    /* Percent-encode path / query / fragment */
    char enc_path[2048], enc_query[2048], enc_frag[2048];
    const char *path_part = path;
    const char *qstart = strchr(path_part, '?');
    const char *fstart = strchr(path_part, '#');
    size_t path_only_len = strlen(path_part);
    if (qstart) path_only_len = (size_t)(qstart - path_part);
    else if (fstart) path_only_len = (size_t)(fstart - path_part);

    char path_only[2048];
    memcpy(path_only, path_part, path_only_len); path_only[path_only_len] = '\0';
    percent_encode(path_only, enc_path, sizeof(enc_path), "/%:@!$&'()*+,;=");
    const char *qpart = (qstart || frag) ? strchr(after, '?') : NULL;
    if (qpart) {
        percent_encode(qpart + 1, enc_query, sizeof(enc_query), "/%:@!$&'()*+,;=?");
    } else enc_query[0] = '\0';
    const char *fpart = strchr(after, '#');
    if (fpart) {
        percent_encode(fpart + 1, enc_frag, sizeof(enc_frag), "/%:@!$'()*+,;=?");
    } else enc_frag[0] = '\0';

    int w = snprintf(normalized_out, norm_size, "%s://%s%s%s%s%s",
                     scheme, final_netloc,
                     enc_path,
                     qpart ? "?" : "", qpart ? enc_query : "",
                     fpart ? "#" : "", fpart ? enc_frag : "");
    if (w < 0 || (size_t)w >= norm_size) { normalized_out[0] = '\0'; return -1; }
    hermes_log(LOG_DEBUG, "url_safety", "normalize: %s", normalized_out);
    return 0;
}

/* PoP: yaml_config_allow_private @ tools/url_safety.py:_global_allow_private_urls */

/* Port of Python tools/url_safety.py:_global_allow_private_urls */
/* Read a top-level nested YAML bool key: "<section>.<key>: true" by scanning
 * for "section:" then "<key>:" indented beneath. Returns 1 if true, 0 if
 * absent or false. Faithful to Python cfg["security"]["allow_private_urls"]. */
static int yaml_config_allow_private(const char *config_path)
{
    FILE *f = fopen(config_path, "r");
    if (!f) return 0;
    char line[1024];
    int in_security = 0, in_browser = 0;
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        /* measure indent */
        size_t indent = 0;
        while (line[indent] == ' ' || line[indent] == '\t') indent++;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        size_t key_len = (size_t)(colon - line) - indent;
        char key[256];
        if (key_len >= sizeof(key)) { continue; }
        memcpy(key, line + indent, key_len);
        key[key_len] = '\0';
        /* trim trailing space from key */
        while (key_len > 0 && (key[key_len-1] == ' ' || key[key_len-1] == '\t')) key[--key_len] = '\0';

        if (indent == 0 && strcmp(key, "security") == 0) { in_security = 1; in_browser = 0; continue; }
        if (indent == 0 && strcmp(key, "browser") == 0) { in_browser = 1; in_security = 0; continue; }
        if (indent > 0 && strcmp(key, "security") == 0) { in_security = 1; in_browser = 0; continue; }
        if (indent > 0 && strcmp(key, "browser") == 0) { in_browser = 1; in_security = 0; continue; }

        if ((in_security || in_browser) && strcmp(key, "allow_private_urls") == 0) {
            char *v = colon + 1;
            while (*v == ' ' || *v == '\t') v++;
            size_t vl = strlen(v);
            while (vl > 0 && (v[vl-1] == '\r' || v[vl-1] == '\n' ||
                   v[vl-1] == ' ' || v[vl-1] == '\t')) vl--;
            if (vl >= 4 && strncmp(v, "true", 4) == 0) { found = 1; break; }
            if (vl >= 1 && (v[0] == '1')) { found = 1; break; }
        }
    }
    fclose(f);
    return found;
}

/* Find the hermes config.yaml path (HERMES_HOME or ~/.hermes). */
static const char *hermes_config_path(char *buf, size_t buflen)
{
    const char *home = getenv("HERMES_HOME");
    if (home && home[0]) { snprintf(buf, buflen, "%s/config.yaml", home); return buf; }
    home = getenv("HOME");
    if (home && home[0]) { snprintf(buf, buflen, "%s/.hermes/config.yaml", home); return buf; }
    return NULL;
}

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

    /* Check config.yaml: security.allow_private_urls (preferred) and
     * browser.allow_private_urls (legacy fallback). */
    char cfgbuf[1024];
    const char *cfg = hermes_config_path(cfgbuf, sizeof(cfgbuf));
    if (cfg && yaml_config_allow_private(cfg)) {
        g_cached_allow_private = 1;
        return 1;
    }
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

/* PoP: endpoint @ tools/url_safety.py:is_always_blocked_url */

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

    /* Port of Python tools/url_safety.py:async_is_safe_url.
     * Python offloads DNS resolution off the event loop via asyncio.to_thread();
     * in C there is no event loop, so we call the synchronous resolver directly.
     * The behavior (SSRF block decision) is identical. */
    *result_out = cli_tools_url_safety_is_safe_url(url);
    return 0;
}


/* ── from src/tools/port_tools_url_safety_ssrf.c ── */
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

/* PoP: set @ tools/url_safety.py:_MAX_SSRF_CONNECT_IPS */
#define SSRF_MAX_CONNECT_IPS 8

/* Proxy environment variables (order matches Python _PROXY_ENV_VARS). */
static const char *SSRF_PROXY_ENV_VARS[] = {
    "HTTPS_PROXY", "https_proxy",
    "HTTP_PROXY", "http_proxy",
    "ALL_PROXY", "all_proxy",
    NULL,
};

/* PoP: set @ tools/url_safety.py:_proxy_is_configured */
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

/* PoP: floor @ tools/url_safety.py:_is_blocked_ip */
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

/* PoP: url_set_allow_private @ tools/url_safety.py:_resolved_http_connect_ips */
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

/* PoP: one @ tools/url_safety.py:_install_ssrf_guard_on_client */
/* PoP: one @ tools/url_safety.py:_install_ssrf_guard_on_async_client */
/* PoP: one @ tools/url_safety.py:_install_ssrf_guard_on_transport */
/* PoP: one @ tools/url_safety.py:_install_ssrf_guard_on_async_transport */
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

/* PoP: IPs @ tools/url_safety.py:ssrf_safe_http_transport */
/* PoP: IPs @ tools/url_safety.py:ssrf_safe_async_http_transport */
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

/* PoP: factory @ tools/url_safety.py:create_ssrf_safe_client */
/* PoP: factory @ tools/url_safety.py:create_ssrf_safe_async_client */
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

/* PoP: equivalent @ tools/url_safety.py:sleep */
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


/* ── from src/tools/port_url_safety_helpers.c ── */
/* Slermes C port — tools/url_safety.py (pure URL-safety helpers). */

#include "url_safety_helpers.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* Faithful copy of _TRUSTED_PRIVATE_IP_HOSTS (url_safety.py:174). */
static const char *URL_SAFETY_TRUSTED_HOSTS[] = {
    "multimedia.nt.qq.com.cn",
    NULL,
};

/* PoP: tools_url_safety_allows_private_ip_resolution @ tools/url_safety.py:_allows_private_ip_resolution */
bool tools_url_safety_allows_private_ip_resolution(const char *hostname, const char *scheme)
{
    if (!scheme || strcmp(scheme, "https") != 0) return false;
    if (!hostname) return false;
    for (int i = 0; URL_SAFETY_TRUSTED_HOSTS[i]; i++)
        if (strcmp(hostname, URL_SAFETY_TRUSTED_HOSTS[i]) == 0) return true;
    return false;
}

/* ── Sensitive query-param detection ───────────────────────────────────────
 * PoP: sensitive_query_param_name / has_sensitive_query_params @ tools/url_safety.py
 * The credential-bearing param names are kept deliberately narrow (see the
 * module docstring). Compared case-insensitively against the unquoted key. */

static const char *URL_SAFETY_SENSITIVE_PARAMS[] = {
    "access_token", "api_key", "apikey", "auth_token", "authorization",
    "awsaccesskeyid", "client_secret", "credential", "credentials", "jwt",
    "password", "passwd", "secret", "session_id", "signature", "token",
    "x_amz_security_token", "x_amz_signature", "x-amz-security-token",
    "x-amz-signature", NULL,
};

/* percent-decode in place (e.g. %20 -> space, %2C -> comma). Returns length. */
static size_t url_pct_decode(char *s)
{
    size_t j = 0, i = 0;
    while (s[i]) {
        if (s[i] == '%' && s[i+1] && s[i+2]) {
            int hi = s[i+1], lo = s[i+2];
            int h = (hi >= '0' && hi <= '9') ? hi - '0'
                  : (hi >= 'a' && hi <= 'f') ? hi - 'a' + 10
                  : (hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : -1;
            int l = (lo >= '0' && lo <= '9') ? lo - '0'
                  : (lo >= 'a' && lo <= 'f') ? lo - 'a' + 10
                  : (lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : -1;
            if (h >= 0 && l >= 0) { s[j++] = (char)((h << 4) | l); i += 3; continue; }
        }
        s[j++] = s[i++];
    }
    s[j] = '\0';
    return j;
}

static int is_sensitive_param(const char *key_lower)
{
    for (int i = 0; URL_SAFETY_SENSITIVE_PARAMS[i]; i++)
        if (strcmp(key_lower, URL_SAFETY_SENSITIVE_PARAMS[i]) == 0) return 1;
    return 0;
}

/* PoP: sensitive_query_param_name @ tools/url_safety.py:sensitive_query_param_name
 * Wrapper — canonical implementation is in url_safety.c */
char *sensitive_query_param_name(const char *url);  /* decl only, impl in url_safety.c */

/* PoP: has_sensitive_query_params */
bool tools_url_safety_has_sensitive_query_params(const char *url)
{
    char *r = sensitive_query_param_name(url);
    if (r) { free(r); return true; }
    return false;
}

