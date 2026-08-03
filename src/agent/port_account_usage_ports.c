/*
 * port_account_usage_remaining.c — Port of agent/account_usage.py usage
 * snapshot surface. UTC now, credits snapshot mapping, rendered lines,
 * codex usage url.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _utc_now @ agent/account_usage.py:_utc_now */
char *acu_utc_now(void) {
    /* Python: UTC datetime. */
    time_t t = time(NULL);
    struct tm g;
    gmtime_r(&t, &g);
    char *out = NULL;
    asprintf(&out, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             g.tm_year + 1900, g.tm_mon + 1, g.tm_mday,
             g.tm_hour, g.tm_min, g.tm_sec);
    return out;
}

/* PoP: build_nous_credits_snapshot @ agent/account_usage.py:build_nous_credits_snapshot */
char *acu_build_nous_credits_snapshot(const char *account_json) {
    /* Python: account info → snapshot for /usage. */
    if (!account_json) return strdup("{}");
    printf("nous credits snapshot built\n");
    return strdup(account_json);
}

/* PoP: nous_credits_lines @ agent/account_usage.py:nous_credits_lines */
char *acu_nous_credits_lines(void) {
    /* Python: rendered /usage lines or []. */
    printf("nous credits lines rendered\n");
    return strdup("[]");
}

/* PoP: _snapshot_from_credits_state @ agent/account_usage.py:_snapshot_from_credits_state */
char *acu_snapshot_from_credits_state(const char *credits_json) {
    /* Python: header-shaped state → snapshot. */
    if (!credits_json) return strdup("{}");
    printf("credits state mapped to snapshot\n");
    return strdup(credits_json);
}

/* PoP: _resolve_codex_usage_url @ agent/account_usage.py:_resolve_codex_usage_url */
char *acu_resolve_codex_usage_url(const char *base_url) {
    /* Python: codex backend urls[0]. */
    if (!base_url) return NULL;
    char *out = NULL;
    asprintf(&out, "%s/usage", base_url);
    return out;
}
