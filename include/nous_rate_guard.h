/*
 * nous_rate_guard.h — Cross-session rate limit guard for Nous Portal.
 * Port of Python agent/nous_rate_guard.py (325 lines).
 *
 * Writes rate limit state to a shared JSON file (~/.hermes/rate_limits/nous.json)
 * so all sessions can check whether Nous Portal is currently rate-limited
 * before making requests. Prevents retry amplification when RPH is tapped.
 */
#ifndef NOUS_RATE_GUARD_H
#define NOUS_RATE_GUARD_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- State file management --- */

/**
 * Record that Nous Portal is rate-limited.
 * Writes reset_at timestamp to the shared state file.
 * @param hermes_home  Path to HERMES_HOME (e.g. ~/.hermes).
 * @param reset_at     POSIX timestamp when rate limit resets (0 = use default cooldown).
 * @param default_cooldown_secs  Fallback cooldown when reset_at is 0 (default 300s).
 */
void nous_rate_guard_record(const char *hermes_home,
                             double reset_at,
                             double default_cooldown_secs);

/**
 * Check if Nous Portal is currently rate-limited.
 * @param hermes_home  Path to HERMES_HOME.
 * @return Seconds remaining until reset, or -1.0 if not rate-limited or on error.
 */
double nous_rate_guard_remaining(const char *hermes_home);

/**
 * Clear the rate limit state file (e.g., after a successful request).
 */
void nous_rate_guard_clear(const char *hermes_home);

/* --- Formatting --- */

/**
 * Format seconds remaining into human-readable duration.
 * e.g. "45s", "3m 12s", "1h 30m"
 * @param seconds  Seconds to format.
 * @param buf      Output buffer (minimum 32 bytes).
 * @param sz       Buffer size.
 * @return buf.
 */
const char *format_remaining(double seconds, char *buf, size_t sz);

/* --- Rate limit detection --- */

/**
 * Parse reset seconds from response headers.
 * Checks x-ratelimit-reset-requests-1h, x-ratelimit-reset-requests, retry-after.
 * @return Seconds from now, or -1.0 if no usable header found.
 */
double parse_reset_seconds(const char *response_headers);

/**
 * Parse rate limit buckets from x-ratelimit-* headers.
 * Returns a JSON object like {"requests": {"remaining": N, "reset": N}, ...}.
 * Caller must free with json_free().
 */
json_t *nous_parse_buckets(const char *response_headers);

/**
 * Check if any bucket has remaining == 0 with a meaningful reset window (>= 60s).
 */
bool has_exhausted_bucket(json_t *buckets);

/**
 * Decide whether a 429 response is a genuine rate limit vs transient throttling.
 * Checks response headers for exhausted buckets with >= 60s reset windows.
 */
bool nous_is_genuine_rate_limit(const char *response_headers);

#ifdef __cplusplus
}
#endif

#endif /* NOUS_RATE_GUARD_H */
