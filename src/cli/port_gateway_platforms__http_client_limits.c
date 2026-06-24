/*
 * port_gateway_platforms__http_client_limits.c — C port of gateway/platforms/_http_client_limits.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_gateway_platforms__http_client_limits_platform_httpx_limits @ gateway/platforms/_http_client_limits.py:platform_httpx_limits */

/*
 * HTTP client limits configuration for persistent platform-adapter clients.
 *
 * Mirrors the Python httpx.Limits with tuned keepalive settings.
 */
typedef struct {
    int   max_keepalive_connections;
    float keepalive_expiry;
    int   max_connections;
} httpx_limits_t;

static const httpx_limits_t DEFAULT_LIMITS = {
    .max_keepalive_connections = 10,
    .keepalive_expiry = 2.0f,
    .max_connections = 100,
};

/*
 * _env_float: Read a float from environment variable with default.
 */
static float _env_float(const char *name, float default_val) {
    const char *raw = getenv(name);
    if (!raw || !raw[0]) return default_val;
    char *endptr;
    float val = strtof(raw, &endptr);
    if (endptr == raw || val <= 0.0f) return default_val;
    return val;
}

/*
 * _env_int: Read an int from environment variable with default.
 */
static int _env_int(const char *name, int default_val) {
    const char *raw = getenv(name);
    if (!raw || !raw[0]) return default_val;
    char *endptr;
    long val = strtol(raw, &endptr, 10);
    if (endptr == raw || val <= 0) return default_val;
    return (int)val;
}

/*
 * platform_httpx_limits: Return HTTP client limits tuned for platform adapters.
 *
 * Reads HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY and
 * HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE env vars for tuning.
 *
 * Returns: pointer to static httpx_limits_t (valid for process lifetime).
 */
void* cli_gateway_platforms__http_client_limits_platform_httpx_limits(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;

    static httpx_limits_t limits;
    static int initialized = 0;

    if (!initialized) {
        limits.max_keepalive_connections = _env_int(
            "HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE",
            DEFAULT_LIMITS.max_keepalive_connections);
        limits.keepalive_expiry = _env_float(
            "HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY",
            DEFAULT_LIMITS.keepalive_expiry);
        limits.max_connections = DEFAULT_LIMITS.max_connections;

        initialized = 1;

        hermes_log(LOG_DEBUG, "port",
                   "platform_httpx_limits: max_keepalive=%d, expiry=%.1f, max=%d",
                   limits.max_keepalive_connections,
                   limits.keepalive_expiry,
                   limits.max_connections);
    }

    return &limits;
}
