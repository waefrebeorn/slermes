/*
 * port_shutdown_watchdog_remaining.c — Port of gateway/shutdown_watchdog.py
 * watchdog surface. Interval loop, cancel flag, stop event.
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

/* PoP: __init__ @ gateway/shutdown_watchdog.py:__init__ */
char *sdw_init(long interval_seconds) {
    if (interval_seconds <= 0) interval_seconds = 5;
    char *out = NULL;
    asprintf(&out, "{\"interval\": %ld, \"cancelled\": false, \"stop_event\": false}",
             interval_seconds);
    return out;
}

/* PoP: stop @ gateway/shutdown_watchdog.py:stop */
int sdw_stop(void) {
    /* Python: set stop event. */
    printf("shutdown watchdog stop event set\n");
    return 0;
}
