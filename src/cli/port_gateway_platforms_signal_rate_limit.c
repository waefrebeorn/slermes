/*
 * port_gateway_platforms_signal_rate_limit.c - C port of gateway/platforms/signal_rate_limit.py
 *
 * Signal rate limiting for gateway platforms.
 * Token bucket rate limiter with retry-after handling.
 */

#include "hermes_core_types.h"
#include "hermes_logger.h"
#include "hermes_json.h"
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
int cli_gateway_platforms_signal_rate_limit__is_signal_rate_limit_error(const char *err) {
    /* Faithful port of the Python _is_signal_rate_limit_error(err):
     *   - typed RATELIMIT_ERROR code (signal-cli >= v0.14.3)
     *   - legacy "[429]" / "RateLimitException" substrings
     *   - libsignal-net "RetryLaterException" / "Retry after N seconds"
     *     surfaced inside AttachmentInvalidException (never re-tagged as
     *     RateLimitException), so substring is the only signal.
     * `err` is either a JSON object {"code":..,"message":..} or a plain
     * string. */
    if (!err || !err[0]) return 0;

    const char *message = err;
    long code = -1;
    char *parse_err = NULL;
    json_t *doc = json_parse(err, &parse_err);
    if (parse_err) free(parse_err);
    if (doc) {
        json_t *c = json_obj_get(doc, "code");
        if (c && c->type == JSON_NUMBER) code = (long)c->num_val;
        const char *m = json_get_str(doc, "message", NULL);
        if (m) message = m;
    }

    if (code == -5) {            /* SIGNAL_RPC_ERROR_RATELIMIT = -5 (signal-cli v0.14.3+) */
        if (doc) json_free(doc);
        return 1;
    }

    /* Case-insensitive substring match (Python str(err).lower()). */
    char low[2048];
    size_t i, n = 0;
    for (i = 0; err[i] && n < sizeof(low) - 1; i++) {
        char ch = err[i];
        if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        low[n++] = ch;
    }
    low[n] = '\0';

    int hit = (strstr(low, "[429]") != NULL ||
               strstr(low, "ratelimit") != NULL ||
               strstr(low, "retrylaterexception") != NULL ||
               strstr(low, "retry after") != NULL);
    if (doc) json_free(doc);
    return hit;
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

/* PoP: cli_gateway_platforms_signal_rate_limit_report_rpc_duration @ gateway/platforms/signal_rate_limit.py:report_rpc_duration */
/* Record an attachment-send RPC that just completed: deduct n_attachments
 * tokens WITHOUT crediting upload-time refill (Signal checks the bucket at RPC
 * start and does not refill during processing — crediting it causes drift →
 * 429s), and advance last_refill to now so the next acquire/_refill counts
 * from this point. n_attachments <= 0 is a no-op (faithful to Python). */
void cli_gateway_platforms_signal_rate_limit_report_rpc_duration(
    json_node_t *bucket, double rpc_duration, int n_attachments)
{
    if (!bucket || !json_node_is_object(bucket)) return;
    if (n_attachments <= 0) return;

    double token_before = json_object_get_number(bucket, "tokens", 0.0);
    double token_after = token_before - (double)n_attachments;
    if (token_after < 0.0) token_after = 0.0;
    json_object_set(bucket, "tokens", json_new_number(token_after));
    json_object_set(bucket, "last_refill", json_new_number((double)time(NULL)));

    double refill_rate = json_object_get_number(bucket, "tokens_per_second", 0.0);
    int level = (rpc_duration > 10.0 && n_attachments > 5) ? LOG_INFO : LOG_DEBUG;
    hermes_log(level, "signal_rl",
        "report_rpc_duration: RPC for %d att took %.1fs — tokens %.1f -> %.1f (deducted=%d, no upload refill credited, refill=%.4f/s)",
        n_attachments, rpc_duration, token_before, token_after, n_attachments, refill_rate);
}
