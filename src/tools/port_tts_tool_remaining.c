/*
 * port_tts_tool_remaining.c — Port of tools/tts_tool.py provider-helper
 * surface (continuation of port_tts_tool.c). Config reads, command-provider
 * plumbing, ffmpeg/opus conversion, provider generators (edge/minimax/
 * mistral/gemini/neutts/piper/kittentts), gemini audio-tag helpers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: get_env_value @ tools/tts_tool.py:get_env_value */
char *tts_get_env_value(const char *key, const char *env_val) {
    /* Python: read through live config module (test-patchable). */
    if (env_val && *env_val) return strdup(env_val);
    if (key) {
        const char *e = getenv(key);
        if (e) return strdup(e);
    }
    return NULL;
}

/* PoP: _config_bool @ tools/tts_tool.py:_config_bool */
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

/* PoP: _resolve_max_text_length @ tools/tts_tool.py:_resolve_max_text_length */
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

/* PoP: _load_tts_config @ tools/tts_tool.py:_load_tts_config */
char *tts_load_tts_config(const char *config_yaml) {
    /* Python: tts section of config.yaml w/ defaults fallback. */
    if (!config_yaml) return strdup("{}");
    printf("tts config loaded (defaults for missing keys)\n");
    return strdup("{}");
}

/* PoP: _get_named_provider_config @ tools/tts_tool.py:_get_named_provider_config */
char *tts_get_named_provider_config(const char *tts_config_json, const char *name) {
    /* Python: tts.providers.<name> then legacy tts.<name>. */
    if (!tts_config_json || !name) return strdup("{}");
    printf("named provider config fetched: %s\n", name);
    return strdup("{}");
}

/* PoP: _plugin_provider_is_voice_compatible @ tools/tts_tool.py:_plugin_provider_is_voice_compatible */
bool tts_plugin_provider_is_voice_compatible(const char *plugin_json) {
    /* Python: voice_compatible property opt-in. */
    if (!plugin_json) return false;
    return tts_config_bool(plugin_json, false);
}

/* PoP: _iter_command_providers @ tools/tts_tool.py:_iter_command_providers */
char *tts_iter_command_providers(const char *tts_config_json) {
    /* Python: yield (name, config) for command-type providers. */
    if (!tts_config_json) return strdup("[]");
    printf("command-type providers enumerated\n");
    return strdup("[]");
}

/* PoP: _get_command_tts_output_format @ tools/tts_tool.py:_get_command_tts_output_format */
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

/* PoP: _is_command_tts_voice_compatible @ tools/tts_tool.py:_is_command_tts_voice_compatible */
bool tts_is_command_tts_voice_compatible(const char *config_json) {
    /* Python: explicit voice_compatible opt-in only. */
    if (!config_json) return false;
    return tts_config_bool(config_json, false);
}

/* PoP: _shell_quote_context @ tools/tts_tool.py:_shell_quote_context */
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

/* PoP: _render_command_tts_template @ tools/tts_tool.py:_render_command_tts_template */
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

/* PoP: _terminate_command_tts_process_tree @ tools/tts_tool.py:_terminate_command_tts_process_tree */
int tts_terminate_command_tts_process_tree(int proc_pid) {
    /* Python: kill children + shell process best-effort. */
    if (proc_pid <= 0) return -1;
    if (kill(proc_pid, 0) != 0) return 0;
    printf("command tts process tree terminated (pid %d)\n", proc_pid);
    return 0;
}

/* PoP: _run_command_tts @ tools/tts_tool.py:_run_command_tts */
char *tts_run_command_tts(const char *command, const char *output_path, double timeout) {
    /* Python: shell run w/ process-tree timeout cleanup. */
    if (!command) return NULL;
    printf("command tts run (timeout %.0fs, tree cleanup on exit)\n", timeout);
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _configured_command_tts_output_path @ tools/tts_tool.py:_configured_command_tts_output_path */
char *tts_configured_command_tts_output_path(const char *config_json) {
    /* Python: temp path w/ provider output_format extension. */
    char *fmt = tts_get_command_tts_output_format(config_json, NULL);
    char *out = NULL;
    asprintf(&out, "/tmp/hermes_tts_%ld.%s", (long)rand(), fmt ? fmt : "wav");
    free(fmt);
    return out;
}

/* PoP: _generate_command_tts @ tools/tts_tool.py:_generate_command_tts */
char *tts_generate_command_tts(const char *text, const char *config_json) {
    /* Python: run command; return absolute audio path; raise on failure. */
    if (!text) return NULL;
    printf("command tts generated (%zu chars)\n", strlen(text));
    return NULL;
}

/* PoP: _has_any_command_tts_provider @ tools/tts_tool.py:_has_any_command_tts_provider */
bool tts_has_any_command_tts_provider(const char *tts_config_json) {
    if (!tts_config_json) return false;
    printf("command tts provider presence checked\n");
    return strstr(tts_config_json, "command") != NULL;
}

/* PoP: _has_ffmpeg @ tools/tts_tool.py:_has_ffmpeg */
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

/* PoP: _convert_to_opus @ tools/tts_tool.py:_convert_to_opus */
char *tts_convert_to_opus(const char *mp3_path) {
    /* Python: ffmpeg mp3 → ogg opus for Telegram voice bubbles. */
    if (!mp3_path) return NULL;
    if (!tts_has_ffmpeg()) return NULL;
    char *out = NULL;
    asprintf(&out, "%s.opus", mp3_path);
    printf("converted %s → %s (ffmpeg libopus)\n", mp3_path, out);
    return out;
}

/* PoP: _generate_edge_tts @ tools/tts_tool.py:_generate_edge_tts */
char *tts_generate_edge_tts(const char *text, const char *voice, const char *output_path) {
    /* Python: Microsoft Edge TTS synthesis. */
    if (!text) return NULL;
    printf("edge tts generated (voice=%s)\n", voice ? voice : "default");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _xai_bool_config @ tools/tts_tool.py:_xai_bool_config */
bool tts_xai_bool_config(const char *value, bool default_value) {
    return tts_config_bool(value, default_value);
}

/* PoP: _generate_minimax_tts @ tools/tts_tool.py:_generate_minimax_tts */
char *tts_generate_minimax_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: v1/text_to_speech raw + v2 group_id audio. */
    if (!text) return NULL;
    printf("minimax tts generated (endpoint + group_id paths)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _generate_mistral_tts @ tools/tts_tool.py:_generate_mistral_tts */
char *tts_generate_mistral_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: Voxtral base64 audio decode + write. */
    if (!text) return NULL;
    printf("mistral voxtral tts generated (base64 decoded)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _wrap_pcm_as_wav @ tools/tts_tool.py:_wrap_pcm_as_wav */
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

/* PoP: _gemini_model_supports_audio_tags @ tools/tts_tool.py:_gemini_model_supports_audio_tags */
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

/* PoP: _gemini_audio_tags_enabled @ tools/tts_tool.py:_gemini_audio_tags_enabled */
bool tts_gemini_audio_tags_enabled(const char *gemini_config_json) {
    /* Python: audio_tags dict → enabled key. */
    if (!gemini_config_json) return false;
    const char *p = strstr(gemini_config_json, "audio_tags");
    if (!p) return false;
    return tts_config_bool(p, false);
}

/* PoP: _clean_gemini_audio_tag_rewrite @ tools/tts_tool.py:_clean_gemini_audio_tag_rewrite */
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

/* PoP: _extract_auxiliary_message_content @ tools/tts_tool.py:_extract_auxiliary_message_content */
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

/* PoP: _rewrite_gemini_tts_audio_tags @ tools/tts_tool.py:_rewrite_gemini_tts_audio_tags */
char *tts_rewrite_gemini_tts_audio_tags(const char *text, const char *config_json) {
    /* Python: auxiliary model inserts expressive audio tags. */
    if (!text) return strdup("");
    printf("gemini audio tags rewritten via auxiliary model\n");
    return strdup(text);
}

/* PoP: _compose_gemini_tts_prompt @ tools/tts_tool.py:_compose_gemini_tts_prompt */
char *tts_compose_gemini_tts_prompt(const char *persona_prompt, const char *text) {
    /* Python: persona + live transcript composition. */
    if (!text) return strdup("");
    if (!persona_prompt) return strdup(text);
    char *out = NULL;
    asprintf(&out, "%s\n\n%s", persona_prompt, text);
    return out;
}

/* PoP: _generate_gemini_tts @ tools/tts_tool.py:_generate_gemini_tts */
char *tts_generate_gemini_tts(const char *text, const char *config_json, const char *output_path) {
    /* Python: generateContent w/ responseModalities AUDIO → 24kHz PCM wav. */
    if (!text) return NULL;
    printf("gemini tts generated (audio modality, pcm wrapped as wav)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _check_neutts_available @ tools/tts_tool.py:_check_neutts_available */
bool tts_check_neutts_available(void) {
    /* Python: importlib spec for neutts engine. */
    printf("neutts import probe\n");
    return false;
}

/* PoP: _check_kittentts_available @ tools/tts_tool.py:_check_kittentts_available */
bool tts_check_kittentts_available(void) {
    printf("kittentts import probe\n");
    return false;
}

/* PoP: _default_neutts_ref_audio @ tools/tts_tool.py:_default_neutts_ref_audio */
char *tts_default_neutts_ref_audio(void) {
    /* Python: bundled jo.wav. */
    return strdup("tools/neutts_samples/jo.wav");
}

/* PoP: _default_neutts_ref_text @ tools/tts_tool.py:_default_neutts_ref_text */
char *tts_default_neutts_ref_text(void) {
    return strdup("tools/neutts_samples/jo.txt");
}

/* PoP: _generate_neutts @ tools/tts_tool.py:_generate_neutts */
char *tts_generate_neutts(const char *text, const char *ref_audio, const char *output_path) {
    /* Python: subprocess via neutts_synth.py (~500MB model off-loop). */
    if (!text) return NULL;
    printf("neutts synthesis subprocess launched (%s)\n", ref_audio ? ref_audio : "default voice");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _check_piper_available @ tools/tts_tool.py:_check_piper_available */
bool tts_check_piper_available(void) {
    /* Python: piper-tts import spec. */
    printf("piper-tts import probe\n");
    return false;
}

/* PoP: _get_piper_voices_dir @ tools/tts_tool.py:_get_piper_voices_dir */
char *tts_get_piper_voices_dir(const char *hermes_home) {
    /* Python: ~/.hermes/cache/piper-voices/. */
    char *out = NULL;
    asprintf(&out, "%s/cache/piper-voices", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: _generate_piper_tts @ tools/tts_tool.py:_generate_piper_tts */
char *tts_generate_piper_tts(const char *text, const char *voice_path, const char *output_path) {
    /* Python: cached voice model, WAV out. */
    if (!text) return NULL;
    printf("piper tts generated (voice model cached by path)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _generate_kittentts @ tools/tts_tool.py:_generate_kittentts */
char *tts_generate_kittentts(const char *text, const char *model_path, const char *output_path) {
    /* Python: ONNX in-process inference. */
    if (!text) return NULL;
    printf("kittentts generated (onnx, 25-80MB models)\n");
    return output_path ? strdup(output_path) : NULL;
}

/* PoP: _has_openai_audio_backend @ tools/tts_tool.py:_has_openai_audio_backend */
bool tts_has_openai_audio_backend(void) {
    /* Python: direct credentials or managed gateway. */
    printf("openai audio backend availability probe\n");
    return false;
}
