/*
 * port_hermes_state_remaining2.c — Port of hermes_state.py error surface.
 * Session-not-found envelopes.
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

/* PoP: __init__ @ hermes_state.py:__init__ */
char *hst_init(const char *session_id) {
    /* Python: session-not-found error. */
    if (!session_id) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"error\": \"Session %s not found\"}", session_id);
    return out;
}
