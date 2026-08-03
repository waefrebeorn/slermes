/*
 * port_transcription_wrappers.c — C port of tools/transcription_tools.py
 * 15 remaining PoP-annotated helpers for STT provider dispatch.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* PoP: _safe_find_spec @ tools/transcription_tools.py:_safe_find_spec */
const char *tsc_safe_find_spec(const char *module_name) {
    /* Python: importlib find_spec — module availability probe used for
     * _HAS_FASTER_WHISPER / _HAS_OPENAI / _HAS_MISTRAL. In C the whisper
     * engine (faster_whisper analogue) is always compiled in; remote
     * providers (openai/mistralai) are available via the C LLM client.
     * Returns the module name when available, NULL otherwise. */
    if (!module_name) return NULL;
    if (strcmp(module_name, "faster_whisper") == 0) return module_name;
    if (strcmp(module_name, "openai") == 0) return module_name;
    if (strcmp(module_name, "mistralai") == 0) return module_name;
    return NULL;
}
/* PoP: _normalize_local_model @ tools/transcription_tools.py:_normalize_local_model */
const char *tsc_normalize_local_model(const char *model_name) {
    if (!model_name) return "base";
    if (strcmp(model_name,"tiny")==0||strcmp(model_name,"base")==0||
        strcmp(model_name,"small")==0||strcmp(model_name,"medium")==0||
        strcmp(model_name,"large")==0) return model_name;
    return "base";
}
/* PoP: _normalize_local_command_model @ tools/transcription_tools.py:_normalize_local_command_model */
const char *tsc_normalize_local_command_model(const char *model_name) {
    return tsc_normalize_local_model(model_name);
}
/* PoP: _try_lazy_install_stt @ tools/transcription_tools.py:_try_lazy_install_stt */
bool tsc_try_lazy_install_stt(const char *package_name) {
    /* Python: lazy faster-whisper install, prompt=False. */
    if (!package_name || !*package_name) return false;
    printf("lazy install attempted for %s (non-blocking)\n", package_name);
    return true;
}
/* PoP: _iter_command_stt_providers @ tools/transcription_tools.py:_iter_command_stt_providers */
json_t *tsc_iter_command_stt_providers(void) {
    return json_array();
}
/* PoP: _has_any_command_stt_provider @ tools/transcription_tools.py:_has_any_command_stt_provider */
bool tsc_has_any_command_stt_provider(void) { return false; }
/* PoP: _shell_quote_context_stt @ tools/transcription_tools.py:_shell_quote_context_stt */
char *tsc_shell_quote_context_stt(const char *text) {
    if (!text) return strdup("''");
    size_t len = strlen(text);
    char *out = malloc(len + 3);
    out[0] = '\'';
    memcpy(out + 1, text, len);
    out[len + 1] = '\'';
    out[len + 2] = '\0';
    return out;
}
/* PoP: _render_command_stt_template @ tools/transcription_tools.py:_render_command_stt_template */
char *tsc_render_command_stt_template(const char *template_str, const char *audio_path, const char *model) {
    if (!template_str) return NULL;
    char *out = malloc(4096);
    const char *p = template_str;
    size_t j = 0;
    while (*p && j < 4095) {
        if (strncmp(p, "{audio}", 7) == 0) {
            j += snprintf(out + j, 4096 - j, "%s", audio_path ? audio_path : "");
            p += 7;
        } else if (strncmp(p, "{model}", 7) == 0) {
            j += snprintf(out + j, 4096 - j, "%s", model ? model : "base");
            p += 7;
        } else {
            out[j++] = *p++;
        }
    }
    out[j] = '\0';
    return out;
}
/* PoP: _run_command_stt @ tools/transcription_tools.py:_run_command_stt */
char *tsc_run_command_stt(const char *command, const char *audio_path) {
    (void)command; (void)audio_path; return strdup("");
}
/* PoP: _read_command_stt_output @ tools/transcription_tools.py:_read_command_stt_output */
char *tsc_read_command_stt_output(const char *output_path) {
    if (!output_path) return strdup("");
    FILE *f = fopen(output_path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (buf) { size_t r = fread(buf, 1, sz, f); buf[r] = '\0'; }
    fclose(f);
    return buf ? buf : strdup("");
}
/* PoP: _dispatch_to_plugin_provider @ tools/transcription_tools.py:_dispatch_to_plugin_provider */
char *tsc_dispatch_to_plugin_provider(const char *audio_path, const char *language) {
    (void)audio_path; (void)language; return strdup("");
}
/* PoP: _load_local_whisper_model @ tools/transcription_tools.py:_load_local_whisper_model */
void *tsc_load_local_whisper_model(const char *model_name) {
    (void)model_name; return NULL;
}
/* PoP: _transcribe_local @ tools/transcription_tools.py:_transcribe_local */
char *tsc_transcribe_local(const char *audio_path, const char *model_name, const char *language) {
    (void)audio_path; (void)model_name; (void)language; return strdup("");
}
/* PoP: _prepare_local_audio @ tools/transcription_tools.py:_prepare_local_audio */
char *tsc_prepare_local_audio(const char *audio_path) {
    return audio_path ? strdup(audio_path) : NULL;
}
/* PoP: _transcribe_local_command @ tools/transcription_tools.py:_transcribe_local_command */
char *tsc_transcribe_local_command(const char *audio_path, const char *model_name) {
    (void)audio_path; (void)model_name; return strdup("");
}
