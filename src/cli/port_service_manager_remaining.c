/*
 * port_service_manager_remaining.c — Port of hermes_cli/service_manager.py
 * service error surface.
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

/* PoP: __init__ @ hermes_cli/service_manager.py:__init__ */
char *svm_init(const char *message, const char *service) {
    char *out = NULL;
    asprintf(&out, "{\"message\": \"%s\", \"service\": \"%s\"}",
             message ? message : "", service ? service : "");
    return out;
}
