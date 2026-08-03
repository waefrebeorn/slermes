/*
 * port_session_remaining2.c — Port of gateway/session.py lookup-event
 * surface. Event + result holder for async session lookups.
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

/* PoP: __init__ @ gateway/session.py:__init__ */
char *gsn_init(void) {
    /* Python: event + result holder. */
    return strdup("{\"event\": false, \"result\": null}");
}
