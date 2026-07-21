/* Slermes C11 port of gateway/platforms/_http_client_limits.py — implementation.
 * PoP: exact port. Semantic source of truth = gateway/platforms/_http_client_limits.py. */
#include "http_client_limits.h"
#include "hermes_gateway_webhook.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef HTTP_CLIENT_LIMITS_DEFAULT_KEEPALIVE_EXPIRY_S
#define HTTP_CLIENT_LIMITS_DEFAULT_KEEPALIVE_EXPIRY_S 2.0
#endif
#ifndef HTTP_CLIENT_LIMITS_DEFAULT_MAX_KEEPALIVE
#define HTTP_CLIENT_LIMITS_DEFAULT_MAX_KEEPALIVE 10
#endif

/* Parse a positive double from an env var; fall back to default on missing /
 * empty / non-numeric / non-positive values (mirrors Python's fail-soft). */
static double http_env_float(const char *name, double dflt) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return dflt;
    /* strip leading/trailing whitespace */
    while (*raw && (*raw == ' ' || *raw == '\t')) raw++;
    if (!*raw) return dflt;
    errno = 0;
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end == raw || errno != 0) return dflt;     /* no digits parsed */
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return dflt;                 /* trailing garbage */
    return val > 0.0 ? val : dflt;
}

/* Parse a positive int from an env var; fall back to default on bad input. */
static int http_env_int(const char *name, int dflt) {
    const char *raw = getenv(name);
    if (!raw || !*raw) return dflt;
    while (*raw && (*raw == ' ' || *raw == '\t')) raw++;
    if (!*raw) return dflt;
    errno = 0;
    char *end = NULL;
    long val = strtol(raw, &end, 10);
    if (end == raw || errno != 0) return dflt;
    while (*end == ' ' || *end == '\t') end++;
    if (*end != '\0') return dflt;
    return (val > 0 && val <= 2147483647L) ? (int)val : dflt;
}

/* PoP: platform_httpx_limits @ gateway/platforms/_http_client_limits.py:platform_httpx_limits */
http_client_limits_t platform_httpx_limits(void) {
    http_client_limits_t out;
    out.httpx_available = true;  /* C runtime always has the equivalent; see header */
    out.keepalive_expiry = http_env_float("HERMES_GATEWAY_HTTPX_KEEPALIVE_EXPIRY",
                                         HTTP_CLIENT_LIMITS_DEFAULT_KEEPALIVE_EXPIRY_S);
    out.max_keepalive = http_env_int("HERMES_GATEWAY_HTTPX_MAX_KEEPALIVE",
                                     HTTP_CLIENT_LIMITS_DEFAULT_MAX_KEEPALIVE);
    return out;
}
