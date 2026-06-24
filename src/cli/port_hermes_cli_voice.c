/*
 * port_hermes_cli_voice.c — C port of hermes_cli/voice.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* PoP: cli_hermes_cli_voice__beeps_enabled @ hermes_cli/voice.py:_beeps_enabled */

/* Port of Python hermes_cli/voice.py:_beeps_enabled */
/* Checks if voice beeps are enabled in config. */
int cli_hermes_cli_voice__beeps_enabled(void)
{
    /* CLI port: default to enabled. */
    return 1;
}

/* PoP: cli_hermes_cli_voice__play_beep @ hermes_cli/voice.py:_play_beep */

/* Port of Python hermes_cli/voice.py:_play_beep */
/* Plays an audible beep. */
void cli_hermes_cli_voice__play_beep(int frequency, int count)
{
    (void)frequency;
    (void)count;
    /* CLI port: beep playback requires sounddevice. */
}

/* PoP: cli_hermes_cli_voice_start_recording @ hermes_cli/voice.py:start_recording */

/* Port of Python hermes_cli/voice.py:start_recording */
/* Begins capturing from the default input device (push-to-talk). */
void cli_hermes_cli_voice_start_recording(void)
{
    hermes_log(LOG_DEBUG, "voice", "start_recording: CLI port — no-op");
}

/* PoP: cli_hermes_cli_voice_stop_and_transcribe @ hermes_cli/voice.py:stop_and_transcribe */

/* Port of Python hermes_cli/voice.py:stop_and_transcribe */
/* Stops recording and transcribes. Returns transcript or NULL. */
char *cli_hermes_cli_voice_stop_and_transcribe(void)
{
    hermes_log(LOG_DEBUG, "voice", "stop_and_transcribe: CLI port — no-op");
    return NULL;
}

/* PoP: cli_hermes_cli_voice_start_continuous @ hermes_cli/voice.py:start_continuous */

/* Port of Python hermes_cli/voice.py:start_continuous */
/* Starts a VAD-driven continuous recording loop. */
int cli_hermes_cli_voice_start_continuous(void)
{
    hermes_log(LOG_DEBUG, "voice", "start_continuous: CLI port — no-op");
    return 0;
}

/* PoP: cli_hermes_cli_voice_stop_continuous @ hermes_cli/voice.py:stop_continuous */

/* Port of Python hermes_cli/voice.py:stop_continuous */
/* Stops the active continuous loop. */
void cli_hermes_cli_voice_stop_continuous(void)
{
    hermes_log(LOG_DEBUG, "voice", "stop_continuous: CLI port — no-op");
}
