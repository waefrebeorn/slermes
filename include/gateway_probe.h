/*
 * gateway_probe.h — Gateway reachability check
 *
 * Probes the Hermes gateway to check if it's reachable and responding.
 * Used by the desktop app to show connection status.
 *
 * PoP: gateway_probe @ electron/gateway-ws-probe.cjs
 */

#ifndef GATEWAY_PROBE_H
#define GATEWAY_PROBE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Configuration ─────────────────────────────────────────────────────── */
#define PROBE_MAX_URL   1024
#define PROBE_TIMEOUT_MS 5000
#define PROBE_MAX_RESPONSE 4096

/* ── Probe result ───────────────────────────────────────────────────────── */
typedef struct {
    bool    reachable;
    bool    authenticated;
    int     latency_ms;
    int     http_status;
    char    version[128];
    char    error[512];
} probe_result_t;

/* PoP: gateway_probe @ electron/gateway-ws-probe.cjs */
/* Probe a gateway URL to check if it's reachable.
 * url: http:// or https:// URL of the gateway.
 * timeout_ms: max time to wait for response.
 * Returns a probe_result_t (stack-allocated, no free needed). */
probe_result_t gateway_probe(const char *url, int timeout_ms);

/* Quick probe with default timeout. */
probe_result_t gateway_probe_default(const char *url);

/* Check if a URL looks like a valid gateway URL. */
bool gateway_url_valid(const char *url);

/* Extract the HTTP URL from a WebSocket URL.
 * e.g. "wss://host:18789/ws" -> "https://host:18789"
 * Returns allocated string (caller must free). */
char *gateway_ws_to_http_url(const char *ws_url);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_PROBE_H */
