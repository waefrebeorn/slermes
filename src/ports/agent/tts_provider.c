/*
 * ports/agent/tts_provider.c — C11 port of agent/tts_provider.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_tts_provider_ports.c ── */
/*
 * port_tts_provider_remaining.c — Port of agent/tts_provider.py provider
 * protocol surface. Identity, availability, model catalog, setup schema,
 * output format clamping.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: ttp_name @ agent/tts_provider.py:name */
char *ttp_name(void) {
    return strdup("tts");
}

/* PoP: ttp_display_name @ agent/tts_provider.py:display_name */
char *ttp_display_name(void) {
    return strdup("TTS");
}

/* PoP: ttp_is_available @ agent/tts_provider.py:is_available */
bool ttp_is_available(void) {
    /* Python: any registered TTS provider key present. Delegates to the
     * real TTS registry. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs) return false;
    bool avail = provs[0] != '\0' && strcmp(provs, "[]") != 0;
    free(provs);
    return avail;
}

/* PoP: ttp_list_models @ agent/tts_provider.py:list_models */
char *ttp_list_models(void) {
    /* Python: catalog entries for the picker. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs) return strdup("[]");
    /* Wrap in a models array: [{id: <provider>, name: <provider>}]. */
    json_t *prov_json = json_parse(provs, NULL);
    free(provs);
    json_t *arr = json_array();
    if (prov_json) {
        if (prov_json->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(prov_json); i++) {
                json_t *item = json_get(prov_json, i);
                const char *id = (item && item->type == JSON_STRING) ? item->str_val : NULL;
                if (id) {
                    json_t *entry = json_object();
                    json_set(entry, "id", json_string(id));
                    json_set(entry, "name", json_string(id));
                    json_append(arr, entry);
                }
            }
        }
        json_free(prov_json);
    }
    char *ser = json_serialize(arr);
    json_free(arr);
    return ser ? ser : strdup("[]");
}

/* PoP: ttp_get_setup_schema @ agent/tts_provider.py:get_setup_schema */
char *ttp_get_setup_schema(void) {
    return strdup("{\"fields\": [{\"key\": \"api_key\", \"label\": \"API Key\", "
                  "\"type\": \"password\"}]}");
}

/* PoP: ttp_default_model @ agent/tts_provider.py:default_model */
char *ttp_default_model(void) {
    /* Python: first provider id or None. */
    extern char *tts_list_providers(void);
    char *provs = tts_list_providers();
    if (!provs || !*provs) { free(provs); return NULL; }
    json_t *prov_json = json_parse(provs, NULL);
    free(provs);
    if (!prov_json) return NULL;
    const char *id = NULL;
    if (prov_json->type == JSON_ARRAY && json_len(prov_json) > 0) {
        json_t *first = json_get(prov_json, 0);
        id = (first && first->type == JSON_STRING) ? first->str_val : NULL;
    }
    char *out = id ? strdup(id) : NULL;
    json_free(prov_json);
    return out;
}

/* PoP: ttp_voice_compatible @ agent/tts_provider.py:voice_compatible */
bool ttp_voice_compatible(void) {
    /* Python: mirrors tts.prefer_voice_bubble. */
    return false;
}

/* PoP: ttp_resolve_output_format @ agent/tts_provider.py:resolve_output_format */
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


/* ── from src/cli/port_agent_tts_provider_methods.c ── */
/*
 * port_agent_tts_provider.c — C port of agent/tts_provider.py
 *
 * Text-to-Speech Provider ABC - additional concrete methods.
 */

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


/* Port of Python agent/tts_provider.py:voice_compatible */
/* Whether output is suitable for voice-bubble delivery. Default: 0 (false). */


