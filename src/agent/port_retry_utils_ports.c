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
long rtu_zai_coding_overload_retry_ceiling(void) {
    /* Python: ceiling for full overload schedule. */
    return 10;
}
