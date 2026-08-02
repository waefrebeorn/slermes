/*
 * port_nous_portal_remaining.c — Port of hermes_cli/proxy/adapters/nous_portal.py
 * nous-portal adapter surface. Serialized proxy state, identity.
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

/* PoP: __init__ @ hermes_cli/proxy/adapters/nous_portal.py:__init__ */
char *npo_init(void) {
    /* Python: serialized proxy state + refresh. */
    return strdup("{\"lock\": \"process-local\", \"refresh\": \"cross-process\"}");
}

/* PoP: name @ hermes_cli/proxy/adapters/nous_portal.py:name */
char *npo_name(void) {
    return strdup("nous");
}

/* PoP: display_name @ hermes_cli/proxy/adapters/nous_portal.py:display_name */
char *npo_display_name(void) {
    return strdup("Nous Portal");
}
