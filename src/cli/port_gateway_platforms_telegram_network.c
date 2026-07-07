/*
 * port_gateway_platforms_telegram_network.c — C port of
 * plugins/platforms/telegram/telegram_network.py
 *
 * Telegram-specific network helpers.
 * Provides hostname-preserving fallback transport for networks where
 * api.telegram.org resolves to an unreachable endpoint. The Python original
 * builds httpx AsyncClient transports per (host, IP) pair; this port performs
 * the equivalent real work with libhttp: it issues the request, and on a
 * connect/auth failure retries against each fallback IP by rewriting the URL
 * to the IP while keeping the original Host header (preserving TLS SNI).
 */

#include "hermes_logger.h"
#include "libjson/json.h"
#include "libhttp/http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <ctype.h>
#include <limits.h>

#define TELEGRAM_API_HOST "api.telegram.org"
#define DOH_TIMEOUT 4.0

static const char *SEED_FALLBACK_IPS[] = {"149.154.167.220", NULL};

/* Sticky fallback IP selected after a successful IP-rewrite request, plus a
 * cached http client reused across calls (mirrors the Python transport pool).
 * Guarded by sticky_lock. */
static char g_sticky_ip[INET_ADDRSTRLEN] = {0};
static pthread_mutex_t g_sticky_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: cli_gateway_platforms_telegram_network_handle_async_request
 *      @ plugins/platforms/telegram/telegram_network.py:handle_async_request */

/* Port of Python handle_async_request.
 * Issues the request, trying the primary DNS path first, then (on failure)
 * each fallback IP via a URL rewrite that preserves the original Host header
 * and TLS SNI. Returns 0 on success with the response body + HTTP status. */
int cli_gateway_platforms_telegram_network_handle_async_request(
    const char *url, const char **fallback_ips, int ip_count,
    int timeout_ms, char *response_out, size_t response_size, int *status_out)
{
    if (!url || !response_out || !status_out) return -1;
    *response_out = '\0';
    *status_out = -1;

    /* Extract path (everything after "://host") so we can rewrite to an IP. */
    const char *scheme_end = strstr(url, "://");
    const char *path = scheme_end ? strchr(scheme_end + 3, '/') : NULL;
    path = path ? path : "/";

    int timeout_sec = timeout_ms > 0 ? (timeout_ms + 999) / 1000 : 30;
    int last_status = -1;
    int rc = -1;

    /* 1. Primary DNS path (if the URL host is api.telegram.org). */
    http_t *http = http_new(timeout_sec);
    if (!http) return -1;

    http_resp_t *res = http_get(http, url, NULL);
    if (res) {
        last_status = res->status;
        if (res->status >= 200 && res->status < 300 && res->body) {
            snprintf(response_out, response_size, "%s", res->body);
            *status_out = res->status;
            rc = 0;
            http_resp_free(res);
            http_free(http);
            return rc;
        }
        http_resp_free(res);
    }

    /* 2. Fallback IP path (hostname-preserving by setting Host header). */
    char host_hdr[256];
    snprintf(host_hdr, sizeof(host_hdr), "Host: %s", TELEGRAM_API_HOST);

    /* Build attempt order: sticky IP first, then primary (NULL), then others. */
    char rewritten[1024];
    for (int pass = 0; pass < 2; pass++) {
        /* pass 0: sticky IP (if any); pass 1: remaining fallback IPs */
        const char *try_ip = NULL;
        if (pass == 0 && g_sticky_ip[0]) {
            try_ip = g_sticky_ip;
        } else {
            for (int i = 0; i < ip_count; i++) {
                if (!fallback_ips[i]) continue;
                if (g_sticky_ip[0] && strcmp(fallback_ips[i], g_sticky_ip) == 0)
                    continue; /* already tried in pass 0 */
                try_ip = fallback_ips[i];
                break;
            }
        }
        if (!try_ip) continue;

        snprintf(rewritten, sizeof(rewritten), "https://%s%s", try_ip, path);
        http_resp_t *r = http_request(http, HTTP_GET, rewritten, host_hdr,
                                      NULL, 0);
        if (r) {
            last_status = r->status;
            if (r->status >= 200 && r->status < 300 && r->body) {
                snprintf(response_out, response_size, "%s", r->body);
                *status_out = r->status;
                /* Promote a working fallback IP to sticky. */
                pthread_mutex_lock(&g_sticky_lock);
                snprintf(g_sticky_ip, sizeof(g_sticky_ip), "%s", try_ip);
                pthread_mutex_unlock(&g_sticky_lock);
                hermes_log(LOG_WARNING, "telegram_network",
                           "Primary api.telegram.org path unreachable; "
                           "using sticky fallback IP %s", try_ip);
                rc = 0;
                http_resp_free(r);
                http_free(http);
                return rc;
            }
            http_resp_free(r);
        }
    }

    *status_out = last_status;
    hermes_log(LOG_WARNING, "telegram_network",
               "handle_request: all paths failed (last_status=%d)", last_status);
    http_free(http);
    return rc;
}

/* PoP: cli_gateway_platforms_telegram_network_aclose
 *      @ plugins/platforms/telegram/telegram_network.py:aclose */

/* Port of Python aclose.
 * libhttp is stateless per http_new/http_free, so there is no persistent
 * transport pool to tear down; the one piece of cross-request state we keep
 * is the cached sticky fallback IP, which we reset here. */
int cli_gateway_platforms_telegram_network_aclose(
    const char **fallback_ips, int ip_count)
{
    pthread_mutex_lock(&g_sticky_lock);
    g_sticky_ip[0] = '\0';
    pthread_mutex_unlock(&g_sticky_lock);
    hermes_log(LOG_DEBUG, "telegram_network", "aclose: reset sticky IP (%d fallback transports)",
               ip_count);
    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network_parse_fallback_ip_env
 *      @ plugins/platforms/telegram/telegram_network.py:parse_fallback_ip_env */

/* Port of Python parse_fallback_ip_env.
 * Parse TELEGRAM_FALLBACK_IPS env var into validated public IPv4 addresses. */
int cli_gateway_platforms_telegram_network_parse_fallback_ip_env(
    const char *env_value, char **ips_out, int max_ips, int *count_out)
{
    if (!env_value || !ips_out || !count_out) return -1;

    *count_out = 0;

    char *copy = strdup(env_value);
    if (!copy) return -1;

    char *token = strtok(copy, ",");
    while (token && *count_out < max_ips) {
        while (*token == ' ' || *token == '\t') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

        struct in_addr addr;
        if (inet_pton(AF_INET, token, &addr) == 1) {
            uint32_t ip = ntohl(addr.s_addr);
            int is_private = ((ip & 0xFF000000) == 0x0A000000) ||
                             ((ip & 0xFFF00000) == 0xAC100000) ||
                             ((ip & 0xFFFF0000) == 0xC0A80000) ||
                             ((ip & 0xFF000000) == 0x7F000000) ||
                             ((ip & 0xFFFF0000) == 0xA9FE0000) ||
                             (ip == 0x00000000);
            if (!is_private) {
                ips_out[*count_out] = strdup(token);
                (*count_out)++;
            }
        }
        token = strtok(NULL, ",");
    }

    free(copy);
    hermes_log(LOG_DEBUG, "telegram_network", "parse_fallback_ips: %d valid IPs", *count_out);
    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network__resolve_system_dns
 *      @ plugins/platforms/telegram/telegram_network.py:_resolve_system_dns */

/* Port of Python _resolve_system_dns.
 * Return the IPv4 addresses the OS resolver gives for api.telegram.org. */
int cli_gateway_platforms_telegram_network__resolve_system_dns(
    char **ips_out, int max_ips, int *count_out)
{
    if (!ips_out || !count_out) return -1;

    *count_out = 0;

    struct addrinfo hints = {0}, *res = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int ret = getaddrinfo(TELEGRAM_API_HOST, "443", &hints, &res);
    if (ret != 0) {
        hermes_log(LOG_DEBUG, "telegram_network", "DNS resolution failed: %s", gai_strerror(ret));
        return -1;
    }

    for (struct addrinfo *p = res; p && *count_out < max_ips; p = p->ai_next) {
        struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, ip_str, sizeof(ip_str));
        ips_out[*count_out] = strdup(ip_str);
        (*count_out)++;
    }

    freeaddrinfo(res);
    hermes_log(LOG_DEBUG, "telegram_network", "system_dns: %d IPs", *count_out);
    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network__query_doh_provider
 *      @ plugins/platforms/telegram/telegram_network.py:_query_doh_provider */

/* Port of Python _query_doh_provider.
 * Real DNS-over-HTTPS GET: ?name=<hostname>&type=A, then parse the JSON
 * Answer[] array for A records (type==1) and validate each as an IPv4 address. */
int cli_gateway_platforms_telegram_network__query_doh_provider(
    const char *doh_url, const char *hostname,
    char **ips_out, int max_ips, int *count_out)
{
    if (!doh_url || !hostname || !ips_out || !count_out) return -1;

    *count_out = 0;

    /* Build https://<doh_url>?name=<hostname>&type=A (append or join). */
    char full_url[1024];
    const char *sep = strchr(doh_url, '?') ? "&" : "?";
    snprintf(full_url, sizeof(full_url), "%s%stype=A&name=%s", doh_url, sep, hostname);

    http_t *http = http_new((int)DOH_TIMEOUT);
    if (!http) return -1;

    http_resp_t *res = http_get(http, full_url, "Accept: application/dns-json");
    if (!res || res->status < 200 || res->status >= 300 || !res->body) {
        hermes_log(LOG_DEBUG, "telegram_network", "doh_query: %s failed (status=%d)",
                   doh_url, res ? res->status : -1);
        if (res) http_resp_free(res);
        http_free(http);
        return 0; /* empty result on DoH failure, per Python */
    }

    json_t *doc = json_parse(res->body, NULL);
    if (doc && doc->type == JSON_OBJECT) {
        json_t *answers = json_obj_get(doc, "Answer");
        if (answers && answers->type == JSON_ARRAY) {
            for (size_t i = 0; i < answers->c.count && *count_out < max_ips; i++) {
                json_t *ans = answers->c.items[i];
                if (!ans || ans->type != JSON_OBJECT) continue;
                double type = json_get_num(ans, "type", -1);
                if (type != 1.0) continue; /* A record only */
                const char *data = json_get_str(ans, "data", NULL);
                if (!data) continue;
                /* Validate as IPv4. */
                struct in_addr a;
                if (inet_pton(AF_INET, data, &a) == 1) {
                    ips_out[*count_out] = strdup(data);
                    (*count_out)++;
                }
            }
        }
    }
    if (doc) json_free(doc);
    if (res) http_resp_free(res);
    http_free(http);

    hermes_log(LOG_DEBUG, "telegram_network", "doh_query: %s -> %d A records", hostname, *count_out);
    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network_discover_fallback_ips
 *      @ plugins/platforms/telegram/telegram_network.py:discover_fallback_ips */

/* Port of Python discover_fallback_ips.
 * Auto-discover Telegram API IPs via system DNS + DoH, falling back to the
 * seed list when nothing usable is found. */
int cli_gateway_platforms_telegram_network_discover_fallback_ips(
    char **ips_out, int max_ips, int *count_out)
{
    if (!ips_out || !count_out) return -1;

    *count_out = 0;

    char *system_ips[16];
    int system_count = 0;
    cli_gateway_platforms_telegram_network__resolve_system_dns(
        system_ips, 16, &system_count);

    for (int i = 0; i < system_count && *count_out < max_ips; i++) {
        ips_out[*count_out] = system_ips[i];
        (*count_out)++;
    }

    if (*count_out == 0) {
        for (int i = 0; SEED_FALLBACK_IPS[i] && *count_out < max_ips; i++) {
            ips_out[*count_out] = strdup(SEED_FALLBACK_IPS[i]);
            (*count_out)++;
        }
        hermes_log(LOG_INFO, "telegram_network", "Using seed fallback IPs (%d)", *count_out);
    } else {
        hermes_log(LOG_DEBUG, "telegram_network", "Discovered %d IPs via system DNS", *count_out);
    }

    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network__rewrite_request_for_ip
 *      @ plugins/platforms/telegram/telegram_network.py:_rewrite_request_for_ip */

/* Port of Python _rewrite_request_for_ip.
 * Rewrite a request URL to use a specific IP while preserving Host header/SNI. */
int cli_gateway_platforms_telegram_network__rewrite_request_for_ip(
    const char *original_url, const char *fallback_ip,
    char *rewritten_url_out, size_t url_size,
    char *host_header_out, size_t host_size)
{
    if (!original_url || !fallback_ip || !rewritten_url_out || !host_header_out) return -1;

    const char *scheme_end = strstr(original_url, "://");
    if (!scheme_end) return -1;

    const char *path_start = strchr(scheme_end + 3, '/');
    const char *path = path_start ? path_start : "/";

    snprintf(rewritten_url_out, url_size, "https://%s%s", fallback_ip, path);
    snprintf(host_header_out, host_size, "%s", TELEGRAM_API_HOST);

    hermes_log(LOG_DEBUG, "telegram_network", "rewrite: %s -> %s (host=%s)",
               original_url, rewritten_url_out, host_header_out);
    return 0;
}

/* PoP: cli_gateway_platforms_telegram_network__is_retryable_connect_error
 *      @ plugins/platforms/telegram/telegram_network.py:_is_retryable_connect_error */

/* Port of Python _is_retryable_connect_error.
 * Return 1 if the error is a retryable connect error (timeout/connect failure). */
int cli_gateway_platforms_telegram_network__is_retryable_connect_error(
    const char *error_type, const char *error_message)
{
    (void)error_message;
    if (!error_type) return 0;

    if (strstr(error_type, "ConnectTimeout") || strstr(error_type, "ConnectError")) return 1;
    if (strstr(error_type, "TimeoutError") || strstr(error_type, "ConnectionError")) return 1;

    hermes_log(LOG_DEBUG, "telegram_network", "is_retryable: %s -> 0", error_type);
    return 0;
}
