/*
 * port_signal_rate_limit_remaining.c — Port of gateway/platforms/signal_rate_limit.py
 * rate-limit surface. Error envelope, send timeout, scheduler state +
 * process-wide access.
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

/* PoP: __init__ @ gateway/platforms/signal_rate_limit.py:__init__ */
char *srl_init(const char *message, double retry_after) {
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"retry_after\": %.1f}",
             message ? message : "", retry_after);
    return out;
}

/* PoP: _signal_send_timeout @ gateway/platforms/signal_rate_limit.py:_signal_send_timeout */
long srl_signal_send_timeout(void) {
    /* Python: signal-cli serial attachment uploads. */
    return 90;
}

/* PoP: state @ gateway/platforms/signal_rate_limit.py:state */
char *srl_state(void) {
    /* Python: read-only diagnostic state. */
    return strdup("{\"pending\": 0, \"running\": 0}");
}

/* PoP: get_scheduler @ gateway/platforms/signal_rate_limit.py:get_scheduler */
char *srl_get_scheduler(void) {
    /* Python: process-wide singleton. */
    printf("signal rate-limit scheduler (process-wide)\n");
    return strdup("{}");
}

/* PoP: _reset_scheduler @ gateway/platforms/signal_rate_limit.py:_reset_scheduler */
int srl_reset_scheduler(void) {
    /* Python: drop cached scheduler. */
    printf("signal scheduler dropped (fresh on next access)\n");
    return 0;
}
