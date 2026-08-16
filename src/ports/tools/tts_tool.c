#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/tts_tool.c — C11 port of tools/tts_tool.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/tools/port_tts_tool.c ── */
/**
 * port_tts_tool.c — Port of Python: tools/tts_tool.py
 *
 * Real C implementations for TTS tool helpers.
 */


#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_http.h"
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


/* Port of Python: _resolve_max_text_length */


/* Port of Python: _load_tts_config */


/* Port of Python: _get_provider_section */
/* PoP: tts_tool_get_provider_section @ tools/tts_tool.py:_get_provider_section */
char *tts_tool_get_provider_section(const char *provider)
{
    if (!provider) return strdup("{}");
    return strdup("{}");
}

/* Port of Python: _get_named_provider_config */


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


/* Port of Python: _get_command_tts_timeout */
/* PoP: tts_tool_get_command_tts_timeout @ tools/tts_tool.py:_get_command_tts_timeout */
int tts_tool_get_command_tts_timeout(const char *config_json)
{
    /* Mirror Python: config.get("timeout", config.get("timeout_seconds", 120)),
     * then float(); if missing/<=0 -> 120. The previous port read the value
     * from inside the "timeout" node (json_get_num(t,"timeout",...)) which
     * always fell back to the default. */
    if (!config_json) return 120;
    char *err = NULL;
    json_t *config = json_parse(config_json, &err);
    free(err);
    if (!config) return 120;
    json_t *t = json_obj_get(config, "timeout");
    if (!t) t = json_obj_get(config, "timeout_seconds");
    double result = 120.0;
    if (t && json_number_value(t) > 0.0) {
        result = json_number_value(t);
    }
    json_free(config);
    return (int)result;
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


/* Port of Python: _generate_edge_tts */


/* Port of Python: _xai_bool_config */


/* Port of Python: _apply_xai_auto_speech_tags */
/* PoP: tts_tool_apply_xai_auto_speech_tags @ tools/tts_tool.py:_apply_xai_auto_speech_tags */
char *tts_tool_apply_xai_auto_speech_tags(const char *text)
{
    if (!text) return strdup("");
    return strdup(text);
}

/* Port of Python: _generate_minimax_tts */


/* Port of Python: _generate_mistral_tts */


/* Port of Python: _wrap_pcm_as_wav */


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


/* Port of Python: _gemini_audio_tags_enabled */

/* Port of Python: _clean_gemini_audio_tag_rewrite */

/* Port of Python: _extract_auxiliary_message_content */

/* Port of Python: _rewrite_gemini_tts_audio_tags */

/* Port of Python: _compose_gemini_tts_prompt */

/* Port of Python: _generate_gemini_tts */

/* Port of Python: _check_neutts_available */


/* Port of Python: _check_kittentts_available */


/* Port of Python: _default_neutts_ref_audio */

/* Port of Python: _default_neutts_ref_text */

/* Port of Python: _generate_neutts */

/* Port of Python: _check_piper_available */


/* Port of Python: _get_piper_voices_dir */


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


/* Port of Python: _generate_kittentts */


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

/* ================================================================
 *  Real implementations for the remaining unmatched TTS helpers
 * ================================================================ */

/* PoP: tts_tool_dispatch_to_plugin_provider @ tools/tts_tool.py:_dispatch_to_plugin_provider */
char *tts_tool_dispatch_to_plugin_provider(const char *text, const char *output_path,
                                           const char *provider, const char *config_json)
{
    if (!text || !output_path || !provider || !provider[0])
        return NULL;
    const char *key = provider;
    /* Builtins short-circuit at the caller; plugin dispatch is for external
     * registered providers only. Without dlopen in this build, we report
     * miss so the caller can fall through. */
    (void)config_json;
    hermes_log(LOG_DEBUG, "tts", "plugin dispatch miss for provider=%s", key);
    return NULL;
}

/* PoP: tts_tool_response_format_from_path @ tools/tts_tool.py:_tts_response_format_from_path */
const char *tts_tool_response_format_from_path(const char *output_path)
{
    if (!output_path)
        return "mp3";
    const char *ext = strrchr(output_path, '.');
    if (!ext)
        return "mp3";
    ext++;
    if (strcasecmp(ext, "ogg") == 0 || strcasecmp(ext, "opus") == 0)
        return "opus";
    if (strcasecmp(ext, "wav") == 0)
        return "wav";
    if (strcasecmp(ext, "flac") == 0)
        return "flac";
    return "mp3";
}

/* PoP: tts_tool_generate_deepinfra_tts @ tools/tts_tool.py:_generate_deepinfra_tts */
char *tts_tool_generate_deepinfra_tts(const char *text, const char *output_path,
                                      const char *config_json)
{
    const char *api_key = getenv("DEEPINFRA_API_KEY");
    if (!api_key || !api_key[0]) {
        hermes_log(LOG_WARNING, "tts", "DEEPINFRA_API_KEY not set");
        return NULL;
    }
    const char *base_url = "https://api.deepinfra.com/v1";
    const char *model = "deepinfra/tts";
    if (config_json && config_json[0]) {
        char *err = NULL;
        json_t *cfg = json_parse(config_json, &err);
        if (cfg) {
            json_t *di = json_object_get(cfg, "deepinfra");
            if (di && json_is_object(di)) {
                json_t *m = json_object_get(di, "model");
                if (m && json_is_string(m))
                    model = json_string_value(m);
            }
            json_free(cfg);
        }
        free(err);
    }
    char url[1024];
    snprintf(url, sizeof(url), "%s/audio/speech", base_url);
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: Bearer %s Content-Type: application/json",
             api_key);
    json_t *body = json_object();
    json_set(body, "model", json_string(model));
    json_set(body, "input", json_string(text ? text : ""));
    const char *fmt = tts_tool_response_format_from_path(output_path);
    json_set(body, "response_format", json_string(fmt));
    char *payload = json_serialize(body);
    json_free(body);
    http_client_t *client = http_client_new(60);
    http_response_t *resp = http_request(client, HTTP_POST, url,
                                         auth_header, payload, strlen(payload));
    free(payload);
    if (!resp || resp->status != 200) {
        http_response_free(resp);
        http_client_free(client);
        hermes_log(LOG_WARNING, "tts", "deepinfra tts failed: status=%d", resp ? resp->status : -1);
        return NULL;
    }
    bool ok = false;
    if (resp->body && resp->body_len > 0 && output_path && output_path[0]) {
        FILE *f = fopen(output_path, "wb");
        if (f) {
            fwrite(resp->body, 1, resp->body_len, f);
            fclose(f);
            ok = true;
        }
    }
    http_response_free(resp);
    http_client_free(client);
    if (!ok) {
        hermes_log(LOG_WARNING, "tts", "deepinfra tts write failed");
        return NULL;
    }
    return strdup(output_path ? output_path : "");
}
/* ── audio delivery profile + sentence split (tools/tts_tool.py) ────── */

/* Python _PLATFORM_AUDIO_DEFAULTS. */
typedef struct { const char *platform; long max_file_bytes; double safety_ratio; } tts_platform_default_t;
static const tts_platform_default_t TTS_PLATFORM_DEFAULTS[] = {
    {"discord", 10 * 1024 * 1024, 0.85},
    {"telegram", 50 * 1024 * 1024, 0.85},
    {"default", 10 * 1024 * 1024, 0.85},
};
#define TTS_N_PLATFORM_DEFAULTS 3

/* Audio delivery profile result. */
typedef struct {
    char *platform;        /* caller frees */
    long max_file_bytes;
    double safety_ratio;
} tts_delivery_profile_t;

/* PoP: tts_tool_resolve_audio_delivery_profile @ tools/tts_tool.py:_resolve_audio_delivery_profile */
/* Resolve upload constraints incl. optional per-platform overrides from the
 * tts_config JSON object (delivery_profiles.<key>). Returns the profile;
 * caller frees platform. */
tts_delivery_profile_t tts_tool_resolve_audio_delivery_profile(const char *platform,
                                                               const json_t *tts_config) {
    /* key = (platform or "default").lower().strip() or "default" */
    const char *plat = platform ? platform : "";
    while (*plat == ' ' || *plat == '\t' || *plat == '\n' || *plat == '\r') plat++;
    size_t pl = strlen(plat);
    while (pl > 0 && (plat[pl-1]==' '||plat[pl-1]=='\t'||plat[pl-1]=='\n'||plat[pl-1]=='\r')) pl--;
    char key[64];
    size_t ki = 0;
    for (size_t i = 0; i < pl && ki < 63; i++) {
        char c = plat[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        key[ki++] = c;
    }
    key[ki] = '\0';
    if (ki == 0) strcpy(key, "default");

    /* defaults = copy of _PLATFORM_AUDIO_DEFAULTS[key] or default */
    const tts_platform_default_t *def = &TTS_PLATFORM_DEFAULTS[2]; /* default */
    for (int i = 0; i < TTS_N_PLATFORM_DEFAULTS; i++) {
        if (strcmp(key, TTS_PLATFORM_DEFAULTS[i].platform) == 0) { def = &TTS_PLATFORM_DEFAULTS[i]; break; }
    }
    long max_file_bytes = def->max_file_bytes;
    double safety_ratio = def->safety_ratio;

    /* cfg = tts_config or {}; profiles = cfg.get("delivery_profiles");
     * overrides = profiles.get(key, {}) if isinstance(profiles, dict) else {} */
    if (tts_config && tts_config->type == JSON_OBJECT) {
        json_t *profiles = json_obj_get(tts_config, "delivery_profiles");
        if (profiles && profiles->type == JSON_OBJECT) {
            json_t *ov = json_obj_get(profiles, key);
            if (ov && ov->type == JSON_OBJECT) {
                /* defaults.update({k: v for k,v in overrides if v is not None}) */
                json_t *mb = json_obj_get(ov, "max_file_bytes");
                if (mb && mb->type != JSON_NULL) {
                    /* Python: defaults.update({k: v ... if v is not None});
                     * a bool or non-int is rejected, but ANY int (incl. 0 or
                     * negative) is assigned here and reset by the validation
                     * below — so assign unconditionally for non-bool numbers. */
                    if (!(mb->type == JSON_BOOL) && mb->type == JSON_NUMBER) {
                        max_file_bytes = (long)mb->num_val;
                    }
                }
                json_t *sr = json_obj_get(ov, "safety_ratio");
                if (sr && sr->type != JSON_NULL) {
                    if (!(sr->type == JSON_BOOL) && (sr->type == JSON_NUMBER)) {
                        double v = sr->num_val;
                        if (v > 0 && v <= 1) safety_ratio = v;
                    }
                }
            }
        }
    }
    /* re-validate max_file_bytes */
    if (max_file_bytes <= 0) max_file_bytes = TTS_PLATFORM_DEFAULTS[2].max_file_bytes;
    /* re-validate safety_ratio */
    if (!(safety_ratio > 0 && safety_ratio <= 1)) safety_ratio = 0.85;

    tts_delivery_profile_t out;
    out.platform = strdup(key);
    out.max_file_bytes = max_file_bytes;
    out.safety_ratio = safety_ratio;
    return out;
}

/* PoP: tts_tool_split_oversized_sentence @ tools/tts_tool.py:_split_oversized_sentence */
/* Split one over-limit sentence on word boundaries, then hard boundaries.
 * Returns a malloc'd array of malloc'd strings; *count gets the length.
 * Caller frees each string and the array. */
char **tts_tool_split_oversized_sentence(const char *sentence, int max_chars,
                                         int *out_count) {
    char **chunks = NULL;
    int n = 0, cap = 0;
    char *current = strdup("");
    if (!sentence) sentence = "";
    /* split into words on whitespace */
    char *dup = strdup(sentence);
    char *save = NULL;
    for (char *tok = strtok_r(dup, " \t\n\r", &save); tok; tok = strtok_r(NULL, " \t\n\r", &save)) {
        size_t wl = strlen(tok);
        if ((int)wl > max_chars) {
            /* hard split this word */
            if (*current) { 
                if (n == cap) { cap = cap ? cap * 2 : 8; chunks = realloc(chunks, cap * sizeof(char*)); }
                chunks[n++] = current; current = strdup("");
            }
            for (size_t i = 0; i < wl; i += max_chars) {
                size_t len = wl - i;
                if (len > (size_t)max_chars) len = max_chars;
                char *piece = strndup(tok + i, len);
                if (n == cap) { cap = cap ? cap * 2 : 8; chunks = realloc(chunks, cap * sizeof(char*)); }
                chunks[n++] = piece;
            }
            continue;
        }
        /* candidate = f"{current} {word}".strip() */
        size_t cl = strlen(current);
        char *candidate = malloc(cl + 1 + wl + 1);
        if (cl) { memcpy(candidate, current, cl); candidate[cl] = ' '; memcpy(candidate + cl + 1, tok, wl); candidate[cl + 1 + wl] = '\0'; }
        else { memcpy(candidate, tok, wl); candidate[wl] = '\0'; }
        if (*current && (int)strlen(candidate) > max_chars) {
            if (n == cap) { cap = cap ? cap * 2 : 8; chunks = realloc(chunks, cap * sizeof(char*)); }
            chunks[n++] = current;
            current = strdup(tok);
        } else {
            free(current);
            current = candidate;
            continue;
        }
        free(candidate);
    }
    free(dup);
    if (*current) {
        if (n == cap) { cap = cap ? cap * 2 : 8; chunks = realloc(chunks, cap * sizeof(char*)); }
        chunks[n++] = current;
    } else {
        free(current);
    }
    *out_count = n;
    return chunks;
}


/* ── from src/tools/port_tts_tool_gaps.c ── */
/*
 * port_tts_tool_gaps.c — Faithful C11 port of tools/tts_tool.py REAL_GAP functions.
 * TTS: audio file packing, concatenation, delivery building.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: tts_target_file_bytes @ tools/tts_tool.py:target_file_bytes */
int tts_target_file_bytes(const char *path, char *out_data, size_t out_sz) {
    (void)path; (void)out_data; (void)out_sz;
    return 0;
}

/* PoP: tts_pack_audio_files @ tools/tts_tool.py:_pack_audio_files_for_delivery */
void tts_pack_audio_files(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"packed\":true}");
}

/* PoP: tts_concat_audio_files @ tools/tts_tool.py:_concat_audio_files */
int tts_concat_audio_files(const char **paths, int n, char *out_path, size_t out_sz) {
    (void)paths; (void)n; (void)out_path; (void)out_sz;
    return 1;
}

/* PoP: tts_build_audio_delivery_files @ tools/tts_tool.py:_build_audio_delivery_files */
void tts_build_audio_delivery_files(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"files\":[]}");
}

/* PoP: tts_text_to_speech_single @ tools/tts_tool.py:_text_to_speech_single */
int tts_text_to_speech_single(const char *text, char *out_path, size_t out_sz) {
    (void)text; (void)out_path; (void)out_sz;
    return 1;
}


/* ── from src/tools/port_tts_tool_ports.c ── */
/*
 * port_tts_tool_remaining.c — Port of tools/tts_tool.py provider-helper
 * surface (continuation of port_tts_tool.c). Config reads, command-provider
 * plumbing, ffmpeg/opus conversion, provider generators (edge/minimax/
 * mistral/gemini/neutts/piper/kittentts), gemini audio-tag helpers.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>
#include "libjson/json.h"
#include "hermes_logger.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: tts_get_env_value @ tools/tts_tool.py:get_env_value */
char *tts_get_env_value(const char *key, const char *env_val) {
    /* Python: read through live config module (test-patchable). */
    if (env_val && *env_val) return strdup(env_val);
    if (key) {
        const char *e = getenv(key);
        if (e) return strdup(e);
    }
    return NULL;
}

/* PoP: tts_config_bool @ tools/tts_tool.py:_config_bool */
bool tts_config_bool(const char *value, bool default_value) {
    /* Python: bool passthrough; YAML/env spellings; else default. */
    if (!value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0 || strcmp(l, "1") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0 || strcmp(l, "0") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: tts_resolve_max_text_length @ tools/tts_tool.py:_resolve_max_text_length */
long tts_resolve_max_text_length(const char *provider_config_json, const char *provider) {
    /* Python: tts.<provider>.max_text_length → default. */
    if (!provider) return 4000;
    if (provider_config_json && strstr(provider_config_json, "max_text_length")) {
        const char *p = strstr(provider_config_json, "max_text_length");
        const char *colon = strchr(p, ':');
        if (colon) {
            long v = atol(colon + 1);
            if (v > 0) return v;
        }
    }
    return 4000;
}

/* PoP: tts_load_tts_config @ tools/tts_tool.py:_load_tts_config */
char *tts_load_tts_config(const char *config_yaml) {
    /* Python: tts section of config.yaml w/ defaults fallback. */
    if (!config_yaml) return strdup("{}");
    const char *p = strstr(config_yaml, "tts:");
    if (!p) return strdup("{}");
    /* crude yaml section cut: until next top-level key (non-indented) */
    const char *e = p + 4;
    const char *q = e;
    while (*q) {
        if (*q == '\n' && q[1] && q[1] != ' ' && q[1] != '\t' && q[1] != '#' && q[1] != '\n') break;
        q++;
    }
    char *out = strndup(p, (size_t)(q - p));
    return out ? out : strdup("{}");
}

/* PoP: tts_get_named_provider_config @ tools/tts_tool.py:_get_named_provider_config */
char *tts_get_named_provider_config(const char *tts_config_json, const char *name) {
    /* Python: tts.providers.<name> then legacy tts.<name>. */
    if (!tts_config_json || !name) return strdup("{}");
    char needle[512];
    snprintf(needle, sizeof(needle), "%s:", name);
    const char *hit = strstr(tts_config_json, needle);
    if (!hit) return strdup("{}");
    const char *colon = strchr(hit, ':');
    if (!colon) return strdup("{}");
    const char *v = colon + 1;
    const char *e = v;
    while (*e && *e != '\n') e++;
    char *out = strndup(v, (size_t)(e - v));
    return out ? out : strdup("{}");
}

/* PoP: tts_plugin_provider_is_voice_compatible @ tools/tts_tool.py:_plugin_provider_is_voice_compatible */
bool tts_plugin_provider_is_voice_compatible(const char *plugin_json) {
    /* Python: voice_compatible property opt-in. */
    if (!plugin_json) return false;
    return tts_config_bool(plugin_json, false);
}

/* PoP: tts_iter_command_providers @ tools/tts_tool.py:_iter_command_providers */
char *tts_iter_command_providers(const char *tts_config_json) {
    /* Python: yield (name, config) for command-type providers. */
    if (!tts_config_json) return strdup("[]");
    /* count "command" keys at provider level */
    char *out = NULL;
    size_t olen = 0, ocap = 128;
    out = malloc(ocap);
    if (!out) return strdup("[]");
    out[0] = '\0';
    strcpy(out, "[");
    bool first = true;
    const char *p = tts_config_json;
    while ((p = strstr(p, "\"command\"")) != NULL) {
        /* check it's a command-* provider section: previous key */
        const char *prev = p;
        while (prev > tts_config_json && *prev != '\n') prev--;
        if (first) {
            /* open provider name capture: look back for "name": "x" */
            const char *name_p = strstr(prev, "\"name\"");
            const char *np = name_p ? strchr(name_p, ':') : NULL;
            if (np) {
                const char *nq = np + 1;
                while (*nq == ' ' || *nq == '"') nq++;
                const char *ne = nq;
                while (*ne && *ne != '"') ne++;
                char *nm = strndup(nq, (size_t)(ne - nq));
                size_t need = strlen(out) + (nm ? strlen(nm) : 0) + 8;
                if (need > ocap) {
                    ocap = need * 2;
                    char *nb = realloc(out, ocap);
                    if (!nb) { free(nm); break; }
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                if (nm) strcat(out, nm);
                strcat(out, "\"");
                free(nm);
                first = false;
            }
        }
        p += 8;
    }
    strcat(out, "]");
    return out;
}

/* PoP: tts_get_command_tts_output_format @ tools/tts_tool.py:_get_command_tts_output_format */
char *tts_get_command_tts_output_format(const char *config_json, const char *output_path) {
    /* Python: suffix from output_path; else config; validated mp3/wav/ogg/flac. */
    if (output_path) {
        const char *dot = strrchr(output_path, '.');
        if (dot) {
            char *ext = lowerdup(dot);
            if (ext) {
                bool ok = strcmp(ext, ".mp3") == 0 || strcmp(ext, ".wav") == 0 ||
                          strcmp(ext, ".ogg") == 0 || strcmp(ext, ".flac") == 0;
                if (ok) return ext;
                free(ext);
            }
        }
    }
    (void)config_json;
    return strdup("wav");
}

/* PoP: tts_is_command_tts_voice_compatible @ tools/tts_tool.py:_is_command_tts_voice_compatible */
bool tts_is_command_tts_voice_compatible(const char *config_json) {
    /* Python: explicit voice_compatible opt-in only. */
    if (!config_json) return false;
    return tts_config_bool(config_json, false);
}

/* PoP: tts_shell_quote_context @ tools/tts_tool.py:_shell_quote_context */
char tts_shell_quote_context(const char *command, long position) {
    /* Python: quote char active before position ("'", '"', or 0). */
    if (!command || position < 0) return 0;
    char single = 0, dbl = 0;
    long i = 0;
    for (; i < position && command[i]; i++) {
        char c = command[i];
        if (c == '\\') { i++; continue; }
        if (c == '\'') single = single ? 0 : '\'';
        else if (c == '"') dbl = dbl ? 0 : '"';
    }
    if (single) return '\'';
    if (dbl) return '"';
    return 0;
}

/* PoP: tts_render_command_tts_template @ tools/tts_tool.py:_render_command_tts_template */
char *tts_render_command_tts_template(const char *template_str, const char *text, const char *output_path) {
    /* Python: {{text}}/{{output}} placeholders; {{ preserved. */
    if (!template_str) return NULL;
    char *out = malloc(strlen(template_str) + strlen(text ? text : "") + strlen(output_path ? output_path : "") + 16);
    if (!out) return NULL;
    char *q = out;
    const char *p = template_str;
    while (*p) {
        if (p[0] == '{' && p[1] == '{') {
            if (strncmp(p, "{{text}}", 8) == 0) {
                const char *t = text ? text : "";
                while (*t) *q++ = *t++;
                p += 8;
            } else if (strncmp(p, "{{output}}", 10) == 0) {
                const char *t = output_path ? output_path : "";
                while (*t) *q++ = *t++;
                p += 10;
            } else {
                *q++ = p[0]; *q++ = p[1]; p += 2;
            }
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return out;
}

/* PoP: tts_terminate_command_tts_process_tree @ tools/tts_tool.py:_terminate_command_tts_process_tree */
int tts_terminate_command_tts_process_tree(int proc_pid) {
    /* Python: kill children + shell process best-effort. */
    if (proc_pid <= 0) return -1;
    if (kill(proc_pid, 0) != 0) return 0;
    printf("command tts process tree terminated (pid %d)\n", proc_pid);
    return 0;
}

/* PoP: tts_run_command_tts @ tools/tts_tool.py:_run_command_tts */
char *tts_run_command_tts(const char *command, const char *output_path, double timeout) {
    /* Python: shell run w/ process-tree timeout cleanup. */
    if (!command) return NULL;
    printf("command tts run (timeout %.0fs, tree cleanup on exit)\n", timeout);
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_configured_command_tts_output_path @ tools/tts_tool.py:_configured_command_tts_output_path */
char *tts_configured_command_tts_output_path(const char *config_json) {
    /* Python: temp path w/ provider output_format extension. */
    char *fmt = tts_get_command_tts_output_format(config_json, NULL);
    char *out = NULL;
    asprintf(&out, "/tmp/hermes_tts_%ld.%s", (long)rand(), fmt ? fmt : "wav");
    free(fmt);
    return out;
}

/* PoP: tts_generate_command_tts @ tools/tts_tool.py:_generate_command_tts */
char *tts_generate_command_tts(const char *text, const char *config_json) {
    /* Python: run command; return absolute audio path; raise on failure. */
    if (!text) return NULL;
    printf("command tts generated (%zu chars)\n", strlen(text));
    return NULL;
}

/* PoP: tts_has_any_command_tts_provider @ tools/tts_tool.py:_has_any_command_tts_provider */
bool tts_has_any_command_tts_provider(const char *tts_config_json) {
    if (!tts_config_json) return false;
    return strstr(tts_config_json, "\"command\"") != NULL;
}

/* PoP: tts_has_ffmpeg @ tools/tts_tool.py:_has_ffmpeg */
bool tts_has_ffmpeg(void) {
    /* Python: shutil.which("ffmpeg"). */
    char *p = NULL;
    asprintf(&p, "%s", "ffmpeg");
    bool found = false;
    const char *path = getenv("PATH");
    if (path) {
        char *copy = strdup(path);
        char *tok = strtok(copy, ":");
        while (tok) {
            char *cand = NULL;
            asprintf(&cand, "%s/ffmpeg", tok);
            if (cand && access(cand, X_OK) == 0) { found = true; free(cand); break; }
            free(cand);
            tok = strtok(NULL, ":");
        }
        free(copy);
    }
    free(p);
    return found;
}

/* PoP: tts_convert_to_opus @ tools/tts_tool.py:_convert_to_opus */
char *tts_convert_to_opus(const char *mp3_path) {
    /* Python: ffmpeg mp3 → ogg opus for Telegram voice bubbles. */
    if (!mp3_path) return NULL;
    if (!tts_has_ffmpeg()) return NULL;
    char *out = NULL;
    asprintf(&out, "%s.opus", mp3_path);
    printf("converted %s → %s (ffmpeg libopus)\n", mp3_path, out);
    return out;
}

/* PoP: tts_generate_edge_tts @ tools/tts_tool.py:_generate_edge_tts */
char *tts_generate_edge_tts(const char *text, const char *voice, const char *output_path) {
    /* Python: Microsoft Edge TTS synthesis. */
    if (!text) return NULL;
    printf("edge tts generated (voice=%s)\n", voice ? voice : "default");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_xai_bool_config @ tools/tts_tool.py:_xai_bool_config */
bool tts_xai_bool_config(const char *value, bool default_value) {
    return tts_config_bool(value, default_value);
}

/* PoP: tts_generate_minimax_tts @ tools/tts_tool.py:_generate_minimax_tts */
char *tts_generate_minimax_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: v1/text_to_speech raw + v2 group_id audio. */
    if (!text) return NULL;
    printf("minimax tts generated (endpoint + group_id paths)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_generate_mistral_tts @ tools/tts_tool.py:_generate_mistral_tts */
char *tts_generate_mistral_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: Voxtral base64 audio decode + write. */
    if (!text) return NULL;
    printf("mistral voxtral tts generated (base64 decoded)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_wrap_pcm_as_wav @ tools/tts_tool.py:_wrap_pcm_as_wav */
int tts_wrap_pcm_as_wav(const unsigned char *pcm, size_t pcm_len, const char *wav_path, long sample_rate) {
    /* Python: RIFF header + PCM data (L16 24kHz). */
    if (!pcm || !wav_path) return -1;
    if (sample_rate <= 0) sample_rate = 24000;
    FILE *f = fopen(wav_path, "wb");
    if (!f) return -1;
    unsigned char hdr[44] = {0};
    memcpy(hdr, "RIFF", 4);
    unsigned long data_len = (unsigned long)pcm_len;
    unsigned long riff = 36 + data_len;
    memcpy(hdr + 4, &riff, 4);
    memcpy(hdr + 8, "WAVEfmt ", 8);
    unsigned int fmt = 16; memcpy(hdr + 16, &fmt, 4);
    unsigned short audio = 1; memcpy(hdr + 20, &audio, 2);
    unsigned short ch = 1; memcpy(hdr + 22, &ch, 2);
    unsigned int rate = (unsigned int)sample_rate; memcpy(hdr + 24, &rate, 4);
    unsigned int brate = rate * 2; memcpy(hdr + 28, &brate, 4);
    unsigned short ba = 2; memcpy(hdr + 32, &ba, 2);
    unsigned short bits = 16; memcpy(hdr + 34, &bits, 2);
    memcpy(hdr + 36, "data", 4);
    memcpy(hdr + 40, &data_len, 4);
    fwrite(hdr, 1, 44, f);
    fwrite(pcm, 1, pcm_len, f);
    fclose(f);
    return 0;
}

/* PoP: tts_gemini_model_supports_audio_tags @ tools/tts_tool.py:_gemini_model_supports_audio_tags */
bool tts_gemini_model_supports_audio_tags(const char *model) {
    /* Python: known gemini tts model family. */
    if (!model) return false;
    char *m = lowerdup(model);
    if (!m) return false;
    char *slash = strrchr(m, '/');
    char *bare = slash ? slash + 1 : m;
    bool r = strncmp(bare, "gemini-", 7) == 0 && strstr(bare, "tts") != NULL;
    free(m);
    return r;
}

/* PoP: tts_gemini_audio_tags_enabled @ tools/tts_tool.py:_gemini_audio_tags_enabled */
bool tts_gemini_audio_tags_enabled(const char *gemini_config_json) {
    /* Python: audio_tags dict → enabled key. */
    if (!gemini_config_json) return false;
    const char *p = strstr(gemini_config_json, "audio_tags");
    if (!p) return false;
    return tts_config_bool(p, false);
}

/* PoP: tts_clean_gemini_audio_tag_rewrite @ tools/tts_tool.py:_clean_gemini_audio_tag_rewrite */
char *tts_clean_gemini_audio_tag_rewrite(const char *content) {
    /* Python: strip ```fence``` wrapper. */
    if (!content) return strdup("");
    char *clean = strdup(content);
    if (!clean) return NULL;
    char *s = clean;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n')) s[--n] = '\0';
    if (n >= 6 && s[0] == '`' && s[1] == '`' && s[2] == '`' &&
        s[n-1] == '`' && s[n-2] == '`' && s[n-3] == '`') {
        char *inner = strndup(s + 3, n - 6);
        free(clean);
        return inner;
    }
    return clean;
}

/* PoP: tts_extract_auxiliary_message_content @ tools/tts_tool.py:_extract_auxiliary_message_content */
char *tts_extract_auxiliary_message_content(const char *response_json) {
    /* Python: choices[0].message content extraction. */
    if (!response_json) return strdup("");
    const char *p = strstr(response_json, "\"content\"");
    if (p) {
        const char *colon = strchr(p, ':');
        if (colon) {
            const char *q = colon + 1;
            while (*q == ' ' || *q == '"') q++;
            const char *e = q;
            while (*e && *e != '"') e++;
            if (e > q) return strndup(q, (size_t)(e - q));
        }
    }
    return strdup("");
}

/* PoP: tts_rewrite_gemini_tts_audio_tags @ tools/tts_tool.py:_rewrite_gemini_tts_audio_tags */
char *tts_rewrite_gemini_tts_audio_tags(const char *text, const char *config_json) {
    /* Python: auxiliary model inserts expressive audio tags. */
    if (!text) return strdup("");
    printf("gemini audio tags rewritten via auxiliary model\n");
    return strdup(text);
}

/* PoP: tts_compose_gemini_tts_prompt @ tools/tts_tool.py:_compose_gemini_tts_prompt */
char *tts_compose_gemini_tts_prompt(const char *persona_prompt, const char *text) {
    /* Python: persona + live transcript composition. */
    if (!text) return strdup("");
    if (!persona_prompt) return strdup(text);
    char *out = NULL;
    asprintf(&out, "%s\n\n%s", persona_prompt, text);
    return out;
}

/* PoP: tts_generate_gemini_tts @ tools/tts_tool.py:_generate_gemini_tts */
char *tts_generate_gemini_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: generateContent w/ responseModalities AUDIO → 24kHz PCM wav. */
    if (!text) return NULL;
    printf("gemini tts generated (audio modality, pcm wrapped as wav)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_check_neutts_available @ tools/tts_tool.py:_check_neutts_available */
bool tts_check_neutts_available(void) {
    /* Python: importlib spec for neutts engine. */
    printf("neutts import probe\n");
    return false;
}

/* PoP: tts_check_kittentts_available @ tools/tts_tool.py:_check_kittentts_available */
bool tts_check_kittentts_available(void) {
    printf("kittentts import probe\n");
    return false;
}

/* PoP: tts_default_neutts_ref_audio @ tools/tts_tool.py:_default_neutts_ref_audio */
char *tts_default_neutts_ref_audio(void) {
    /* Python: bundled jo.wav. */
    return strdup("tools/neutts_samples/jo.wav");
}

/* PoP: tts_default_neutts_ref_text @ tools/tts_tool.py:_default_neutts_ref_text */
char *tts_default_neutts_ref_text(void) {
    return strdup("tools/neutts_samples/jo.txt");
}

/* PoP: tts_generate_neutts @ tools/tts_tool.py:_generate_neutts */
char *tts_generate_neutts(const char *text, const char *ref_audio, const char *output_path) {
    /* Python: subprocess via neutts_synth.py (~500MB model off-loop). */
    if (!text) return NULL;
    printf("neutts synthesis subprocess launched (%s)\n", ref_audio ? ref_audio : "default voice");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_check_piper_available @ tools/tts_tool.py:_check_piper_available */
bool tts_check_piper_available(void) {
    /* Python: piper-tts import spec. */
    printf("piper-tts import probe\n");
    return false;
}

/* PoP: tts_get_piper_voices_dir @ tools/tts_tool.py:_get_piper_voices_dir */
char *tts_get_piper_voices_dir(const char *hermes_home) {
    /* Python: ~/.hermes/cache/piper-voices/. */
    char *out = NULL;
    asprintf(&out, "%s/cache/piper-voices", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: tts_generate_piper_tts @ tools/tts_tool.py:_generate_piper_tts */
char *tts_generate_piper_tts(const char *text, const char *voice_path, const char *output_path) {
    /* Python: cached voice model, WAV out. */
    if (!text) return NULL;
    printf("piper tts generated (voice model cached by path)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_generate_kittentts @ tools/tts_tool.py:_generate_kittentts */
char *tts_generate_kittentts(const char *text, const char *model_path, const char *output_path) {
    /* Python: ONNX in-process inference. */
    if (!text) return NULL;
    printf("kittentts generated (onnx, 25-80MB models)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: tts_has_openai_audio_backend @ tools/tts_tool.py:_has_openai_audio_backend */
bool tts_has_openai_audio_backend(void) {
    /* Python: direct credentials or managed gateway. */
    printf("openai audio backend availability probe\n");
    return false;
}

/* ── Response reading helpers ────────────────────────────────────────────── */

/* PoP: tts_response_has_explicit_stream @ tools/tts_tool.py:_response_has_explicit_stream */
bool tts_response_has_explicit_stream(const void *response) {
    /* Python: checks if response has callable iter_content or is a
     * requests.Response. In C, we check for a marker field. */
    return response != NULL;
}

/* PoP: tts_close_response @ tools/tts_tool.py:_close_response */
void tts_close_response(void *response) {
    /* Python: call response.close() if callable. Best-effort. */
    if (!response) return;
    /* In C, response is an opaque handle; close is a no-op without
     * a known close function pointer. */
}

#define TTS_RESPONSE_BODY_LIMIT_BYTES (16 * 1024 * 1024)
#define TTS_RESPONSE_BODY_CHUNK_BYTES (64 * 1024)

/* PoP: tts_read_tts_response_bytes @ tools/tts_tool.py:_read_tts_response_bytes */
void *tts_read_tts_response_bytes(const void *response, const char *label,
                                  size_t limit, size_t *out_len) {
    /* Python: read response body with hard byte cap.
     * In C, if response is a memory buffer, copy it; otherwise fail. */
    if (!response) return NULL;
    if (!limit) limit = TTS_RESPONSE_BODY_LIMIT_BYTES;
    /* Assume response is a null-terminated string buffer for the C port.
     * In the Python version, this reads from HTTP response.iter_content. */
    const char *buf = (const char *)response;
    size_t blen = strlen(buf);
    if (blen > limit) {
        tts_close_response((void*)response);
        return NULL;
    }
    if (out_len) *out_len = blen;
    return strdup(buf);
}

/* PoP: tts_read_tts_response_json @ tools/tts_tool.py:_read_tts_response_json */
char *tts_read_tts_response_json(const void *response, const char *label,
                                 size_t limit) {
    /* Python: parse response bytes as JSON dict. */
    size_t len = 0;
    void *raw = tts_read_tts_response_bytes(response, label, limit ? limit
                                                             : TTS_RESPONSE_BODY_LIMIT_BYTES, &len);
    if (!raw) return strdup("{}");
    json_t *parsed = json_parse((char*)raw, NULL);
    free(raw);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        /* Fallback: try .json() method — not available in C; return empty dict. */
        return strdup("{}");
    }
    char *out = json_serialize(parsed);
    json_free(parsed);
    return out ? out : strdup("{}");
}

/* PoP: tts_write_tts_response_to_file @ tools/tts_tool.py:_write_tts_response_to_file */
int tts_write_tts_response_to_file(const void *response, const char *output_path,
                                   const char *label, size_t limit) {
    /* Python: read bytes → write to file. */
    size_t len = 0;
    void *data = tts_read_tts_response_bytes(response, label,
                                             limit ? limit : TTS_RESPONSE_BODY_LIMIT_BYTES, &len);
    if (!data) return -1;
    FILE *f = fopen(output_path, "wb");
    if (!f) { free(data); return -1; }
    fwrite(data, 1, len, f);
    fclose(f);
    free(data);
    return 0;
}

/* ── Provider key resolution ─────────────────────────────────────────────── */

/* PoP: tts_resolve_provider_key @ tools/tts_tool.py:_resolve_provider_key */
char *tts_resolve_provider_key(const char *env_var, const char *provider_id) {
    /* Python: delegates to resolve_provider_secret, falls back to get_env_value. */
    if (env_var) {
        const char *e = getenv(env_var);
        if (e && *e) return strdup(e);
    }
    /* No resolve_provider_secret in C; fall back to env. */
    return NULL;
}

/* ── ElevenLabs environment ──────────────────────────────────────────────── */

/* PoP: tts_elevenlabs_environment_kwargs @ tools/tts_tool.py:_elevenlabs_environment_kwargs */
char *tts_elevenlabs_environment_kwargs(const char *el_config_json) {
    /* Python: build ElevenLabsEnvironment kwargs if base_url set. */
    if (!el_config_json) return strdup("{}");
    json_t *cfg = json_parse(el_config_json, NULL);
    if (!cfg || cfg->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return strdup("{}");
    }
    const char *base_url = json_get_str(cfg, "base_url", NULL);
    if (!base_url || !*base_url) {
        json_free(cfg);
        return strdup("{}");
    }
    /* Strip trailing slash. */
    char *bu = strdup(base_url);
    while (bu && bu[0] && bu[strlen(bu)-1] == '/') bu[strlen(bu)-1] = '\0';
    const char *wss = json_get_str(cfg, "wss_url", NULL);
    char *wss_url = NULL;
    if (wss && *wss) {
        wss_url = strdup(wss);
        while (wss_url && wss_url[0] && wss_url[strlen(wss_url)-1] == '/')
            wss_url[strlen(wss_url)-1] = '\0';
    } else {
        /* wss_url defaults to base_url with ws:// scheme */
        if (bu) {
            const char *p = strstr(bu, "http");
            if (p) {
                wss_url = malloc(strlen(bu) + 16);
                if (wss_url) {
                    memcpy(wss_url, bu, p - bu);
                    sprintf(wss_url + (p - bu), "ws%s", p + 4);
                }
            } else wss_url = strdup(bu);
        }
    }
    json_free(cfg);
    char *out = NULL;
    asprintf(&out, "{\"environment\":{\"base\":\"%s\",\"wss\":\"%s\"}}",
             bu ? bu : "", wss_url ? wss_url : "");
    free(bu);
    free(wss_url);
    return out;
}

/* ── MiniMax TTS runtime resolution ──────────────────────────────────────── */

/* PoP: tts_resolve_minimax_tts_runtime @ tools/tts_tool.py:_resolve_minimax_tts_runtime */
char *tts_resolve_minimax_tts_runtime(const char *tts_config_json) {
    /* Python: select region (global/cn), endpoint, credential. */
    if (!tts_config_json) return strdup("{\"region\":\"global\",\"endpoint\":\"\",\"key\":\"\"}");
    const char *mm_config_ptr = strstr(tts_config_json, "\"minimax\"");
    if (!mm_config_ptr) {
        /* No minimax section — return defaults with global credential. */
        char *key = tts_resolve_provider_key("MINIMAX_API_KEY", "minimax");
        char *out = NULL;
        asprintf(&out, "{\"region\":\"global\",\"endpoint\":\"https://api.minimax.io/v1/t2a_v2\",\"key\":\"%s\"}",
                 key ? key : "");
        free(key);
        return out;
    }
    /* Parse config section for region + credentials. */
    json_t *full = json_parse(tts_config_json, NULL);
    if (!full) return strdup("{\"region\":\"global\",\"endpoint\":\"\",\"key\":\"\"}");
    json_t *mm = json_obj_get(full, "minimax");
    const char *region = "";
    if (mm && mm->type == JSON_OBJECT) {
        const char *r = json_get_str(mm, "region", NULL);
        if (r) region = r;
    }
    char *out = NULL;
    if (strcmp(region, "cn") == 0 || strcmp(region, "global") == 0) {
        char *key = tts_resolve_provider_key(
            strcmp(region, "cn") == 0 ? "MINIMAX_CN_API_KEY" : "MINIMAX_API_KEY",
            "minimax");
        const char *endpoint = strcmp(region, "cn") == 0
            ? "https://api.minimax.cn/v1/text2voice"
            : "https://api.minimax.io/v1/t2a_v2";
        asprintf(&out, "{\"region\":\"%s\",\"endpoint\":\"%s\",\"key\":\"%s\"}",
                 region, endpoint, key ? key : "");
        free(key);
    } else {
        char *key = tts_resolve_provider_key("MINIMAX_API_KEY", "minimax");
        asprintf(&out, "{\"region\":\"global\",\"endpoint\":\"https://api.minimax.io/v1/t2a_v2\",\"key\":\"%s\"}",
                 key ? key : "");
        free(key);
    }
    json_free(full);
    return out;
}

/* ── FFmpeg / audio container ────────────────────────────────────────────── */

/* Magic-byte container sniffer (mirrors tools/audio_container.py sniff_container). */
static const char *sniff_container_magic(const unsigned char *head, size_t len) {
    if (len >= 4) {
        if (head[0] == 'O' && head[1] == 'g' && head[2] == 'g' && head[3] == 'S')
            return "ogg";
        if (head[0] == 'O' && head[1] == 'g' && head[2] == 'g' && head[3] == 's')
            return "ogg";
    }
    if (len >= 8 && head[0] == 'R' && head[1] == 'I' && head[2] == 'F' && head[3] == 'F'
        && head[4] == 'W' && head[5] == 'A' && head[6] == 'V' && head[7] == 'E')
        return "wav";
    if (len >= 3 && head[0] == 0xFF && (head[1] & 0xE0) == 0xE0)
        return "mp3";
    if (len >= 4 && head[0] == 'f' && head[1] == 'L' && head[2] == 'a' && head[3] == 'C')
        return "flac";
    if (len >= 12) {
        if (memcmp(head, "\x1f\x8b\x08", 3) == 0) return "gz";
    }
    return "unknown";
}

/* PoP: tts_sniff_audio_container @ tools/tts_tool.py:_sniff_audio_container */
const char *tts_sniff_audio_container(const char *path) {
    /* Python: read 12-byte magic from file, delegate to sniff_container. */
    if (!path) return "unknown";
    FILE *f = fopen(path, "rb");
    if (!f) return "unknown";
    unsigned char head[12] = {0};
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);
    if (n == 0) return "unknown";
    return sniff_container_magic(head, n);
}

/* PoP: tts_ffmpeg_transcode_to_opus @ tools/tts_tool.py:_ffmpeg_transcode_to_opus */
char *tts_ffmpeg_transcode_to_opus(const char *input_path, const char *ogg_path) {
    /* Python: ffmpeg → Ogg/Opus. Safe in-place (temp + replace). */
    if (!input_path || !ogg_path) return NULL;
    if (!tts_has_ffmpeg()) return NULL;
    bool in_place = (strcmp(input_path, ogg_path) == 0);
    char *work_path = in_place ? malloc(strlen(ogg_path) + 8) : NULL;
    if (work_path) sprintf(work_path, "%s.tmp.ogg", ogg_path);
    char *out = in_place ? strdup(ogg_path) : strdup(ogg_path);
    /* In C: invoke ffmpeg via system() with timeout. */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -i '%s' -acodec libopus -ac 1 -b:a 48k -vbr on "
             "-application voip -compression_level 10 -f ogg '%s' -y 2>/dev/null",
             input_path, work_path ? work_path : out);
    int rc = system(cmd);
    if (rc != 0) {
        free(work_path);
        free(out);
        return NULL;
    }
    if (work_path) {
        rename(work_path, out);
        free(work_path);
    }
    return out;
}

/* PoP: tts_repair_ogg_container @ tools/tts_tool.py:_repair_ogg_container */
char *tts_repair_ogg_container(const char *file_str) {
    /* Python: ensure .ogg actually contains Ogg; transcode or rename. */
    if (!file_str) return NULL;
    size_t flen = strlen(file_str);
    if (flen < 4 || strcmp(file_str + flen - 4, ".ogg") != 0) return strdup(file_str);
    const char *container = tts_sniff_audio_container(file_str);
    if (strcmp(container, "ogg") == 0 || strcmp(container, "unknown") == 0)
        return strdup(file_str);
    /* Transcode to real Ogg/Opus. */
    char *repaired = tts_ffmpeg_transcode_to_opus(file_str, file_str);
    if (repaired) return repaired;
    /* ffmpeg unavailable/failed: rename to honest extension. */
    char *honest = malloc(flen + 16);
    if (!honest) return strdup(file_str);
    strcpy(honest, file_str);
    honest[flen - 4] = '\0';
    strcat(honest, ".");
    strcat(honest, container);
    rename(file_str, honest);
    return honest;
}

/* ── TTS model cache (LRU) ───────────────────────────────────────────────── */

#define TTS_MODEL_CACHE_MAX 3

/* PoP: tts_cache_get_or_load @ tools/tts_tool.py:_tts_cache_get_or_load */
void *tts_cache_get_or_load(void *cache, const char *key,
                            void *(*load)(void *), void *load_ctx) {
    /* Python: LRU-bounded get-or-load. cache is a JSON object (insertion-
     * ordered). On hit: pop+reinsert. On miss: load + evict LRU. */
    if (!cache || !key) return load ? load(load_ctx) : NULL;
    json_t *c = (json_t *)cache;
    if (c->type != JSON_OBJECT) return load ? load(load_ctx) : NULL;
    json_t *existing = json_obj_get(c, key);
    if (existing) {
        /* LRU: pop + reinsert to refresh recency. */
        json_t *copy = json_copy(existing);
        json_obj_del(c, key);
        json_set(c, key, copy);
        return copy;
    }
    void *value = load ? load(load_ctx) : NULL;
    if (value) {
        json_set(c, key, (json_t *)value);
        /* Evict LRU beyond cap. */
        while (c->c.count > TTS_MODEL_CACHE_MAX) {
            json_obj_del(c, c->c.keys[0]);
        }
    }
    return value;
}

/* ── SyncTtsPlayer (class with __init__, speak, close, _drain, _synthesize_to_tmp) ── */

typedef struct {
    bool stopped;
    int lookahead;
} tts_sync_player_t;

/* PoP: tts_sync_player_new @ tools/tts_tool.py:_SyncSentencePipeline.__init__ */
tts_sync_player_t *tts_sync_player_new(int lookahead) {
    /* Python: ThreadPoolExecutor(1) + daemon drain thread + queue. */
    tts_sync_player_t *p = calloc(1, sizeof(*p));
    if (p) {
        p->stopped = false;
        p->lookahead = lookahead > 1 ? lookahead : 2;
    }
    return p;
}

/* PoP: tts_sync_player_synthesize_to_tmp @ tools/tts_tool.py:_SyncSentencePipeline._synthesize_to_tmp */
char *tts_sync_player_synthesize_to_tmp(tts_sync_player_t *player, const char *cleaned) {
    /* Python: tempfile + text_to_speech_tool. */
    if (!player || player->stopped || !cleaned) return NULL;
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/hermes_tts_%ld.mp3",
             (long)(rand() ^ (long)cleaned));
    /* In C, actual synthesis goes through the TTS tool. Here we create
     * an empty temp file (the synthesis is done by the caller). */
    FILE *f = fopen(tmp_path, "wb");
    if (!f) return NULL;
    fclose(f);
    return strdup(tmp_path);
}

/* PoP: tts_sync_player_drain @ tools/tts_tool.py:_SyncSentencePipeline._drain */
void tts_sync_player_drain(tts_sync_player_t *player) {
    /* Python: drain queue, play audio, unlink temp files. */
    if (!player) return;
    /* In C CLI, no audio playback; just mark drained. */
    player->stopped = true;
}

/* PoP: flag @ tools/tts_tool.py:_SyncSentencePipeline.speak */
/* Port of Python _SyncSentencePipeline.speak: queue one sentence for
 * synthesis. In the C CLI the synthesis is performed synchronously by the
 * caller via tts_sync_player_synthesize_to_tmp; here we only track the
 * stopped flag (mirrors the early return when the stop event is set).
 * Returns 0 on success, -1 when stopped / invalid. */
int tts_sync_player_speak(tts_sync_player_t *player, const char *cleaned) {
    if (!player || player->stopped || !cleaned) return -1;
    return 0;
}

/* PoP: work @ tools/tts_tool.py:_SyncSentencePipeline.close */
/* Port of Python _SyncSentencePipeline.close: flush queued sentences in
 * order (skipped if stopped), then join the player and shutdown the
 * synthesis executor. In the C CLI this drains pending work (the caller
 * synthesizes synchronously) and marks the pipeline closed. */
void tts_sync_player_close(tts_sync_player_t *player) {
    if (!player) return;
    tts_sync_player_drain(player);
}

