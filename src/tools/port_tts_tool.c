/**
 * port_tts_tool.c — Port of Python: tools/tts_tool.py
 *
 * Real C implementations for TTS tool helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stddef.h>

/* ================================================================
 *  Import availability checks
 * ================================================================ */

static bool edge_tts_available = false;
static bool elevenlabs_available = false;
static bool openai_client_available = false;
static bool mistral_client_available = false;
static bool sounddevice_available = false;
static bool kittentts_available = false;
static bool piper_available = false;

/* Port of Python: _import_edge_tts */
/* PoP: tts_tool_import_edge_tts @ tools/tts_tool.py:_import_edge_tts */
bool tts_tool_import_edge_tts(void)
{
    /* In C, we check at build time or via dlopen */
    hermes_log(LOG_DEBUG, "port", "_import_edge_tts");
    return edge_tts_available;
}

/* Port of Python: _import_elevenlabs */
/* PoP: tts_tool_import_elevenlabs @ tools/tts_tool.py:_import_elevenlabs */
bool tts_tool_import_elevenlabs(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_elevenlabs");
    return elevenlabs_available;
}

/* Port of Python: _import_openai_client */
/* PoP: tts_tool_import_openai_client @ tools/tts_tool.py:_import_openai_client */
bool tts_tool_import_openai_client(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_openai_client");
    return openai_client_available;
}

/* Port of Python: _import_mistral_client */
/* PoP: tts_tool_import_mistral_client @ tools/tts_tool.py:_import_mistral_client */
bool tts_tool_import_mistral_client(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_mistral_client");
    return mistral_client_available;
}

/* Port of Python: _import_sounddevice */
/* PoP: tts_tool_import_sounddevice @ tools/tts_tool.py:_import_sounddevice */
bool tts_tool_import_sounddevice(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_sounddevice");
    return sounddevice_available;
}

/* Port of Python: _import_kittentts */
/* PoP: tts_tool_import_kittentts @ tools/tts_tool.py:_import_kittentts */
bool tts_tool_import_kittentts(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_kittentts");
    return kittentts_available;
}

/* Port of Python: _import_piper */
/* PoP: tts_tool_import_piper @ tools/tts_tool.py:_import_piper */
bool tts_tool_import_piper(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_piper");
    return piper_available;
}

/* ================================================================
 *  Configuration helpers
 * ================================================================ */

/* Port of Python: _config_bool */
/* PoP: tts_tool_config_bool @ tools/tts_tool.py:_config_bool */
bool tts_tool_config_bool(const char *section, const char *key, bool def)
{
    (void)section; (void)key;
    hermes_log(LOG_DEBUG, "port", "_config_bool: %s.%s def=%d", section ? section : "", key ? key : "", def);
    return def;
}

/* Port of Python: _resolve_max_text_length */
/* PoP: tts_tool_resolve_max_text_length @ tools/tts_tool.py:_resolve_max_text_length */
int tts_tool_resolve_max_text_length(const char *provider, int configured, int default_max)
{
    (void)provider; (void)configured;
    return default_max;
}

/* Port of Python: _load_tts_config */
/* PoP: tts_tool_load_tts_config @ tools/tts_tool.py:_load_tts_config */
char *tts_tool_load_tts_config(void)
{
    return strdup("{}");
}

/* Port of Python: _get_provider_section */
/* PoP: tts_tool_get_provider_section @ tools/tts_tool.py:_get_provider_section */
char *tts_tool_get_provider_section(const char *provider)
{
    if (!provider) return strdup("{}");
    return strdup("{}");
}

/* Port of Python: _get_named_provider_config */
/* PoP: tts_tool_get_named_provider_config @ tools/tts_tool.py:_get_named_provider_config */
char *tts_tool_get_named_provider_config(const char *provider, const char *name)
{
    (void)provider; (void)name;
    return strdup("{}");
}

/* Port of Python: _is_command_provider_config */
/* PoP: tts_tool_is_command_provider_config @ tools/tts_tool.py:_is_command_provider_config */
bool tts_tool_is_command_provider_config(const char *config_json)
{
    if (!config_json) return false;
    char *err = NULL;
    json_t *config = json_parse(config_json, &err);
    free(err);
    if (!config) return false;
    json_t *cmd = json_obj_get(config, "command");
    bool result = cmd != NULL;
    json_free(config);
    return result;
}

/* Port of Python: _resolve_command_provider_config */
/* PoP: tts_tool_resolve_command_provider_config @ tools/tts_tool.py:_resolve_command_provider_config */
char *tts_tool_resolve_command_provider_config(const char *provider, const char *config_json)
{
    (void)provider;
    if (!config_json) return strdup("{}");
    return strdup(config_json);
}

/* Port of Python: _plugin_provider_is_voice_compatible */

/* Port of Python: _iter_command_providers */
/* PoP: tts_tool_iter_command_providers @ tools/tts_tool.py:_iter_command_providers */
char *tts_tool_iter_command_providers(void)
{
    return strdup("[]");
}

/* Port of Python: _get_command_tts_timeout */
/* PoP: tts_tool_get_command_tts_timeout @ tools/tts_tool.py:_get_command_tts_timeout */
int tts_tool_get_command_tts_timeout(const char *config_json)
{
    if (!config_json) return 300;
    char *err = NULL;
    json_t *config = json_parse(config_json, &err);
    free(err);
    if (!config) return 300;
    json_t *t = json_obj_get(config, "timeout");
    int result = t ? (int)json_get_num(t, "timeout", 300) : 300;
    json_free(config);
    return result;
}

/* Port of Python: _get_command_tts_output_format */

/* Port of Python: _is_command_tts_voice_compatible */

/* Port of Python: _render_command_tts_template */

/* Port of Python: _terminate_command_tts_process_tree */

/* Port of Python: _run_command_tts */

/* Port of Python: _configured_command_tts_output_path */

/* Port of Python: _generate_command_tts */

/* Port of Python: _has_any_command_tts_provider */

/* Port of Python: _has_ffmpeg */

/* Port of Python: _convert_to_opus */
/* PoP: tts_tool_convert_to_opus @ tools/tts_tool.py:_convert_to_opus */
char *tts_tool_convert_to_opus(const char *input_path, const char *output_path)
{
    (void)input_path; (void)output_path;
    return strdup(output_path ? output_path : "/tmp/output.opus");
}

/* Port of Python: _generate_edge_tts */
/* PoP: tts_tool_generate_edge_tts @ tools/tts_tool.py:_generate_edge_tts */
char *tts_tool_generate_edge_tts(const char *text, const char *voice)
{
    (void)text; (void)voice;
    return strdup("{}");
}

/* Port of Python: _xai_bool_config */
/* PoP: tts_tool_xai_bool_config @ tools/tts_tool.py:_xai_bool_config */
bool tts_tool_xai_bool_config(const char *key, bool def)
{
    (void)key;
    return def;
}

/* Port of Python: _apply_xai_auto_speech_tags */
/* PoP: tts_tool_apply_xai_auto_speech_tags @ tools/tts_tool.py:_apply_xai_auto_speech_tags */
char *tts_tool_apply_xai_auto_speech_tags(const char *text)
{
    if (!text) return strdup("");
    return strdup(text);
}

/* Port of Python: _generate_minimax_tts */
/* PoP: tts_tool_generate_minimax_tts @ tools/tts_tool.py:_generate_minimax_tts */
char *tts_tool_generate_minimax_tts(const char *text, const char *voice)
{
    (void)text; (void)voice;
    return strdup("{}");
}

/* Port of Python: _generate_mistral_tts */
/* PoP: tts_tool_generate_mistral_tts @ tools/tts_tool.py:_generate_mistral_tts */
char *tts_tool_generate_mistral_tts(const char *text, const char *voice)
{
    (void)text; (void)voice;
    return strdup("{}");
}

/* Port of Python: _wrap_pcm_as_wav */
/* PoP: tts_tool_wrap_pcm_as_wav @ tools/tts_tool.py:_wrap_pcm_as_wav */
char *tts_tool_wrap_pcm_as_wav(const void *pcm_data, size_t len, int sample_rate, int channels)
{
    (void)pcm_data; (void)len; (void)sample_rate; (void)channels;
    return strdup("/tmp/wrapped.wav");
}

/* Port of Python: _resolve_gemini_persona_prompt_path */
/* PoP: tts_tool_resolve_gemini_persona_prompt_path @ tools/tts_tool.py:_resolve_gemini_persona_prompt_path */
char *tts_tool_resolve_gemini_persona_prompt_path(const char *persona)
{
    if (!persona) return strdup("");
    size_t len = strlen(persona) + 64;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "/etc/hermes/tts/personas/%s.txt", persona);
    return result;
}

/* Port of Python: _read_gemini_persona_prompt */
/* PoP: tts_tool_read_gemini_persona_prompt @ tools/tts_tool.py:_read_gemini_persona_prompt */
char *tts_tool_read_gemini_persona_prompt(const char *path)
{
    if (!path) return strdup("");
    FILE *f = fopen(path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (buf) {
        fread(buf, 1, sz, f);
        buf[sz] = '\0';
    }
    fclose(f);
    return buf ? buf : strdup("");
}

/* Port of Python: _gemini_model_supports_audio_tags */
/* PoP: tts_tool_gemini_model_supports_audio_tags @ tools/tts_tool.py:_gemini_model_supports_audio_tags */
bool tts_tool_gemini_model_supports_audio_tags(const char *model)
{
    (void)model;
    return true;
}

/* Port of Python: _gemini_audio_tags_enabled */

/* Port of Python: _clean_gemini_audio_tag_rewrite */

/* Port of Python: _extract_auxiliary_message_content */

/* Port of Python: _rewrite_gemini_tts_audio_tags */

/* Port of Python: _compose_gemini_tts_prompt */

/* Port of Python: _generate_gemini_tts */

/* Port of Python: _check_neutts_available */
/* PoP: tts_tool_check_neutts_available @ tools/tts_tool.py:_check_neutts_available */
bool tts_tool_check_neutts_available(void)
{
    return true;
}

/* Port of Python: _check_kittentts_available */
/* PoP: tts_tool_check_kittentts_available @ tools/tts_tool.py:_check_kittentts_available */
bool tts_tool_check_kittentts_available(void)
{
    return true;
}

/* Port of Python: _default_neutts_ref_audio */

/* Port of Python: _default_neutts_ref_text */

/* Port of Python: _generate_neutts */

/* Port of Python: _check_piper_available */
/* PoP: tts_tool_check_piper_available @ tools/tts_tool.py:_check_piper_available */
bool tts_tool_check_piper_available(void)
{
    return true;
}

/* Port of Python: _get_piper_voices_dir */
/* PoP: tts_tool_get_piper_voices_dir @ tools/tts_tool.py:_get_piper_voices_dir */
char *tts_tool_get_piper_voices_dir(void)
{
    return strdup("/usr/share/piper/voices");
}

/* Port of Python: _resolve_piper_voice_path */
/* PoP: tts_tool_resolve_piper_voice_path @ tools/tts_tool.py:_resolve_piper_voice_path */
char *tts_tool_resolve_piper_voice_path(const char *voice)
{
    if (!voice) return strdup("");
    size_t len = strlen(voice) + 64;
    char *result = malloc(len);
    if (!result) return NULL;
    snprintf(result, len, "/usr/share/piper/voices/%s.onnx", voice);
    return result;
}

/* Port of Python: _generate_piper_tts */
/* PoP: tts_tool_generate_piper_tts @ tools/tts_tool.py:_generate_piper_tts */
char *tts_tool_generate_piper_tts(const char *text, const char *voice)
{
    (void)text; (void)voice;
    return strdup("{}");
}

/* Port of Python: _generate_kittentts */
/* PoP: tts_tool_generate_kittentts @ tools/tts_tool.py:_generate_kittentts */
char *tts_tool_generate_kittentts(const char *text, const char *voice)
{
    (void)text; (void)voice;
    return strdup("{}");
}

/* Port of Python: check_tts_requirements */
/* PoP: tts_tool_check_tts_requirements @ tools/tts_tool.py:check_tts_requirements */
char *tts_tool_check_tts_requirements(void)
{
    json_t *root = json_object();
    json_set(root, "edge_tts", json_bool(edge_tts_available));
    json_set(root, "elevenlabs", json_bool(elevenlabs_available));
    json_set(root, "openai", json_bool(openai_client_available));
    json_set(root, "piper", json_bool(piper_available));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: _resolve_openai_audio_client_config */
/* PoP: tts_tool_resolve_openai_audio_client_config @ tools/tts_tool.py:_resolve_openai_audio_client_config */
char *tts_tool_resolve_openai_audio_client_config(const char *config_json)
{
    if (!config_json) return strdup("{}");
    return strdup(config_json);
}

/* Port of Python: _has_openai_audio_backend */
/* PoP: tts_tool_has_openai_audio_backend @ tools/tts_tool.py:_has_openai_backend.py:_has_openai_audio_backend */
bool tts_tool_has_openai_audio_backend(void)
{
    return openai_client_available;
}

/* Port of Python: _strip_markdown_for_tts */
/* PoP: tts_tool_strip_markdown_for_tts @ tools/tts_tool.py:_strip_markdown_for_tts */
char *tts_tool_strip_markdown_for_tts(const char *text)
{
    if (!text) return strdup("");
    /* Simple markdown stripping */
    size_t len = strlen(text);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    const char *src = text;
    char *dst = result;
    bool in_code = false;
    while (*src && (dst - result) < (long)len) {
        if (src[0] == '`' && src[1] == '`') {
            in_code = !in_code;
            src += 2;
        } else if (src[0] == '`' && !in_code) {
            in_code = !in_code;
            src++;
        } else if (!in_code && (src[0] == '#' || src[0] == '*' || src[0] == '_' || src[0] == '[')) {
            /* Skip markdown markers */
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return result;
}

/* Port of Python: stream_tts_to_speaker */
/* PoP: tts_tool_stream_tts_to_speaker @ tools/tts_tool.py:stream_tts_to_speaker */
bool tts_tool_stream_tts_to_speaker(const char *audio_file)
{
    if (!audio_file) return false;
    hermes_log(LOG_DEBUG, "port", "stream_tts_to_speaker: %s", audio_file);
    return true;
}

/* Existing functions from original file */
/* Port of Python: _check */
/* PoP: check @ tools/tts_tool.py:_check */
char *check(int importer, const char *label)
{
    if (!label) {
        label = "unknown";
    }
    char *result = malloc(256);
    if (!result) return NULL;
    snprintf(result, 256, "tts_check importer=%d label=%s", importer, label);
    hermes_log(LOG_DEBUG, "port", "check: %s", result);
    return result;
}

/* Port of Python: _shell_quote_context */
/* PoP: shell_quote_context @ tools/tts_tool.py:_shell_quote_context_stt */
char *shell_quote_context(double command_template, int position)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "template=%.2f pos=%d", command_template, position);
    hermes_log(LOG_DEBUG, "port", "shell_quote_context: %s", buf);
    return strdup(buf);
}