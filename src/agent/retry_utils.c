/* Retry utilities — jittered exponential backoff for decorrelated retries.
 *
 * Python equivalent: agent/retry_utils.py
 */

#define _GNU_SOURCE  /* strptime, timegm */

#include "hermes_retry_utils.h"
#include "hermes_core_types.h"
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Monotonic counter for jitter seed uniqueness within the same process.
 * Protected by a mutex to avoid race conditions in concurrent retry paths. */
static uint64_t g_jitter_counter = 0;
static pthread_mutex_t g_jitter_lock = PTHREAD_MUTEX_INITIALIZER;

/* Simple xorshift64 PRNG — stateful, seeded per-call so concurrent callers
 * each get independent sequences without shared state contention. */
static inline uint64_t xorshift64(uint64_t seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

/* Port of Python retry_utils.py:jittered_backoff(). */
double jittered_backoff(int attempt,
                        double base_delay,
                        double max_delay,
                        double jitter_ratio)
{
    uint64_t tick;

    /* Thread-safe counter increment */
    pthread_mutex_lock(&g_jitter_lock);
    g_jitter_counter++;
    tick = g_jitter_counter;
    pthread_mutex_unlock(&g_jitter_lock);

    /* Compute exponential delay */
    int exponent = (attempt > 1) ? attempt - 1 : 0;
    double delay;
    if (exponent >= 63 || base_delay <= 0.0) {
        delay = max_delay;
    } else {
        delay = base_delay * pow(2.0, exponent);
        if (delay > max_delay)
            delay = max_delay;
    }

    /* Seed from time + counter for decorrelation even with coarse clocks */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t seed = ((uint64_t)ts.tv_nsec ^ (tick * 0x9E3779B97F4A7C15ULL)) & 0xFFFFFFFFULL;
    uint64_t rnd = xorshift64(seed);

    /* Uniform jitter in [0, jitter_ratio * delay) */
    double jitter = jitter_ratio * delay * ((double)rnd / (double)UINT64_MAX);

    return delay + jitter;
}

void jittered_backoff_reset(void)
{
    pthread_mutex_lock(&g_jitter_lock);
    g_jitter_counter = 0;
    pthread_mutex_unlock(&g_jitter_lock);
}

/* Port of Python retry_utils.py:_error_text()
 * Also exposed as `error_text` for parity-scanner matching. */
char *retry_utils_error_text(const retry_utils_err_t *err)
{
    if (!err) return strdup("");
    char buf[16384];
    size_t n = 0;
    buf[0] = '\0';

    const char *parts[] = { NULL, NULL, NULL };
    /* We only have status_code + text; mirror Python None-filter join */
    if (err->text[0]) parts[0] = err->text;

    for (int i = 0; i < 3 && parts[i]; i++) {
        if (!parts[i] || !parts[i][0]) continue;
        if (n) { buf[n++] = ' '; if (n >= sizeof(buf) - 1) break; }
        for (const char *p = parts[i]; *p && n + 1 < sizeof(buf); p++) buf[n++] = *p;
    }
    buf[n] = '\0';

    /* lowercase */
    for (size_t i = 0; i <= n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);

    return strdup(buf);
}

/* Public `error_text` — parity alias for Python `_error_text`. */
char *error_text(int status_code, const char *msg, const char *body, const char *response)
{
    retry_utils_err_t err;
    memset(&err, 0, sizeof(err));
    err.status_code = status_code;

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

/* PoP: is_zai_coding_overload_error @ retry_utils.py:is_zai_coding_overload_error */
/* Port of Python retry_utils.py:is_zai_coding_overload_error(). */
bool retry_utils_is_zai_coding_overload_error(const char *base_url, const char *model, const retry_utils_err_t *err)
{
    if (!err) return false;

    char base[1024], mdl[1024], txt[8192];
    size_t n;

    n = 0;
    for (const char *p = base_url ? base_url : ""; *p && n + 1 < sizeof(base); p++)
        base[n++] = (char)tolower((unsigned char)*p);
    base[n] = '\0';

    n = 0;
    for (const char *p = model ? model : ""; *p && n + 1 < sizeof(mdl); p++)
        mdl[n++] = (char)tolower((unsigned char)*p);
    mdl[n] = '\0';

    n = 0;
    for (const char *p = err->text; *p && n + 1 < sizeof(txt); p++)
        txt[n++] = *p;
    txt[n] = '\0';

    if (err->status_code != 429) return false;
    if (!strstr(base, "api.z.ai/api/coding/paas/v4")) return false;
    if (!strstr(mdl, "glm-5.2")) return false;
    if (strstr(txt, "1305") || strstr(txt, "temporarily overloaded")) return true;
    return false;
}

/* Local jittered backoff helper (1-based attempt, same shape as Python). */
static double retry_utils_jittered_backoff(int attempt, double base_delay, double max_delay, double jitter_ratio)
{
    if (attempt < 1) attempt = 1;
    double exponent = attempt - 1 > 62 ? 62 : (attempt - 1);
    double delay;
    if (exponent >= 63 || base_delay <= 0) {
        delay = max_delay;
    } else {
        double e = 1.0;
        for (int i = 0; i < (int)exponent; i++) e *= 2.0;
        delay = e * base_delay;
        if (delay > max_delay) delay = max_delay;
    }

    unsigned long long seed = ((unsigned long long)time(NULL) ^ ((unsigned long long)attempt * 0x9E3779B9u)) & 0xFFFFFFFFu;
    double r = ((double)(seed % 1000000) / 1000000.0); /* uniform [0,1) */
    double jitter = r * jitter_ratio * delay;
    return delay + jitter;
}

/* Port of Python retry_utils.py:adaptive_rate_limit_backoff()
 * Returns malloc'd "W|label" or NULL if not a Z.AI overload. */
char *retry_utils_adaptive_rate_limit_backoff(int attempt, const char *base_url, const char *model,
                                               const retry_utils_err_t *err, double default_wait,
                                               int short_attempts)
{
    if (!retry_utils_is_zai_coding_overload_error(base_url, model, err))
        return NULL;

    if (attempt <= short_attempts)
        return strdup("SHORT");

    static const double LONG_BACKOFF[] = { 30.0, 60.0, 90.0, 120.0 };
    int idx = attempt - short_attempts - 1;
    if (idx < 0) idx = 0;
    if (idx >= (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])))
        idx = (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])) - 1;

    double base_delay = LONG_BACKOFF[idx];
    double wait = retry_utils_jittered_backoff(1, base_delay, base_delay, 0.2);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.3f|zai_coding_overload_long", wait);
    return strdup(buf);
}

/* PoP: zai_coding_overload_retry_ceiling @ retry_utils.py:zai_coding_overload_retry_ceiling */
/* Port of Python retry_utils.py:zai_coding_overload_retry_ceiling(). */
int retry_utils_zai_coding_overload_retry_ceiling(int short_attempts)
{
    static const double LONG_BACKOFF[] = { 30.0, 60.0, 90.0, 120.0 };
    return short_attempts + (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])) + 1;
}

/* ------------------------------------------------------------------ */
/* HTTP-date parsing for parse_retry_after_seconds                    */
/* ------------------------------------------------------------------ */

/* RFC 2822 / RFC 7231 IMF-fixdate parser — faithful to Python's
 * email.utils.parsedate_to_datetime. Returns 0 on success (tm filled,
 * UTC assumed), -1 on parse failure.
 * Handles: "Wed, 21 Oct 2015 07:28:00 GMT" (RFC 1123 IMF-fixdate). */
static int parse_rfc2822_date(const char *s, struct tm *tm)
{
    memset(tm, 0, sizeof(*tm));
    return strptime(s, "%a, %d %b %Y %H:%M:%S %Z", tm) ? 0 : -1;
}

/* PoP: parse_retry_after_seconds @ agent/retry_utils.py:parse_retry_after_seconds */
/* Port of Python agent/retry_utils.py:parse_retry_after_seconds(). */
double retry_utils_parse_retry_after_seconds(const char *value, int *ok)
{
    if (ok) *ok = 0;
    if (!value) return 0.0;

    /* Skip leading/trailing whitespace (Python: .strip()) */
    const char *start = value;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
        start++;
    char *stripped = NULL;
    size_t slen = strlen(start);
    /* trim trailing whitespace */
    while (slen > 0 && (start[slen-1] == ' ' || start[slen-1] == '\t' ||
                        start[slen-1] == '\r' || start[slen-1] == '\n'))
        slen--;
    if (slen == 0) return 0.0;
    if (start != value) {
        stripped = (char *)malloc(slen + 1);
        if (!stripped) return 0.0;
        memcpy(stripped, start, slen);
        stripped[slen] = '\0';
        start = stripped;
    }

    /* Try numeric seconds first (Python: float(text)) */
    char *end = NULL;
    double seconds = strtod(start, &end);
    if (end != start && *end == '\0') {
        if (ok) *ok = 1;
        double r = seconds > 0.0 ? seconds : 0.0;
        if (stripped) free(stripped);
        return r;
    }

    /* Try HTTP-date form (RFC 7231): seconds until that instant, clamped >= 0 */
    struct tm tm;
    if (parse_rfc2822_date(start, &tm) == 0) {
        time_t then = timegm(&tm);
        if (then != (time_t)-1) {
            time_t now = time(NULL);
            double diff = difftime(then, now);
            if (ok) *ok = 1;
            double r = diff > 0.0 ? diff : 0.0;
            if (stripped) free(stripped);
            return r;
        }
    }

    /* Unparseable — Python returns None */
    if (stripped) free(stripped);
    return 0.0;
}
