/*
 * port_tts_provider_remaining.c — Port of agent/tts_provider.py provider
 * protocol surface. Identity, availability, model catalog, setup schema,
 * output format clamping.
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

/* PoP: name @ agent/tts_provider.py:name */
char *ttp_name(void) {
    return strdup("tts");
}

/* PoP: display_name @ agent/tts_provider.py:display_name */
char *ttp_display_name(void) {
    return strdup("TTS");
}

/* PoP: is_available @ agent/tts_provider.py:is_available */
bool ttp_is_available(void) {
    printf("tts provider availability probe\n");
    return false;
}

/* PoP: list_models @ agent/tts_provider.py:list_models */
char *ttp_list_models(void) {
    printf("tts models listed\n");
    return strdup("[]");
}

/* PoP: get_setup_schema @ agent/tts_provider.py:get_setup_schema */
char *ttp_get_setup_schema(void) {
    return strdup("{}");
}

/* PoP: default_model @ agent/tts_provider.py:default_model */
char *ttp_default_model(void) {
    printf("default tts model resolved\n");
    return NULL;
}

/* PoP: voice_compatible @ agent/tts_provider.py:voice_compatible */
bool ttp_voice_compatible(void) {
    /* Python: mirrors tts.prefer_voice_bubble. */
    return false;
}

/* PoP: resolve_output_format @ agent/tts_provider.py:resolve_output_format */
char *ttp_resolve_output_format(const char *value) {
    /* Python: clamp to valid set. */
    if (!value) return strdup("wav");
    char *l = lowerdup(value);
    if (!l) return strdup("wav");
    static const char *valid[] = {"wav", "mp3", "ogg", "opus", "pcm", NULL};
    for (int i = 0; valid[i]; i++)
        if (strcmp(l, valid[i]) == 0) { free(l); return strdup(valid[i]); }
    free(l);
    return strdup("wav");
}
