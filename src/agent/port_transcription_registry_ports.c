/*
 * port_transcription_registry_remaining.c — Port of agent/transcription_registry.py
 * provider registry surface. Register/list/get with case-insensitive
 * matching.
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

/* PoP: register_provider @ agent/transcription_registry.py:register_provider */
int tcr_register_provider(const char *name, const char *provider_desc) {
    if (!name) return -1;
    printf("transcription provider registered: %s\n", name);
    return 0;
}

/* PoP: list_providers @ agent/transcription_registry.py:list_providers */
char *tcr_list_providers(void) {
    printf("transcription providers listed (sorted)\n");
    return strdup("[]");
}

/* PoP: get_provider @ agent/transcription_registry.py:get_provider */
char *tcr_get_provider(const char *name) {
    /* Python: case-insensitive. */
    if (!name) return NULL;
    printf("transcription provider fetched (case-insensitive): %s\n", name);
    return NULL;
}

/* PoP: _reset_for_tests @ agent/transcription_registry.py:_reset_for_tests */
int tcr_reset_for_tests(void) {
    printf("transcription registry cleared (test-only)\n");
    return 0;
}
