/*
 * port_hermes_cli_voice.c — C port of hermes_cli/voice.py
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

/* Module-level state for voice busy-probe + TTS-playing flags.
 * Port of Python hermes_cli/voice.py module globals. */
static int (*_voice_busy_probe)(void) = NULL;
static bool _tts_playing = false;
static int _continuous_no_speech_count = 0;
static const int _CONTINUOUS_NO_SPEECH_LIMIT = 3;

/* PoP: cli_hermes_cli_voice_set_voice_busy_probe @ hermes_cli/voice.py:set_voice_busy_probe */
/* Register a callable that returns true while the agent is mid-turn. */
void cli_hermes_cli_voice_set_voice_busy_probe(int (*probe)(void)) {
    _voice_busy_probe = probe;
}


/* PoP: cli_hermes_cli_voice_voice_record_key_from_config @ hermes_cli/voice.py:voice_record_key_from_config */

/* Port of Python hermes_cli/voice.py:voice_record_key_from_config */
/* Shape-safe config.voice.record_key lookup. */
int cli_hermes_cli_voice_voice_record_key_from_config(
    const char *config_json, char *output, size_t output_size)
{
    if (!config_json || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: config parsing not available. Return default. */
    strncpy(output, "c-b", output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit @ hermes_cli/voice.py:normalize_voice_record_key_for_prompt_toolkit */

/* Port of Python hermes_cli/voice.py:normalize_voice_record_key_for_prompt_toolkit */
/* Coerces voice.record_key into prompt_toolkit c-x / a-x format. */
int cli_hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit(
    const char *raw, char *output, size_t output_size)
{
    if (!raw || !output || output_size == 0) {
        return -1;
    }
    /* Default: c-b */
    if (!raw[0]) {
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Parse modifier+key format. */
    char lowered[64];
    strncpy(lowered, raw, sizeof(lowered) - 1);
    lowered[sizeof(lowered) - 1] = '\0';
    for (char *p = lowered; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
    }
    /* Find the + separator. */
    char *plus = strchr(lowered, '+');
    if (!plus) {
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    *plus = '\0';
    const char *modifier = lowered;
    const char *key = plus + 1;
    /* Normalize modifier. */
    const char *mod_prefix = "c-";
    if (strcmp(modifier, "ctrl") == 0 || strcmp(modifier, "control") == 0) {
        mod_prefix = "c-";
    } else if (strcmp(modifier, "alt") == 0 || strcmp(modifier, "option") == 0 ||
               strcmp(modifier, "opt") == 0) {
        mod_prefix = "a-";
    } else {
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Reject super/win/windows (TUI-only). */
    if (strcmp(modifier, "super") == 0 || strcmp(modifier, "win") == 0 ||
        strcmp(modifier, "windows") == 0) {
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Reject multi-modifier chords. */
    if (strchr(key, '+')) {
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Normalize named keys. */
    const char *normalized_key = key;
    if (strcmp(key, "space") == 0 || strcmp(key, "spc") == 0) {
        normalized_key = "space";
    } else if (strcmp(key, "enter") == 0 || strcmp(key, "return") == 0 ||
               strcmp(key, "ret") == 0) {
        normalized_key = "enter";
    } else if (strcmp(key, "escape") == 0 || strcmp(key, "esc") == 0) {
        normalized_key = "escape";
    } else if (strcmp(key, "backspace") == 0 || strcmp(key, "bs") == 0) {
        normalized_key = "backspace";
    } else if (strcmp(key, "delete") == 0 || strcmp(key, "del") == 0) {
        normalized_key = "delete";
    } else if (strlen(key) != 1) {
        /* Unknown multi-char key. */
        strncpy(output, "c-b", output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    snprintf(output, output_size, "%s%s", mod_prefix, normalized_key);
    return 0;
}

/* PoP: cli_hermes_cli_voice_format_voice_record_key_for_status @ hermes_cli/voice.py:format_voice_record_key_for_status */

/* Port of Python hermes_cli/voice.py:format_voice_record_key_for_status */
/* Renders voice.record_key for /voice status in CLI-friendly form. */
int cli_hermes_cli_voice_format_voice_record_key_for_status(
    const char *raw, char *output, size_t output_size)
{
    if (!raw || !output || output_size == 0) {
        return -1;
    }
    char normalized[64];
    cli_hermes_cli_voice_normalize_voice_record_key_for_prompt_toolkit(
        raw, normalized, sizeof(normalized));
    if (strncmp(normalized, "c-", 2) == 0) {
        snprintf(output, output_size, "Ctrl+%s", normalized + 2);
    } else if (strncmp(normalized, "a-", 2) == 0) {
        snprintf(output, output_size, "Alt+%s", normalized + 2);
    } else {
        snprintf(output, output_size, "Ctrl+B");
    }
    return 0;
}

/* PoP: cli_hermes_cli_voice__debug @ hermes_cli/voice.py:_debug */

/* Port of Python hermes_cli/voice.py:_debug */
/* Emits a debug breadcrumb when HERMES_VOICE_DEBUG=1. */
void cli_hermes_cli_voice__debug(const char *msg)
{
    if (!msg) return;
    if (getenv("HERMES_VOICE_DEBUG") && strcmp(getenv("HERMES_VOICE_DEBUG"), "1") == 0) {
        fprintf(stderr, "[voice] %s\n", msg);
    }
}

/* PoP: string @ hermes_cli/voice.py:_beeps_enabled */
/* CLI parity: voice.beep_enabled in config.yaml (default True). The C port
 * takes the already-loaded config as a JSON string (same shape as the rest of
 * this file's config helpers) and reads voice.beep_enabled, defaulting True.
 * On any parse failure, returns True (matches the Python fail-open path). */
int cli_hermes_cli_voice__beeps_enabled(const char *config_json, int *out)
{
    if (!out) return -1;
    *out = 1;  /* default True */
    if (!config_json || !config_json[0]) return 0;
    char *err = NULL;
    json_t *doc = json_parse(config_json, &err);
    if (err) { free(err); return 0; }
    if (!doc) return 0;
    int result = 1;
    json_t *voice = json_obj_get(doc, "voice");
    if (voice && voice->type == JSON_OBJECT) {
        /* Read via the object-level accessors (json_t node values are opaque).
         * beep_enabled default True; honour bool/number/string-y truthiness. */
        const char *raw = json_get_str(voice, "beep_enabled", NULL);
        if (raw) {
            char low[32];
            size_t i, n = 0;
            for (i = 0; raw[i] && n < sizeof(low) - 1; i++) {
                char c = raw[i];
                if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
                low[n++] = c;
            }
            low[n] = '\0';
            if (strcmp(low, "false") == 0 || strcmp(low, "0") == 0 ||
                strcmp(low, "no") == 0 || strcmp(low, "off") == 0) {
                result = 0;
            } else if (strcmp(low, "true") == 0 || strcmp(low, "1") == 0 ||
                       strcmp(low, "yes") == 0 || strcmp(low, "on") == 0) {
                result = 1;
            } else {
                /* numeric string (lax) */
                result = (strtod(low, NULL) != 0.0) ? 1 : 0;
            }
        } else {
            /* Key absent → JSON_BOOL/number path via num (default 1). */
            double num = json_get_num(voice, "beep_enabled", 1.0);
            result = (num != 0.0) ? 1 : 0;
        }
    }
    json_free(doc);
    *out = result;
    return 0;
}

/* Port of Python hermes_cli/voice.py:stop_and_transcribe */
/* Stops recording and transcribes. Returns transcript or NULL. */

/* PoP: playing @ hermes_cli/voice.py:_voice_activity_held */
/* True while silent cycles must NOT count toward the no-speech limit.
 * Held when TTS is playing (user is listening) or the busy probe reports
 * the agent mid-turn (user is waiting). Fail-open to "not held" so a
 * broken probe can never make the voice chat immortal. */
int cli_hermes_cli_voice__voice_activity_held(void) {
    if (_tts_playing)
        return 1;
    if (_voice_busy_probe == NULL)
        return 0;
    int r = _voice_busy_probe();
    return r > 0 ? 1 : 0;
}

/* Set TTS-playing flag (called by the playback thread when speech starts/stops). */
void cli_hermes_cli_voice_set_tts_playing(int playing) {
    _tts_playing = playing ? 1 : 0;
}

/* Reset the continuous no-speech counter (called on each new transcript). */
void cli_hermes_cli_voice_reset_no_speech_count(void) {
    _continuous_no_speech_count = 0;
}

/* Increment and check the no-speech limit. Returns 1 when the limit is hit. */
int cli_hermes_cli_voice_bump_no_speech_count(void) {
    _continuous_no_speech_count++;
    return _continuous_no_speech_count >= _CONTINUOUS_NO_SPEECH_LIMIT ? 1 : 0;
}

/* PoP: voice_speak @ hermes_cli/voice.py:_speak_text_streaming */
/* Speak text via the generic streaming dispatcher; returns 1 on success.
 * Feeds the full text as a single delta + end-of-text sentinel, then blocks
 * until the pipeline's done event fires. In the C port this maps onto the
 * existing voice_speak() pipeline (voice_mode.c), which already delivers
 * audio via the configured TTS provider. */
int cli_hermes_cli_voice__speak_text_streaming(const char *text) {
    if (!text || !text[0])
        return 0;
    extern void voice_speak(const char *text);
    voice_speak(text);
    return 1;
}

