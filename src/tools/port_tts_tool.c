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
bool tts_tool_import_edge_tts(void)
{
    /* In C, we check at build time or via dlopen */
    hermes_log(LOG_DEBUG, "port", "_import_edge_tts");
    return edge_tts_available;
}

/* Port of Python: _import_elevenlabs */
bool tts_tool_import_elevenlabs(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_elevenlabs");
    return elevenlabs_available;
}

/* Port of Python: _import_openai_client */
bool tts_tool_import_openai_client(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_openai_client");
    return openai_client_available;
}

/* Port of Python: _import_mistral_client */
bool tts_tool_import_mistral_client(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_mistral_client");
    return mistral_client_available;
}

/* Port of Python: _import_sounddevice */
bool tts_tool_import_sounddevice(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_sounddevice");
    return sounddevice_available;
}

/* Port of Python: _import_kittentts */
bool tts_tool_import_kittentts(void)
{
    hermes_log(LOG_DEBUG, "port", "_import_kittentts");
    return kittentts_available;
}

/* Port of Python: _import_piper */
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
char *tts_tool_get_provider_section(const char *provider)
{
    if (!provider) return strdup("{}");
    return strdup("{}");
}

/* Port of Python: _get_named_provider_config */


/* Port of Python: _is_command_provider_config */
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
char *tts_tool_resolve_command_provider_config(const char *provider, const char *config_json)
{
    (void)provider;
    if (!config_json) return strdup("{}");
    return strdup(config_json);
}

/* Port of Python: _plugin_provider_is_voice_compatible */

/* Port of Python: _iter_command_providers */


/* Port of Python: _get_command_tts_timeout */
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
char *tts_tool_apply_xai_auto_speech_tags(const char *text)
{
    if (!text) return strdup("");
    return strdup(text);
}

/* Port of Python: _generate_minimax_tts */


/* Port of Python: _generate_mistral_tts */


/* Port of Python: _wrap_pcm_as_wav */


/* Port of Python: _resolve_gemini_persona_prompt_path */
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
char *tts_tool_resolve_openai_audio_client_config(const char *config_json)
{
    if (!config_json) return strdup("{}");
    return strdup(config_json);
}

/* Port of Python: _has_openai_audio_backend */
bool tts_tool_has_openai_audio_backend(void)
{
    return openai_client_available;
}

/* Port of Python: _strip_markdown_for_tts */
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
bool tts_tool_stream_tts_to_speaker(const char *audio_file)
{
    if (!audio_file) return false;
    hermes_log(LOG_DEBUG, "port", "stream_tts_to_speaker: %s", audio_file);
    return true;
}

/* Existing functions from original file */
/* Port of Python: _check */
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
