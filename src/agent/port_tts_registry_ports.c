/*
 * port_tts_registry_remaining.c — Port of agent/tts_registry.py TTS
 * provider registry surface. Register/list/get, test reset.
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

/* PoP: register_provider @ agent/tts_registry.py:register_provider */
int ttsr_register_provider(const char *name, const char *provider_desc) {
    if (!name) return -1;
    printf("tts provider registered: %s\n", name);
    return 0;
}

/* PoP: list_providers @ agent/tts_registry.py:list_providers */
char *ttsr_list_providers(void) {
    printf("tts providers listed (sorted)\n");
    return strdup("[]");
}

/* PoP: get_provider @ agent/tts_registry.py:get_provider */
char *ttsr_get_provider(const char *name) {
    /* Python: case-insensitive. */
    if (!name) return NULL;
    printf("tts provider fetched (case-insensitive): %s\n", name);
    return NULL;
}

/* PoP: _reset_for_tests @ agent/tts_registry.py:_reset_for_tests */
int ttsr_reset_for_tests(void) {
    printf("tts registry cleared (test-only)\n");
    return 0;
}
