/*
 * port_agent_tts_provider.c — C port of agent/tts_provider.py
 *
 * Text-to-Speech Provider ABC - additional concrete methods.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: tts_provider_list_voices @ agent/tts_provider.py:list_voices */

/* Port of Python agent/tts_provider.py:list_voices */
/* Return voice catalog entries. Returns count, fills arrays. */
int tts_provider_list_voices(char **voice_ids, char **voice_names, int *languages, int max_voices)
{
    if (!voice_ids || max_voices <= 0) return 0;

    /* Base provider has no concrete voices; concrete providers override this.
     * Faithful to Python base class which returns [] (empty catalog). */
    hermes_log(LOG_DEBUG, "tts_provider", "Listed 0 voices (abstract base provider)");
    return 0;
}

/* PoP: tts_provider_default_voice @ agent/tts_provider.py:default_voice */

/* Port of Python agent/tts_provider.py:default_voice */
/* Return the default voice id, or empty string if not applicable. */
char *tts_provider_default_voice(void)
{
    char *voices[16];
    char *names[16];
    int langs[16];
    int count = tts_provider_list_voices(voices, names, langs, 16);

    char *result = NULL;
    if (count > 0) {
        result = strdup(voices[0]);
    } else {
        result = strdup("");
    }

    /* Cleanup */
    for (int i = 0; i < count; i++) {
        free(voices[i]);
        free(names[i]);
    }

    hermes_log(LOG_DEBUG, "tts_provider", "Default voice: %s", result);
    return result;
}

/* PoP: tts_provider_synthesize @ agent/tts_provider.py:synthesize */

/* Port of Python agent/tts_provider.py:synthesize */
/* Synthesize text and write audio bytes to output_path. Returns output_path on success. */
char *tts_provider_synthesize(const char *text, const char *output_path,
                                const char *voice, const char *model, float speed,
                                const char *format)
{
    if (!text || !text[0] || !output_path || !output_path[0]) {
        hermes_log(LOG_ERROR, "tts_provider", "synthesize: invalid args");
        return NULL;
    }

    hermes_log(LOG_INFO, "tts_provider", "Synthesizing: text='%s' voice=%s format=%s",
               text, voice ? voice : "(default)", format ? format : "mp3");

    /* The base provider is abstract: synthesize() raises NotImplementedError in
     * Python. Concrete providers (OpenAI, ElevenLabs, ...) override this to call
     * their real TTS API. Return NULL rather than emit a fake audio file. */
    hermes_log(LOG_WARNING, "tts_provider",
               "synthesize() not implemented by abstract base provider; "
               "use a concrete TTS provider (e.g. OpenAI TTS)");
    return NULL;
}

/* PoP: tts_provider_stream @ agent/tts_provider.py:stream */

/* Port of Python agent/tts_provider.py:stream */
/* Stream synthesized audio bytes. Returns first chunk or NULL if not supported. */
char *tts_provider_stream(const char *text, const char *voice, const char *model,
                            const char *format)
{
    if (!text || !text[0]) {
        hermes_log(LOG_ERROR, "tts_provider", "stream: empty text");
        return NULL;
    }

    hermes_log(LOG_INFO, "tts_provider", "Streaming: text='%s' voice=%s format=%s",
               text, voice ? voice : "(default)", format ? format : "opus");

    /* Abstract base: stream() raises NotImplementedError in Python. Return NULL
     * (no fake chunk) rather than a placeholder. */
    hermes_log(LOG_WARNING, "tts_provider",
               "stream() not implemented by abstract base provider; "
               "use a concrete TTS provider");
    return NULL;
}

/* PoP: tts_provider_voice_compatible @ agent/tts_provider.py:voice_compatible */

/* Port of Python agent/tts_provider.py:voice_compatible */
/* Whether output is suitable for voice-bubble delivery. Default: 0 (false). */
int tts_provider_voice_compatible(void)
{
    /* Default: not voice-compatible (safe default) */
    /* Providers that support Opus output should override this */
    hermes_log(LOG_DEBUG, "tts_provider", "voice_compatible: 0 (default)");
    return 0;
}
