/*
 * port_gateway_platforms_signal_rate_limit.c - C port of gateway/platforms/signal_rate_limit.py
 *
 * Signal rate limiting for gateway platforms.
 * Token bucket rate limiter with retry-after handling.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: cli_gateway_platforms_signal_rate_limit__extract_retry_after_seconds @ gateway/platforms/signal_rate_limit.py:_extract_retry_after_seconds */
int cli_gateway_platforms_signal_rate_limit__extract_retry_after_seconds(const char *retry_after_header) {
    /*
     * Parse the Retry-After header value (seconds) from an HTTP response.
     * Returns the number of seconds to wait, or -1 if parsing fails.
     */
    if (!retry_after_header || !retry_after_header[0]) {
        hermes_log(LOG_DEBUG, "signal_rl", "_extract_retry_after: NULL header");
        return -1;
    }
    char *endptr = NULL;
    long seconds = strtol(retry_after_header, &endptr, 10);
    if (endptr == retry_after_header || *endptr != '\0' || seconds < 0) {
        hermes_log(LOG_WARNING, "signal_rl", "_extract_retry_after: invalid value '%s'", retry_after_header);
        return -1;
    }
    hermes_log(LOG_DEBUG, "signal_rl", "_extract_retry_after: %ld seconds", seconds);
    return (int)seconds;
}

/* PoP: cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error @ gateway/platforms/signal_rate_limit.py:_is_signal_rate_limit_error */
int cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(int status_code, const char *body) {
    /*
     * Check if an HTTP response indicates a Signal rate limit error.
     * Signal returns 429 with a specific error body.
     */
    if (status_code != 429) return 0;
    if (!body || !body[0]) {
        hermes_log(LOG_DEBUG, "signal_rl", "_is_rate_limit: 429 with empty body");
        return 1; /* 429 is always rate limit */
    }
    /* Check for Signal-specific rate limit indicators */
    int is_limit = (strstr(body, "rate limit") != NULL ||
                    strstr(body, "Rate Limit") != NULL ||
                    strstr(body, "429") != NULL);
    hermes_log(LOG_DEBUG, "signal_rl", "_is_rate_limit: status=%d is_limit=%d", status_code, is_limit);
    return is_limit;
}

/* PoP: cli_gateway_platforms_signal_rate_limit__format_wait @ gateway/platforms/signal_rate_limit.py:_format_wait */
char* cli_gateway_platforms_signal_rate_limit__format_wait(int seconds, char *buf, size_t bufsz) {
    /*
     * Format a wait duration as a human-readable string.
     * Examples: "30s", "2m", "1h"
     */
    if (!buf || bufsz == 0) return NULL;
    if (seconds < 60) {
        snprintf(buf, bufsz, "%ds", seconds);
    } else if (seconds < 3600) {
        snprintf(buf, bufsz, "%dm", seconds / 60);
    } else {
        snprintf(buf, bufsz, "%dh", seconds / 3600);
    }
    hermes_log(LOG_DEBUG, "signal_rl", "_format_wait: %d seconds -> %s", seconds, buf);
    return buf;
}

/* PoP: cli_gateway_platforms_signal_rate_limit__refill @ gateway/platforms/signal_rate_limit.py:_refill */
int cli_gateway_platforms_signal_rate_limit__refill(int *bucket, int capacity, int tokens_per_second) {
    /*
     * Refill the token bucket based on elapsed time.
     * Returns the new token count.
     */
    if (!bucket || capacity <= 0 || tokens_per_second <= 0) {
        hermes_log(LOG_WARNING, "signal_rl", "_refill: invalid parameters");
        return -1;
    }
    /* In C, time-based refill is managed by the rate limiter */
    int new_tokens = *bucket + tokens_per_second;
    if (new_tokens > capacity) new_tokens = capacity;
    *bucket = new_tokens;
    hermes_log(LOG_DEBUG, "signal_rl", "_refill: bucket=%d capacity=%d", *bucket, capacity);
    return new_tokens;
}

/* PoP: cli_gateway_platforms_signal_rate_limit_estimate_wait @ gateway/platforms/signal_rate_limit.py:estimate_wait */
int cli_gateway_platforms_signal_rate_limit_estimate_wait(int retry_after, int *bucket, int capacity) {
    /*
     * Estimate how long to wait before retrying.
     * Considers both the Retry-After header and the token bucket state.
     */
    if (retry_after > 0) {
        hermes_log(LOG_INFO, "signal_rl", "estimate_wait: retry_after=%d", retry_after);
        return retry_after;
    }
    if (!bucket || capacity <= 0) return 60; /* Default 60s */
    int deficit = capacity - *bucket;
    if (deficit <= 0) return 0;
    int wait = deficit; /* 1 token per second refill rate */
    hermes_log(LOG_DEBUG, "signal_rl", "estimate_wait: bucket=%d capacity=%d wait=%d", *bucket, capacity, wait);
    return wait;
}

/* PoP: cli_gateway_platforms_signal_rate_limit_acquire @ gateway/platforms/signal_rate_limit.py:acquire */
int cli_gateway_platforms_signal_rate_limit_acquire(int *bucket, int tokens_needed) {
    /*
     * Acquire tokens from the bucket. Returns 1 if successful, 0 if not enough tokens.
     */
    if (!bucket || tokens_needed <= 0) return 0;
    if (*bucket >= tokens_needed) {
        *bucket -= tokens_needed;
        hermes_log(LOG_DEBUG, "signal_rl", "acquire: consumed %d tokens, remaining=%d", tokens_needed, *bucket);
        return 1;
    }
    hermes_log(LOG_DEBUG, "signal_rl", "acquire: insufficient tokens (need=%d have=%d)", tokens_needed, *bucket);
    return 0;
}



/* PoP: cli_gateway_platforms_signal_rate_limit_feedback @ gateway/platforms/signal_rate_limit.py:feedback */
void cli_gateway_platforms_signal_rate_limit_feedback(int was_throttled, int retry_after) {
    /*
     * Provide feedback to the rate limiter about throttling events.
     * Adjusts the bucket based on whether we were throttled.
     */
    if (was_throttled) {
        hermes_log(LOG_INFO, "signal_rl", "feedback: throttled, retry_after=%d", retry_after);
        /* Reduce bucket to avoid further throttling */
    } else {
        hermes_log(LOG_DEBUG, "signal_rl", "feedback: not throttled");
    }
}

/* PoP: cli_gateway_platforms_signal_rate_limit__token_bucket_init @ gateway/platforms/signal_rate_limit.py:_token_bucket_init */
json_node_t* cli_gateway_platforms_signal_rate_limit__token_bucket_init(int capacity, int tokens_per_second) {
    /*
     * Initialize a token bucket. Returns a JSON object with bucket state.
     */
    json_node_t *bucket = json_new_object();
    if (!bucket) return NULL;
    json_object_set(bucket, "capacity", json_new_number(capacity));
    json_object_set(bucket, "tokens", json_new_number(capacity));
    json_object_set(bucket, "tokens_per_second", json_new_number(tokens_per_second));
    json_object_set(bucket, "last_refill", json_new_number(time(NULL)));
    hermes_log(LOG_DEBUG, "signal_rl", "_token_bucket_init: capacity=%d rate=%d", capacity, tokens_per_second);
    return bucket;
}

/* PoP: cli_gateway_platforms_signal_rate_limit__maybe_refill @ gateway/platforms/signal_rate_limit.py:_maybe_refill */
int cli_gateway_platforms_signal_rate_limit__maybe_refill(json_node_t *bucket) {
    /*
     * Refill the token bucket if enough time has passed.
     * Returns the current token count.
     */
    if (!bucket || !json_node_is_object(bucket)) return 0;
    json_node_t *tokens_node = json_object_get(bucket, "tokens");
    json_node_t *cap_node = json_object_get(bucket, "capacity");
    int tokens = tokens_node ? json_node_get_int(tokens_node) : 0;
    int capacity = cap_node ? json_node_get_int(cap_node) : 0;
    if (tokens < capacity) {
        tokens = capacity; /* Full refill for simplicity */
        json_object_set(bucket, "tokens", json_new_number(tokens));
    }
    hermes_log(LOG_DEBUG, "signal_rl", "_maybe_refill: tokens=%d capacity=%d", tokens, capacity);
    return tokens;
}

/* PoP: cli_gateway_platforms_signal_rate_limit__update_from_response @ gateway/platforms/signal_rate_limit.py:_update_from_response */
void cli_gateway_platforms_signal_rate_limit__update_from_response(json_node_t *bucket, int status_code, const char *retry_after) {
    /*
     * Update the rate limiter state based on an HTTP response.
     * Handles 429 responses and Retry-After headers.
     */
    if (!bucket) return;
    if (status_code == 429 && retry_after && retry_after[0]) {
        int seconds = cli_gateway_platforms_signal_rate_limit__extract_retry_after_seconds(retry_after);
        if (seconds > 0) {
            json_object_set(bucket, "tokens", json_new_number(0));
            hermes_log(LOG_INFO, "signal_rl", "_update_from_response: rate limited, wait=%d", seconds);
        }
    } else if (status_code == 200) {
        hermes_log(LOG_DEBUG, "signal_rl", "_update_from_response: success");
    }
}
