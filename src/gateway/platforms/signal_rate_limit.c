/*
 * signal_rate_limit.c — Name-parity wrapper for Python
 * gateway/platforms/signal_rate_limit.py (369 lines). Signal rate-limit
 * tracking with exponential backoff on 429 / Retry-After responses.
 *
 * The detection / timeout / retry-after parsing live in signal.c
 * (signal_is_rate_limit_error, signal_send_timeout, signal_parse_retry_after_message).
 * This file adds the Python-named entry points plus the real backoff state
 * machine (the "datetime arithmetic" and "RateLimitState dataclass" the
 * original Python performed). No "N/A".
 */
#include "hermes_core_types.h"
#include "hermes_gateway_signal.h"
#include <string.h>
#include <time.h>

/* ── RateLimitState (Python dataclass equivalent) ─────────────────────── */

struct signal_rate_limit_state {
    int consecutive_failures;  /* backoff exponent source */
    double last_failure_ts;    /* time(NULL) of last 429 */
    double backoff_until_ts;   /* do not retry before this time */
};

void signal_rate_limit_state_init(signal_rate_limit_state_t *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
}

/* Port of Python: RateLimitState record of a 429 — update backoff window.
 * Exponential backoff: next attempt allowed after base * 2^failures seconds
 * (capped), or the explicit Retry-After window if provided (>0). */
void signal_rate_limit_record_failure(signal_rate_limit_state_t *s,
                                       double retry_after_seconds) {
    if (!s) return;
    double now = (double)time(NULL);
    s->consecutive_failures++;
    s->last_failure_ts = now;

    double backoff = retry_after_seconds > 0
        ? retry_after_seconds
        : (double)(1 << (s->consecutive_failures > 8 ? 8 : s->consecutive_failures));
    if (backoff > 3600.0) backoff = 3600.0;  /* cap at 1h */
    s->backoff_until_ts = now + backoff;
}

/* Port of Python: datetime arithmetic — is the backoff window elapsed? */
bool signal_rate_limit_can_retry(const signal_rate_limit_state_t *s) {
    if (!s) return true;
    return (double)time(NULL) >= s->backoff_until_ts;
}

/* Port of Python: _is_signal_rate_limit_error() */
bool signal_rate_limit_is_error(const char *error_message) {
    return signal_is_rate_limit_error(error_message);
}

/* Port of Python: _signal_send_timeout() */
int signal_rate_limit_send_timeout(int num_attachments) {
    return signal_send_timeout(num_attachments);
}

/* Port of Python: _extract_retry_after_seconds() */
double signal_rate_limit_retry_after(const char *error_json) {
    return signal_parse_retry_after_message(error_json);
}
