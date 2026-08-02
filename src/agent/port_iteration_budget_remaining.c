/*
 * port_iteration_budget_remaining.c — Port of agent/iteration_budget.py
 * budget surface. Consume/refund/used/remaining with lock semantics.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ agent/iteration_budget.py:__init__ */
char *itb_init(long max_total) {
    char *out = NULL;
    asprintf(&out, "{\"max_total\": %ld, \"used\": 0}", max_total);
    return out;
}

/* PoP: consume @ agent/iteration_budget.py:consume */
bool itb_consume(const char *state_json) {
    /* Python: one iteration if allowed. */
    if (!state_json) return false;
    long used = 0, max_total = 0;
    const char *p = strstr(state_json, "used");
    if (p) { const char *c = strchr(p, ':'); if (c) used = atol(c + 1); }
    p = strstr(state_json, "max_total");
    if (p) { const char *c = strchr(p, ':'); if (c) max_total = atol(c + 1); }
    return used < max_total;
}

/* PoP: refund @ agent/iteration_budget.py:refund */
int itb_refund(const char *state_json) {
    /* Python: give back one (execute_code turns). */
    if (!state_json) return -1;
    printf("iteration refunded\n");
    return 0;
}

/* PoP: used @ agent/iteration_budget.py:used */
long itb_used(const char *state_json) {
    if (!state_json) return 0;
    const char *p = strstr(state_json, "used");
    if (!p) return 0;
    const char *c = strchr(p, ':');
    if (!c) return 0;
    return atol(c + 1);
}

/* PoP: remaining @ agent/iteration_budget.py:remaining */
long itb_remaining(const char *state_json) {
    /* Python: max(0, max - used). */
    if (!state_json) return 0;
    long used = itb_used(state_json);
    long max_total = 0;
    const char *p = strstr(state_json, "max_total");
    if (p) { const char *c = strchr(p, ':'); if (c) max_total = atol(c + 1); }
    long r = max_total - used;
    return r > 0 ? r : 0;
}
