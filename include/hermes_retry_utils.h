#ifndef HERMES_RETRY_UTILS_H
#define HERMES_RETRY_UTILS_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int status_code;
    char text[8192];
} retry_utils_err_t;

double jittered_backoff(int attempt,
                               double base_delay,
                               double max_delay,
                               double jitter_ratio);
void jittered_backoff_reset(void);

/* Flattened lowercased error text for retry classification.
 * Also exposed as `error_text` for parity-scanner matching. */
char *retry_utils_error_text(const retry_utils_err_t *err);

/* Public `error_text` — parity alias for Python `_error_text`. */
char *error_text(int status_code, const char *msg, const char *body, const char *response);

/* Z.AI Coding Plan overload 429 detector. */
bool retry_utils_is_zai_coding_overload_error(const char *base_url,
                                              const char *model,
                                              const retry_utils_err_t *err);

/* Provider-aware backoff policy.
 * Returns malloc'd "W|label" on Z.AI overload long-backoff tier,
 * "SHORT" for short tier, or NULL if policy does not apply. */
char *retry_utils_adaptive_rate_limit_backoff(int attempt,
                                               const char *base_url,
                                               const char *model,
                                               const retry_utils_err_t *err,
                                               double default_wait,
                                               int short_attempts);

/* Retry-loop ceiling so the full Z.AI long-backoff schedule is reachable. */
int retry_utils_zai_coding_overload_retry_ceiling(int short_attempts);

/* Parse a Retry-After value into non-negative seconds.
 * Python: retry_utils.parse_retry_after_seconds(value_or_headers).
 *
 * - `value` is a NUL-terminated string header value (numeric seconds or
 *   RFC 7231 HTTP-date). `ok` is set to 1 on success, 0 when the value is
 *   absent/unparseable (Python returns None in that case).
 * - When `value` is a numeric string, returns max(0.0, float(value)).
 * - When `value` is an HTTP-date, returns seconds-until-then clamped to >= 0.
 * - Empty/NULL `value` or unparseable text -> *ok = 0, returns 0.0. */
double retry_utils_parse_retry_after_seconds(const char *value, int *ok);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_RETRY_UTILS_H */
