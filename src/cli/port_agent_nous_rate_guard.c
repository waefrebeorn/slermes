/*
 * port_agent_nous_rate_guard.c — C port of agent/nous_rate_guard.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_nous_rate_guard__has_exhausted_bucket_in_object @ agent/nous_rate_guard.py:_has_exhausted_bucket_in_object */

#define MIN_RESET_FOR_BREAKER_SECONDS 60.0f

/*
 * Rate limit bucket: mirrors the Python dataclass attributes.
 */
typedef struct {
    int   limit;
    int   remaining;
    float remaining_seconds_now;
    float reset_seconds;
} rate_limit_bucket_t;

/*
 * Rate limit state: contains the four bucket types.
 */
typedef struct {
    rate_limit_bucket_t requests_min;
    rate_limit_bucket_t requests_hour;
    rate_limit_bucket_t tokens_min;
    rate_limit_bucket_t tokens_hour;
} rate_limit_state_t;

/*
 * _has_exhausted_bucket_in_object: Check if any rate limit bucket is exhausted.
 *
 * p1 = pointer to rate_limit_state_t
 *
 * Returns: (void*)1 if any bucket is exhausted, (void*)0 otherwise.
 */
void* cli_agent_nous_rate_guard__has_exhausted_bucket_in_object(
    void* p1, void* p2, void* p3, void* p4, void* p5)
{
    (void)p2; (void)p3; (void)p4; (void)p5;

    rate_limit_state_t *state = (rate_limit_state_t *)p1;
    if (!state) return (void *)0;

    /* Check each bucket: requests_min, requests_hour, tokens_min, tokens_hour */
    rate_limit_bucket_t *buckets[4] = {
        &state->requests_min,
        &state->requests_hour,
        &state->tokens_min,
        &state->tokens_hour
    };

    const char *bucket_names[4] = {
        "requests_min", "requests_hour", "tokens_min", "tokens_hour"
    };

    for (int i = 0; i < 4; i++) {
        rate_limit_bucket_t *b = buckets[i];

        if (b->limit <= 0) continue;
        if (b->remaining > 0) continue;

        float reset = b->remaining_seconds_now;
        if (reset <= 0.0f) reset = b->reset_seconds;

        if (reset >= MIN_RESET_FOR_BREAKER_SECONDS) {
            hermes_log(LOG_DEBUG, "port",
                       "nous_rate_guard: bucket '%s' exhausted (limit=%d, remaining=%d, reset=%.1f)",
                       bucket_names[i], b->limit, b->remaining, reset);
            return (void *)1;
        }
    }

    return (void *)0;
}
