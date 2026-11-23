/*
 * rate_limit_credits.c — C home for upstream Hermes agent/rate_limit_credits.py.
 *
 * Faithful C11 ports of the pure header/credit helpers split out of the mixin.
 * Provider-host detection and the credits-notices config value are injected
 * through the args object ("nous_host_match", "config_value") instead of
 * reading live config / performing DNS-free host matching in C.
 *
 * PoP: w10_ratelimit_e741d9_is_nous_agent @ agent/rate_limit_credits.py:_is_nous_agent
 * PoP: w10_ratelimit_e741d9_pct @ agent/rate_limit_credits.py:_pct
 * PoP: w10_ratelimit_e741d9_credits_spent_micros @ agent/rate_limit_credits.py:RateLimitCreditsMixin.get_credits_spent_micros
 * PoP: w10_ratelimit_e741d9_notices_enabled @ agent/rate_limit_credits.py:RateLimitCreditsMixin._credits_notices_enabled
 * PoP: w10_ratelimit_e741d9_openrouter_cache_status @ agent/rate_limit_credits.py:RateLimitCreditsMixin._check_openrouter_cache_status
 */
#include "noc_w10_ratelimit_e741d9.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *py_strip(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\n'||s[a]=='\r'||s[a]=='\f'||s[a]=='\v')) a++;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\n'||s[b-1]=='\r'||s[b-1]=='\f'||s[b-1]=='\v')) b--;
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, s + a, b - a); r[b - a] = '\0'; return r;
}
static char *py_lower(const char *s) {
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) r[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i]-'A'+'a') : s[i];
    r[n] = '\0'; return r;
}
static char *py_upper(const char *s) {
    size_t n = strlen(s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    for (size_t i = 0; i < n; i++) r[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i]-'a'+'A') : s[i];
    r[n] = '\0'; return r;
}

/* PoP: w10_ratelimit_e741d9_is_nous_agent @ agent/rate_limit_credits.py:_is_nous_agent */
json_t *w10_ratelimit_e741d9_is_nous_agent(json_t *a)
{
    const char *provider = json_get_str(a, "provider", "");
    char *st = py_strip(provider ? provider : "");
    char *low = py_lower(st ? st : "");
    int nous = low && !strcmp(low, "nous");
    free(st); free(low);
    if (nous) return json_bool(true);
    const char *base = json_get_str(a, "base_url", "");
    int has_base = base && *base;
    return json_bool(has_base && json_get_bool(a, "nous_host_match", false));
}

/* PoP: w10_ratelimit_e741d9_pct @ agent/rate_limit_credits.py:_pct */
json_t *w10_ratelimit_e741d9_pct(json_t *a)
{
    json_t *f = a ? json_obj_get(a, "fraction") : NULL;
    if (!f || json_is_null(f)) return json_string("n/a");
    char buf[64];
    snprintf(buf, sizeof(buf), "%.0f%%", json_number_value(f) * 100.0);
    return json_string(buf);
}

/* PoP: w10_ratelimit_e741d9_credits_spent_micros @ agent/rate_limit_credits.py:RateLimitCreditsMixin.get_credits_spent_micros */
json_t *w10_ratelimit_e741d9_credits_spent_micros(json_t *a)
{
    json_t *s = a ? json_obj_get(a, "session_start") : NULL;
    json_t *r = a ? json_obj_get(a, "remaining") : NULL;
    if (!json_is_string(s) || !json_is_string(r)) return json_null();
    long long start = strtoll(json_string_value(s), NULL, 10);
    long long rem = strtoll(json_string_value(r), NULL, 10);
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", start - rem);
    return json_string(buf);
}

/* PoP: w10_ratelimit_e741d9_notices_enabled @ agent/rate_limit_credits.py:RateLimitCreditsMixin._credits_notices_enabled */
json_t *w10_ratelimit_e741d9_notices_enabled(json_t *a)
{
    json_t *cache = a ? json_obj_get(a, "cache") : NULL;
    if (json_is_bool(cache)) return json_bool(json_bool_value(cache));
    json_t *cv = a ? json_obj_get(a, "config_value") : NULL;
    if (json_is_bool(cv)) return json_bool(json_bool_value(cv));
    return json_bool(true);  /* fail-open default in the Python source */
}

/* PoP: w10_ratelimit_e741d9_openrouter_cache_status @ agent/rate_limit_credits.py:RateLimitCreditsMixin._check_openrouter_cache_status */
json_t *w10_ratelimit_e741d9_openrouter_cache_status(json_t *a)
{
    const char *status = json_get_str(a, "status", "");
    int hit = 0;
    if (status && *status) {
        char *up = py_upper(status);
        hit = up && !strcmp(up, "HIT");
        free(up);
    }
    long long count = 0;
    json_t *cb = a ? json_obj_get(a, "count_before") : NULL;
    if (json_is_string(cb)) count = strtoll(json_string_value(cb), NULL, 10);
    else if (json_is_number(cb)) count = (long long)json_number_value(cb);
    if (hit) count++;
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", count);
    json_t *out = json_object();
    json_set(out, "hit", json_bool(hit));
    json_set(out, "count", json_string(buf));
    return out;
}
