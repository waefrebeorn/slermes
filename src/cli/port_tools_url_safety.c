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

/* PoP: cli_tools_url_safety__global_allow_private_urls @ tools/url_safety.py:_global_allow_private_urls */

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

    /* Port of Python tools/url_safety.py:async_is_safe_url.
     * Python offloads DNS resolution off the event loop via asyncio.to_thread();
     * in C there is no event loop, so we call the synchronous resolver directly.
     * The behavior (SSRF block decision) is identical. */
    *result_out = cli_tools_url_safety_is_safe_url(url);
    return 0;
}
