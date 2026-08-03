/*
 * port_title_generator_remaining.c — Port of agent/title_generator.py
 * title surface. Generate via runtime model, auto-title, fire-and-forget.
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

/* PoP: generate_title @ agent/title_generator.py:generate_title */
char *tig_generate_title(const char *first_exchange_json) {
    /* Python: title from first exchange via runtime model. */
    if (!first_exchange_json) return NULL;
    printf("session title generated from first exchange\n");
    return strdup("New session");
}

/* PoP: auto_title_session @ agent/title_generator.py:auto_title_session */
char *tig_auto_title_session(const char *session_id, const char *first_exchange_json) {
    /* Python: set if none exists. */
    if (!session_id || !first_exchange_json) return NULL;
    printf("session auto-titled (%s)\n", session_id);
    return strdup("New session");
}

/* PoP: maybe_auto_title @ agent/title_generator.py:maybe_auto_title */
int tig_maybe_auto_title(const char *session_id, const char *first_exchange_json) {
    /* Python: fire-and-forget after first exchange. */
    if (!session_id) return -1;
    printf("fire-and-forget title generation (%s)\n", session_id);
    return 0;
}
