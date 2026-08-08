/* port_agent_retry_utils.c — Thin adapter for legacy agent_retry_utils_* callers.
 *
 * These symbols appear in the Python C-port mapping layer; this file
 * preserves that surface while the canonical implementations live in
 * src/agent/retry_utils.c.
 */

#include "hermes_retry_utils.h"
#include <stdlib.h>
#include <string.h>

/* Legacy wrapper -> canonical retry_utils_* namespace. */
bool agent_retry_utils_is_zai_coding_overload_error(const char *base_url,
                                                    const char *model,
                                                    const retry_utils_err_t *err)
{
    return retry_utils_is_zai_coding_overload_error(base_url, model, err);
}

char *agent_retry_utils__error_text(int status_code,
                                    const char *msg,
                                    const char *body,
                                    const char *response)
{
    retry_utils_err_t err;
    memset(&err, 0, sizeof(err));

    /* Rebuild a flat text from the three fragments, tracking length. */
    char buf[16384];
    size_t n = 0;
    const char *parts[] = { msg, body, response };
    for (int i = 0; i < 3 && parts[i]; i++) {
        if (!parts[i] || !parts[i][0]) continue;
        if (n) { buf[n++] = ' '; if (n >= sizeof(buf) - 1) break; }
        for (const char *p = parts[i]; *p && n + 1 < sizeof(buf); p++) buf[n++] = *p;
    }
    buf[n] = '\0';
    memcpy(err.text, buf, sizeof(err.text) - 1);
    err.text[sizeof(err.text) - 1] = '\0';

    return retry_utils_error_text(&err);
}

char *agent_retry_utils_adaptive_rate_limit_backoff(int attempt,
                                                     const char *base_url,
                                                     const char *model,
                                                     const retry_utils_err_t *err,
                                                     double default_wait,
                                                     int short_attempts)
{
    (void)default_wait;

    char *label = retry_utils_adaptive_rate_limit_backoff(attempt,
                                                           base_url,
                                                           model,
                                                           err,
                                                           default_wait,
                                                           short_attempts);
    if (!label) return NULL;

    /* Normalize "SHORT"/"W|LONG" -> caller-safe labels. */
    if (strcmp(label, "SHORT") == 0) {
        free(label);
        return strdup("SHORT");
    }
    return label;
}

double agent_retry_utils_parse_retry_after_seconds(const char *value, int *ok)
{
    return retry_utils_parse_retry_after_seconds(value, ok);
}
