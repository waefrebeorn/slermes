/* Slermes C11 port of gateway/platforms/_http_client_limits.py
 *
 * Shared HTTP client limits for long-lived platform adapters. Returns the
 * tuned keep-alive config (expiry + max keep-alive connections), overridable
 * via HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY / HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE.
 * Negative/zero/garbage env values fall back to the defaults (fail-soft).
 *
 * PoP: exact port. Semantic source of truth = gateway/platforms/_http_client_limits.py.
 */
#ifndef SLERMES_HTTP_CLIENT_LIMITS_H
#define SLERMES_HTTP_CLIENT_LIMITS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_CLIENT_LIMITS_DEFAULT_KEEPALIVE_EXPIRY_S 2.0
#define HTTP_CLIENT_LIMITS_DEFAULT_MAX_KEEPALIVE      10

/* The resolved httpx.Limits-shaped config. httpx absent -> httpx_available=false
 * (mirrors the Python helper returning None when httpx isn't importable). */
typedef struct {
    bool  httpx_available;
    double keepalive_expiry;   /* seconds */
    int    max_keepalive;      /* max keep-alive connections */
} http_client_limits_t;

/* Resolve the limits, reading the override env vars. Always returns a populated
 * struct (defaults applied); httpx_available reflects whether the (optional)
 * httpx dependency is present — in C we treat it as always available and let
 * the caller decide. */
http_client_limits_t platform_httpx_limits(void);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_HTTP_CLIENT_LIMITS_H */
