/*
 * transcription_provider.c — Transcription Provider abstraction.
 *
 * Port of Python agent/transcription_provider.py (193 lines), a plugin-based
 * STT backend ABC. In the C port this is NOT a dead "N/A" — the real STT
 * engine lives in lib/libtranscribe (transcribe_audio / provider registry /
 * setup schema) and src/tools/transcribe.c. This file is the C equivalent of
 * the Python TranscriptionProvider ABC: an opaque handle plus vtable-style
 * accessors backed by libtranscribe.
 */
#include "hermes_core_types.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "transcription_provider.h" /* transcription_provider_t */
#include "transcribe.h"        /* transcribe_audio(), transcribe_supported_formats[] */
#include "transcribe_helpers.h"/* transcribe_find_whisper_binary(), setup schema helpers */
#include "hermes_transcription.h" /* transcription_register_provider(), list/get */

/* Opaque transcription provider handle.
 * Port of Python TranscriptionProvider (the ABC instance). */
struct transcription_provider {
    char name[64];          /* provider registry key, e.g. "openai", "groq", "xai" */
    char display_name[128]; /* human label */
    char model[64];         /* model id, or "" for auto-detect */
};

/* ------------------------------------------------------------------ */
/* Opaque-handle lifecycle (port of TranscriptionProvider.__init__)     */
/* ------------------------------------------------------------------ */

/* Create a transcription provider backed by a registered libtranscribe
 * provider type. Returns NULL if the provider type is unknown.
 * Port of Python TranscriptionProvider.__init__(name, ...). */
transcription_provider_t *transcription_provider_create(const char *name,
                                                         const char *display_name,
                                                         const char *model)
{
    if (!name || !name[0]) return NULL;
    transcription_provider_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    snprintf(p->name, sizeof(p->name), "%s", name);
    snprintf(p->display_name, sizeof(p->display_name), "%s",
             (display_name && *display_name) ? display_name : name);
    snprintf(p->model, sizeof(p->model), "%s", model ? model : "");
    return p;
}

void transcription_provider_free(transcription_provider_t *p) {
    free(p);
}

/* ------------------------------------------------------------------ */
/* ABC method equivalents                                              */
/* ------------------------------------------------------------------ */

/* Port of Python TranscriptionProvider.name (property). */
const char *transcription_provider_name(const transcription_provider_t *p) {
    return p ? p->name : "";
}

/* Port of Python TranscriptionProvider.display_name (property). */
const char *transcription_provider_display_name(const transcription_provider_t *p) {
    return p ? p->display_name : "";
}

/* Port of Python TranscriptionProvider.is_available() — true when the
 * provider is registered and has a usable backend (binary present for local
 * providers, env credential for cloud providers). */
bool transcription_provider_is_available(const transcription_provider_t *p) {
    if (!p || !p->name[0]) return false;
    /* Cloud providers need an API key in the environment. */
    if (strcmp(p->name, "openai") == 0) return getenv("OPENAI_API_KEY") != NULL;
    if (strcmp(p->name, "groq")  == 0) return getenv("GROQ_API_KEY")  != NULL;
    if (strcmp(p->name, "xai")   == 0) return getenv("XAI_API_KEY")   != NULL;
    if (strcmp(p->name, "deepgram") == 0) return getenv("DEEPGRAM_API_KEY") != NULL;
    /* Local providers need the whisper binary. */
    if (strcmp(p->name, "local") == 0 || strcmp(p->name, "local_command") == 0)
        return transcribe_find_whisper_binary() != NULL;
    /* Already-registered registry entries are available. */
    return transcription_get_provider(p->name) != NULL;
}

/* Port of Python TranscriptionProvider.transcribe() — dispatch to the real
 * STT engine. Returns malloc'd transcript text (or NULL on failure). Caller
 * free()s. */
char *transcription_provider_transcribe(transcription_provider_t *p,
                                         const char *audio_path)
{
    if (!p || !audio_path || !audio_path[0]) return NULL;
    const char *model = p->model[0] ? p->model : NULL;
    char *json = transcribe_audio(audio_path, model);
    if (!json) return NULL;
    char *text = (char *)transcribe_extract_transcript_text(json);
    free(json);
    return text;
}

/* Port of Python TranscriptionProvider.get_setup_schema() — JSON schema for
 * configuring this provider. Returns malloc'd string. Caller free()s. */
char *transcription_provider_get_setup_schema(const transcription_provider_t *p) {
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"object\",\"title\":\"%s\",\"properties\":{"
             "\"model\":{\"type\":\"string\",\"description\":\"STT model id for %s\"},"
             "\"language\":{\"type\":\"string\",\"description\":\"ISO-639-1 language code\"}}}",
             p ? p->display_name : "Transcription",
             p ? p->name : "");
    return strdup(buf);
}

/* ------------------------------------------------------------------ */
/* Convenience: register a provider in the global registry.            */
/* ------------------------------------------------------------------ */

void transcription_provider_register(const char *name, const char *type) {
    transcription_register_provider(name, type);
}
