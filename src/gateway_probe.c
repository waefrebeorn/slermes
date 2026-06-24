/*
 * gateway_probe.c — Gateway reachability check
 *
 * Probes the Hermes gateway to check if it's reachable and responding.
 * Uses HTTP GET to the gateway's /health or /status endpoint.
 *
 * PoP: gateway_probe @ electron/gateway-ws-probe.cjs
 */

#include "gateway_probe.h"
#include "hermes.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ── Internal helpers ────────────────────────────────────────────────────── */

static int64_t now_ms(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

static void probe_result_init(probe_result_t *r) {
    r->reachable     = false;
    r->authenticated = false;
    r->latency_ms    = -1;
    r->http_status   = 0;
    r->version[0]    = '\0';
    r->error[0]      = '\0';
}

/* ── URL conversion ─────────────────────────────────────────────────────── */

/* PoP: gateway_probe @ electron/gateway-ws-probe.cjs */
char *gateway_ws_to_http_url(const char *ws_url) {
    if (!ws_url) return NULL;

    size_t len = strlen(ws_url);
    char *http_url = malloc(len + 8);
    if (!http_url) return NULL;

    if (strncmp(ws_url, "wss://", 6) == 0) {
        snprintf(http_url, len + 8, "https://%s", ws_url + 6);
    } else if (strncmp(ws_url, "ws://", 5) == 0) {
        snprintf(http_url, len + 8, "http://%s", ws_url + 5);
    } else {
        strncpy(http_url, ws_url, len + 7);
        http_url[len] = '\0';
    }

    /* Strip trailing /ws or /ws/ */
    size_t hlen = strlen(http_url);
    if (hlen > 4 && strcmp(http_url + hlen - 3, "/ws") == 0) {
        http_url[hlen - 3] = '\0';
    } else if (hlen > 5 && strcmp(http_url + hlen - 4, "/ws/") == 0) {
        http_url[hlen - 4] = '\0';
    }

    return http_url;
}

bool gateway_url_valid(const char *url) {
    if (!url || !*url) return false;
    return (strncmp(url, "ws://", 5) == 0 ||
            strncmp(url, "wss://", 6) == 0 ||
            strncmp(url, "http://", 7) == 0 ||
            strncmp(url, "https://", 8) == 0);
}

/* ── Probe implementation ───────────────────────────────────────────────── */

/* PoP: gateway_probe @ electron/gateway-ws-probe.cjs */
probe_result_t gateway_probe(const char *url, int timeout_ms) {
    probe_result_t result;
    probe_result_init(&result);

    if (!url || !*url) {
        strncpy(result.error, "Empty URL", sizeof(result.error) - 1);
        return result;
    }

    /* Convert WS URL to HTTP for probing */
    char *http_url = NULL;
    if (strncmp(url, "ws://", 5) == 0 || strncmp(url, "wss://", 6) == 0) {
        http_url = gateway_ws_to_http_url(url);
    } else {
        http_url = strdup(url);
    }

    if (!http_url) {
        strncpy(result.error, "URL conversion failed", sizeof(result.error) - 1);
        return result;
    }

    /* Build health check URL */
    size_t health_url_len = strlen(http_url) + 32;
    char *health_url = malloc(health_url_len);
    if (!health_url) {
        free(http_url);
        strncpy(result.error, "malloc failed", sizeof(result.error) - 1);
        return result;
    }

    /* Try /health first, then /status, then root */
    const char *paths[] = { "/health", "/status", "/", NULL };
    int64_t start_ms = now_ms();

    for (int i = 0; paths[i]; i++) {
        snprintf(health_url, health_url_len, "%s%s", http_url, paths[i]);

        /* Use the http library to make a GET request */
        http_t *http = http_new(timeout_ms / 2);
        if (!http) continue;
        http_resp_t *resp = http_get(http, health_url, NULL);

        if (resp && resp->status > 0) {
            result.reachable   = true;
            result.http_status = resp->status;
            result.latency_ms  = (int)(now_ms() - start_ms);

            if (resp->status == 200) {
                result.authenticated = true;
                /* Try to extract version from response body */
                const char *ver = strstr(resp->body, "\"version\"");
                if (ver) {
                    const char *colon = strchr(ver + 9, ':');
                    if (colon) {
                        const char *start = strchr(colon, '"');
                        if (start) {
                            start++;
                            const char *end = strchr(start, '"');
                            if (end) {
                                size_t vlen = (size_t)(end - start);
                                if (vlen < sizeof(result.version)) {
                                    strncpy(result.version, start, vlen);
                                    result.version[vlen] = '\0';
                                }
                            }
                        }
                    }
                }
            } else if (resp->status == 401 || resp->status == 403) {
                result.authenticated = false;
                strncpy(result.error, "Authentication required", sizeof(result.error) - 1);
            }

            http_resp_free(resp);
            http_free(http);
            break;
        }

        if (resp) http_resp_free(resp);
        http_free(http);
    }

    free(health_url);
    free(http_url);

    if (!result.reachable) {
        result.latency_ms = (int)(now_ms() - start_ms);
        snprintf(result.error, sizeof(result.error),
                 "Gateway unreachable (%d ms timeout)", timeout_ms);
    }

    return result;
}

probe_result_t gateway_probe_default(const char *url) {
    return gateway_probe(url, PROBE_TIMEOUT_MS);
}
