/*
 * port_browser_provider_remaining.c — Port of agent/browser_provider.py
 * cloud-browser provider protocol surface. Identity, availability,
 * setup schema, alias.
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

/* PoP: name @ agent/browser_provider.py:name */
char *brp_name(void) {
    return strdup("browser");
}

/* PoP: display_name @ agent/browser_provider.py:display_name */
char *brp_display_name(void) {
    return strdup("Browser");
}

/* PoP: is_available @ agent/browser_provider.py:is_available */
bool brp_is_available(void) {
    printf("browser provider availability probe\n");
    return false;
}

/* PoP: get_setup_schema @ agent/browser_provider.py:get_setup_schema */
char *brp_get_setup_schema(void) {
    return strdup("{}");
}

/* PoP: provider_name @ agent/browser_provider.py:provider_name */
char *brp_provider_name(void) {
    /* Python: backward-compat alias. */
    return strdup("Browser");
}
