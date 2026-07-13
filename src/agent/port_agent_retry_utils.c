/* Slermes C port — agent/retry_utils.py (pure Z.AI coding-overload classifier) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>

/* Mirrors the error object shape the Python classifier reads:
 * status_code + flattened text (message/body/response lowercased). */
typedef struct {
    int status_code;
    char text[8192];
} retry_utils_err_t;

/* PoP: agent_retry_utils_is_zai_coding_overload_error @ agent/retry_utils.py:is_zai_coding_overload_error */
bool agent_retry_utils_is_zai_coding_overload_error(const char *base_url, const char *model, const retry_utils_err_t *err)
{
    if (!err) return false;
    char base[1024], mdl[1024], txt[8192];
    size_t n;
    n = 0; for (const char *p = base_url ? base_url : ""; *p && n + 1 < sizeof(base); p++) base[n++] = (char)tolower((unsigned char)*p); base[n] = '\0';
    n = 0; for (const char *p = model ? model : ""; *p && n + 1 < sizeof(mdl); p++) mdl[n++] = (char)tolower((unsigned char)*p); mdl[n] = '\0';
    n = 0; for (const char *p = err->text; *p && n + 1 < sizeof(txt); p++) txt[n++] = (char)tolower((unsigned char)*p); txt[n] = '\0';

    if (err->status_code != 429) return false;
    if (!strstr(base, "api.z.ai/api/coding/paas/v4")) return false;
    if (!strstr(mdl, "glm-5.2")) return false;
    if (strstr(txt, "1305") || strstr(txt, "temporarily overloaded")) return true;
    return false;
}

/* PoP: agent_retry_utils__error_text @ agent/retry_utils.py:_error_text */
/* Best-effort flattened provider error text for retry classification.
 * Accepts up to 3 string fragments (message/body/response) plus an
 * explicit status_code (ignored for text). Faithful to the Python
 * " ".join(...).lower() with None filtering. */
char *agent_retry_utils__error_text(int status_code, const char *msg, const char *body, const char *response)
{
    (void)status_code;
    char buf[16384];
    size_t n = 0;
    buf[0] = '\0';
    const char *parts[3] = { msg, body, response };
    for (int i = 0; i < 3; i++) {
        if (!parts[i] || !parts[i][0]) continue;
        if (n) { buf[n++] = ' '; if (n >= sizeof(buf) - 1) break; }
        for (const char *p = parts[i]; *p && n + 1 < sizeof(buf); p++) buf[n++] = *p;
    }
    buf[n] = '\0';
    char *out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    for (size_t i = 0; i <= n; i++) out[i] = (char)tolower((unsigned char)buf[i]);
    return out;
}

/* Local jittered backoff (mirrors retry_utils.jittered_backoff 1-based). */
static double ru_jittered_backoff(int attempt, double base_delay, double max_delay, double jitter_ratio)
{
    if (attempt < 1) attempt = 1;
    double exponent = attempt - 1 > 62 ? 62 : (attempt - 1);
    double delay = (exponent >= 63 || base_delay <= 0) ? max_delay : base_delay;
    if (exponent < 63 && base_delay > 0) {
        double e = 1.0;
        for (int i = 0; i < (int)exponent; i++) e *= 2.0;
        delay = e * base_delay;
        if (delay > max_delay) delay = max_delay;
    }
    /* Deterministic jitter from time + attempt (no global lock needed; faithful to shape). */
    unsigned long long seed = ((unsigned long long)time(NULL) ^ ((unsigned long long)attempt * 0x9E3779B9u)) & 0xFFFFFFFFu;
    double r = ((double)(seed % 1000000) / 1000000.0); /* uniform [0,1) */
    double jitter = r * jitter_ratio * delay;
    return delay + jitter;
}

/* PoP: agent_retry_utils_adaptive_rate_limit_backoff @ agent/retry_utils.py:adaptive_rate_limit_backoff */
/* Returns malloc'd "(wait_seconds, reason_label|empty)" packed as "W|label". */
char *agent_retry_utils_adaptive_rate_limit_backoff(int attempt, const char *base_url, const char *model,
                                                     const retry_utils_err_t *err, double default_wait,
                                                     int short_attempts)
{
    if (!agent_retry_utils_is_zai_coding_overload_error(base_url, model, err))
        return NULL; /* caller keeps default_wait, no label */
    if (attempt <= short_attempts)
        return strdup("SHORT");
    static const double LONG_BACKOFF[] = { 30.0, 60.0, 90.0, 120.0 };
    int idx = attempt - short_attempts - 1;
    if (idx < 0) idx = 0;
    if (idx >= (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])))
        idx = (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])) - 1;
    double base_delay = LONG_BACKOFF[idx];
    double wait = ru_jittered_backoff(1, base_delay, base_delay, 0.2);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f|zai_coding_overload_long", wait);
    return strdup(buf);
}
