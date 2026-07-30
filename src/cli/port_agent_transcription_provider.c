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
