/*
 * port_retry_utils_remaining.c — Port of agent/retry_utils.py backoff
 * surface. Jittered exponential backoff, error flattening, zai overload
 * classification, adaptive backoff, retry ceilings.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: jittered_backoff @ agent/retry_utils.py:jittered_backoff */
double rtu_jittered_backoff(long attempt, double base_delay, double max_delay) {
    /* Python: exponential + jitter. */
    if (attempt < 1) attempt = 1;
    if (base_delay <= 0) base_delay = 1.0;
    if (max_delay <= 0) max_delay = 60.0;
    double exp = base_delay * pow(2.0, (double)(attempt - 1));
    if (exp > max_delay) exp = max_delay;
    double jitter = ((double)(rand() % 1000) / 1000.0) * exp * 0.5;
    return exp + jitter;
}

/* PoP: _error_text @ agent/retry_utils.py:_error_text */
char *rtu_error_text(const char *error_json) {
    /* Python: flattened provider error text. */
    if (!error_json) return strdup("");
    const char *m = strstr(error_json, "\"message\"");
    if (m) {
        const char *colon = strchr(m, ':');
        if (colon) {
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"') e++;
            if (e > v) return strndup(v, (size_t)(e - v));
        }
    }
    return strdup(error_json);
}

/* PoP: is_zai_coding_overload_error @ agent/retry_utils.py:is_zai_coding_overload_error */
bool rtu_is_zai_coding_overload_error(const char *error) {
    /* Python: Z.AI coding plan transient overload 429. */
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "overload") && (strstr(l, "coding") || strstr(l, "429"));
    free(l);
    return r;
}

/* PoP: adaptive_rate_limit_backoff @ agent/retry_utils.py:adaptive_rate_limit_backoff */
double rtu_adaptive_rate_limit_backoff(const char *provider, const char *error) {
    /* Python: provider-aware default backoff. */
    if (rtu_is_zai_coding_overload_error(error)) return 30.0;
    return 5.0;
}

/* PoP: zai_coding_overload_retry_ceiling @ agent/retry_utils.py:zai_coding_overload_retry_ceiling */
/* Python: ceiling = short_attempts + len(LONG_BACKOFF) + 1.
 * With defaults (short_attempts=3, len(LONG_BACKOFF)=4) => 8.
 * The +1 accounts for the retry-count>=ceiling check running before the
 * backoff is computed, so every long-tier entry is reachable. */
long rtu_zai_coding_overload_retry_ceiling(int short_attempts) {
    if (short_attempts < 0) short_attempts = 3;
    static const double LONG_BACKOFF[] = {30.0, 60.0, 90.0, 120.0};
    return short_attempts + (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])) + 1;
}

/* PoP: parse_retry_after_seconds @ agent/retry_utils.py:parse_retry_after_seconds */
double rtu_parse_retry_after_seconds(const char *value)
{
    /* Parse a "Retry-After" header value into non-negative seconds.
     * Accepts a numeric string / number / HTTP-date (RFC 7231). Returns -1
     * (sentinel for None) when absent or unparseable. Negative deltas are
     * clamped to 0.0. */
    if (!value) return -1.0;
    const char *raw = value;
    /* bool in Python → None; skip. */
    if (strcmp(raw, "true") == 0 || strcmp(raw, "false") == 0) return -1.0;
    /* trim */
    while (*raw == ' ' || *raw == '\t') raw++;
    const char *end = raw + strlen(raw);
    while (end > raw && (end[-1] == ' ' || end[-1] == '\t')) end--;
    if (raw == end) return -1.0;
    /* numeric string */
    char tmp[64];
    size_t len = (size_t)(end - raw);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, raw, len); tmp[len] = '\0';
    char *nend = NULL;
    double num = strtod(tmp, &nend);
    if (nend && *nend == '\0') {
        return num < 0.0 ? 0.0 : num;
    }
    /* HTTP-date form (RFC 7231): seconds until that instant, clamped at 0.
     * Formats: IMF-fixdate "Sun, 06 Nov 1994 08:49:37 GMT",
     * RFC 850 "Sunday, 06-Nov-94 08:49:37 GMT",
     * asctime "Sun Nov  6 08:49:37 1994". Dates are GMT. */
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    const char *fmts[] = {
        "%a, %d %b %Y %H:%M:%S GMT",   /* IMF-fixdate */
        "%A, %d-%b-%y %H:%M:%S GMT",   /* RFC 850 */
        "%a %b %e %H:%M:%S %Y",        /* asctime */
        NULL
    };
    for (int i = 0; fmts[i]; i++) {
        memset(&tm, 0, sizeof(tm));
        char *r = strptime(tmp, fmts[i], &tm);
        if (r && *r == '\0') {
            time_t when = timegm(&tm);
            time_t now = time(NULL);
            double secs = (double)(when - now);
            return secs < 0.0 ? 0.0 : secs;
        }
    }
    return -1.0;
}
