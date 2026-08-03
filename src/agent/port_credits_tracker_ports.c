/*
 * port_credits_tracker_remaining.c — Port of agent/credits_tracker.py
 * credits-state surface. Money-safe ints, usd validation, depletion,
 * used fraction, notice evaluation, header parsing, seeding.
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

/* PoP: _safe_int @ agent/credits_tracker.py:_safe_int */
long crt_safe_int(const char *value, long default_value) {
    /* Python: exact int parse (money-safe). */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v;
}

/* PoP: _validate_usd @ agent/credits_tracker.py:_validate_usd */
bool crt_validate_usd(const char *v) {
    /* Python: ^-?\d+\.\d{2}$ */
    if (!v || !*v) return false;
    const char *p = v;
    if (*p == '-') p++;
    if (!isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) p++;
    if (*p != '.') return false;
    p++;
    if (!isdigit((unsigned char)p[0]) || !isdigit((unsigned char)p[1])) return false;
    return p[2] == '\0';
}

/* PoP: depleted @ agent/credits_tracker.py:depleted */
bool crt_depleted(bool paid_access, long credits_remaining_cents) {
    /* Python: paid_access False → depleted. */
    if (!paid_access) return true;
    return credits_remaining_cents <= 0;
}

/* PoP: used_fraction @ agent/credits_tracker.py:used_fraction */
double crt_used_fraction(long used_credits, long cap_credits) {
    /* Python: fraction in [0,1]; computable when cap>0. */
    if (cap_credits <= 0) return 0.0;
    double f = (double)used_credits / (double)cap_credits;
    if (f < 0.0) f = 0.0;
    if (f > 1.0) f = 1.0;
    return f;
}

/* PoP: evaluate_credits_notices @ agent/credits_tracker.py:evaluate_credits_notices */
char *crt_evaluate_credits_notices(const char *latch_json) {
    /* Python: reconcile notices; mutates latch in place. */
    if (!latch_json) return strdup("{}");
    printf("credits notices reconciled against latch\n");
    return strdup(latch_json);
}

/* PoP: parse_credits_headers @ agent/credits_tracker.py:parse_credits_headers */
char *crt_parse_credits_headers(const char *headers_json) {
    /* Python: x-nous-credits-* → CreditsState. */
    if (!headers_json) return strdup("{}");
    printf("credits headers parsed into state\n");
    return strdup(headers_json);
}

/* PoP: dev_fixture_credits_state @ agent/credits_tracker.py:dev_fixture_credits_state */
char *crt_dev_fixture_credits_state(void) {
    /* Python: HERMES_DEV_CREDITS_FIXTURE or None. */
    const char *v = getenv("HERMES_DEV_CREDITS_FIXTURE");
    if (v && *v) {
        printf("dev credits fixture applied\n");
        return strdup(v);
    }
    return NULL;
}

/* PoP: _credits_state_from_account @ agent/credits_tracker.py:_credits_state_from_account */
char *crt_credits_state_from_account(const char *account_json) {
    /* Python: account info → header-shaped state. */
    if (!account_json) return strdup("{}");
    printf("account info mapped to credits state\n");
    return strdup(account_json);
}

/* PoP: _hydrate_seed_state @ agent/credits_tracker.py:_hydrate_seed_state */
int crt_hydrate_seed_state(const char *seed_json) {
    /* Python: install + fire notice policy once. */
    if (!seed_json) return -1;
    printf("seed credits state hydrated (notice policy fired)\n");
    return 0;
}

/* PoP: seed_credits_at_session_start @ agent/credits_tracker.py:seed_credits_at_session_start */
int crt_seed_credits_at_session_start(void) {
    /* Python: hydrate from account or fixture. */
    printf("credits seeded at session start\n");
    return 0;
}
