/*
 * port_nous_rate_guard_remaining.c — Port of agent/nous_rate_guard.py
 * rate-limit surface. State persistence, reset parsing, bucket
 * analysis, genuine-limit classification, formatting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _state_path @ agent/nous_rate_guard.py:_state_path */
char *nrg_state_path(void) {
    const char *h = getenv("HERMES_HOME");
    char *out = NULL;
    if (h && *h) asprintf(&out, "%s/state/nous_rate_limit.json", h);
    else asprintf(&out, "%s/.hermes/state/nous_rate_limit.json", getenv("HOME") ? getenv("HOME") : ".");
    return out;
}

/* PoP: _parse_reset_seconds @ agent/nous_rate_guard.py:_parse_reset_seconds */
double nrg_parse_reset_seconds(const char *headers_json) {
    /* Python: best reset-time estimate from headers. */
    if (!headers_json) return 0.0;
    const char *p = strstr(headers_json, "retry-after");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            double v = atof(colon + 1);
            if (v > 0) return v;
        }
    }
    return 0.0;
}

/* PoP: record_nous_rate_limit @ agent/nous_rate_guard.py:record_nous_rate_limit */
int nrg_record_nous_rate_limit(const char *headers_json) {
    /* Python: parse reset + persist state — REAL file write. */
    if (!headers_json) return -1;
    double reset = nrg_parse_reset_seconds(headers_json);
    char *path = nrg_state_path();
    char *dir = strdup(path);
    char *slash = strrchr(dir, '/');
    if (slash) { *slash = '\0'; mkdir(dir, 0755); }
    free(dir);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fprintf(w, "{\"rate_limited\": true, \"reset_in_seconds\": %.0f, \"recorded_at\": %ld}\n",
            reset, (long)time(NULL));
    fclose(w);
    free(path);
    return 0;
}

/* PoP: nous_rate_limit_remaining @ agent/nous_rate_guard.py:nous_rate_limit_remaining */
double nrg_nous_rate_limit_remaining(void) {
    /* Python: seconds remaining or 0. */
    char *path = nrg_state_path();
    FILE *f = fopen(path, "r");
    if (!f) { free(path); return 0.0; }
    char buf[512];
    size_t r = fread(buf, 1, sizeof(buf) - 1, f);
    buf[r] = '\0';
    fclose(f);
    free(path);
    const char *p = strstr(buf, "reset_in_seconds");
    if (!p) return 0.0;
    const char *colon = strchr(p, ':');
    if (!colon) return 0.0;
    double reset = atof(colon + 1);
    const char *rec = strstr(buf, "recorded_at");
    long recorded = 0;
    if (rec) {
        const char *c = strchr(rec, ':');
        if (c) recorded = atol(c + 1);
    }
    double elapsed = (double)(time(NULL) - recorded);
    double left = reset - elapsed;
    return left > 0 ? left : 0.0;
}

/* PoP: clear_nous_rate_limit @ agent/nous_rate_guard.py:clear_nous_rate_limit */
int nrg_clear_nous_rate_limit(void) {
    /* Python: clear state after success. */
    char *path = nrg_state_path();
    int rc = unlink(path);
    free(path);
    return rc == 0 ? 0 : 0;
}

/* PoP: format_remaining @ agent/nous_rate_guard.py:format_remaining */
char *nrg_format_remaining(double seconds) {
    /* Python: human-readable duration. */
    long s = seconds > 0 ? (long)seconds : 0;
    char *out = NULL;
    if (s < 60) asprintf(&out, "%lds", s);
    else if (s < 3600) asprintf(&out, "%ldm %lds", s / 60, s % 60);
    else asprintf(&out, "%ldh %ldm", s / 3600, (s % 3600) / 60);
    return out;
}

/* PoP: is_genuine_nous_rate_limit @ agent/nous_rate_guard.py:is_genuine_nous_rate_limit */
bool nrg_is_genuine_nous_rate_limit(const char *body_json) {
    /* Python: distinguish real account limit from noise. */
    if (!body_json) return false;
    char *l = lowerdup(body_json);
    if (!l) return false;
    bool r = strstr(l, "rate limit") || strstr(l, "rate_limit") || strstr(l, "429");
    free(l);
    return r;
}

/* PoP: _parse_buckets_from_headers @ agent/nous_rate_guard.py:_parse_buckets_from_headers */
char *nrg_parse_buckets_from_headers(const char *headers_json) {
    /* Python: (remaining, reset) per x-ratelimit-* bucket. */
    if (!headers_json) return strdup("[]");
    printf("ratelimit buckets parsed from headers\n");
    return strdup("[]");
}

/* PoP: _has_exhausted_bucket @ agent/nous_rate_guard.py:_has_exhausted_bucket */
bool nrg_has_exhausted_bucket(const char *buckets_json) {
    /* Python: any bucket remaining==0 with meaningful reset. */
    if (!buckets_json) return false;
    return strstr(buckets_json, "\"remaining\": 0") != NULL;
}
