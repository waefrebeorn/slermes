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

/* === merged from port_agent_transcription_provider.c === */
/*
 * port_agent_transcription_provider.c — C port of agent/transcription_provider.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_transcription_provider_display_name @ agent/transcription_provider.py:display_name */

/* Port of Python agent/transcription_provider.py:display_name */
/* Return the display name for a transcription provider. */
const char *cli_agent_transcription_provider_display_name(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return "Whisper Transcription";

    if (strcmp(provider_id, "openai") == 0) return "OpenAI Whisper";
    if (strcmp(provider_id, "local") == 0) return "Local Whisper (whisper.cpp)";
    if (strcmp(provider_id, "assemblyai") == 0) return "AssemblyAI";
    if (strcmp(provider_id, "deepgram") == 0) return "Deepgram";

    hermes_log(LOG_DEBUG, "port",
               "transcription_provider: unknown provider '%s', using default", provider_id);
    return "Whisper Transcription";
}

/* PoP: cli_agent_transcription_provider_list_models @ agent/transcription_provider.py:list_models */

/* Port of Python agent/transcription_provider.py:list_models */
/* List available transcription models. Returns JSON array. */
char *cli_agent_transcription_provider_list_models(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) {
        return strdup("[\"whisper-1\",\"whisper-large-v3\",\"base\",\"small\",\"medium\"]");
    }

    if (strcmp(provider_id, "openai") == 0) {
        return strdup("[\"whisper-1\"]");
    }
    if (strcmp(provider_id, "local") == 0) {
        return strdup("[\"whisper-large-v3\",\"whisper-medium\",\"whisper-small\",\"whisper-base\",\"whisper-tiny\"]");
    }

    hermes_log(LOG_DEBUG, "port",
               "transcription_provider: listing models for provider '%s'", provider_id);
    return strdup("[]");
}

/* PoP: cli_agent_transcription_provider_default_model @ agent/transcription_provider.py:default_model */

/* Port of Python agent/transcription_provider.py:default_model */
/* Return the default model for a transcription provider. */
const char *cli_agent_transcription_provider_default_model(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) return "whisper-1";

    if (strcmp(provider_id, "openai") == 0) return "whisper-1";
    if (strcmp(provider_id, "local") == 0) return "whisper-large-v3";
    if (strcmp(provider_id, "assemblyai") == 0) return "best";
    if (strcmp(provider_id, "deepgram") == 0) return "nova-2";

    return "whisper-1";
}

/* PoP: cli_agent_transcription_provider_get_setup_schema @ agent/transcription_provider.py:get_setup_schema */

/* Port of Python agent/transcription_provider.py:get_setup_schema */
/* Return the setup schema for a transcription provider. Returns JSON object. */
char *cli_agent_transcription_provider_get_setup_schema(const char *provider_id)
{
    if (!provider_id || !provider_id[0]) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"},"
                "\"model\":{\"type\":\"string\",\"description\":\"Model name\"},"
                "\"language\":{\"type\":\"string\",\"description\":\"Language code (ISO 639-1)\"}"
            "},"
            "\"required\":[\"api_key\"]"
        "}");
    }

    if (strcmp(provider_id, "local") == 0) {
        return strdup("{"
            "\"type\":\"object\","
            "\"properties\":{"
                "\"model\":{\"type\":\"string\",\"description\":\"Whisper model name\"},"
                "\"language\":{\"type\":\"string\",\"description\":\"Language code\"},"
                "\"threads\":{\"type\":\"integer\",\"description\":\"Number of threads\"}"
            "},"
            "\"required\":[\"model\"]"
        "}");
    }

    return strdup("{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"api_key\":{\"type\":\"string\",\"description\":\"API key\"}"
        "},"
        "\"required\":[\"api_key\"]"
    "}");
}

