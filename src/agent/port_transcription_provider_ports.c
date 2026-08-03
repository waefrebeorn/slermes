/*
 * port_transcription_provider_remaining.c — Port of agent/transcription_provider.py
 * provider protocol surface. Identity, availability, transcribe.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "transcribe.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: name @ agent/transcription_provider.py:name */
char *tsp_name(void) {
    return strdup("stt");
}

/* PoP: is_available @ agent/transcription_provider.py:is_available */
bool tsp_is_available(void) {
    /* Python: plugin state check. C: libtranscribe backend availability. */
    extern bool transcription_is_available(void);
    return transcription_is_available();
}

/* PoP: transcribe @ agent/transcription_provider.py:transcribe */
char *tsp_transcribe(const char *file_path) {
    /* Python: standard result dict. Delegates to the real
     * transcribe_audio (libtranscribe whisper backends). */
    if (!file_path) return NULL;
    char *result = transcribe_audio(file_path, NULL);
    if (!result) return strdup("{\"success\":false,\"error\":\"no result\"}");
    return result;
}
