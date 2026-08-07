/*
 * voice_mode_pure.c — Pure/platform/config helpers ported from
 * tools/voice_mode.py. No audio-runtime deps (numpy/sounddevice/ALSA).
 * Closes pure-helpers REAL_GAPs in the voice_mode module.
 */
#define _GNU_SOURCE
#include "voice_mode_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>

#include "truthy.h"  /* is_truthy_value */

#define VM_DEFAULT_BEEP_VOLUME 0.3

/* ── _is_nan ─────────────────────────────────────────────────────── */

/* PoP: _is_nan @ tools/voice_mode.py:_is_nan */
bool is_nan(double value) {
    return isnan(value);
}

/* ── _get_beep_volume ────────────────────────────────────────────── */

/* PoP: _get_beep_volume @ tools/voice_mode.py:_get_beep_volume */
double voice_get_beep_volume(const json_t *voice_cfg) {
    const double default_vol = VM_DEFAULT_BEEP_VOLUME;
    if (!voice_cfg || voice_cfg->type != JSON_OBJECT) return default_vol;
    json_t *raw = json_obj_get(voice_cfg, "beep_volume");
    if (!raw) return default_vol;
    double vol;
    if (raw->type == JSON_NUMBER) vol = raw->num_val;
    else if (raw->type == JSON_BOOL) return default_vol;  /* bool rejected */
    else return default_vol;
    if (is_nan(vol) || vol < 0.0 || vol > 1.0) return default_vol;
    return vol;
}

/* ── _sounddevice_output_allowed ──────────────────────────────────── */

/* PoP: _sounddevice_output_allowed @ tools/voice_mode.py:_sounddevice_output_allowed */
bool _sounddevice_output_allowed(void) {
#ifdef __APPLE__
    return false;
#else
    return true;
#endif
}

/* ── _is_wsl / _is_wsl2_env / _wsl_powershell_tts_available ───────── */

static const char *_proc_version(void) {
    static char buf[512];
    static int done = 0;
    if (done) return buf;
    FILE *f = fopen("/proc/version", "r");
    if (!f) { buf[0] = '\0'; done = 1; return buf; }
    if (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    } else buf[0] = '\0';
    fclose(f);
    done = 1;
    return buf;
}

/* PoP: _is_wsl @ tools/voice_mode.py:_is_wsl */
bool _is_wsl(void) {
    return strstr(_proc_version(), "microsoft") != NULL;
}

/* PoP: _is_wsl2_env @ tools/voice_mode.py:_is_wsl2_env */
bool _is_wsl2_env(void) {
    return strstr(_proc_version(), "microsoft") != NULL;
}

/* PoP: _wsl_powershell_tts_available @ tools/voice_mode.py:_wsl_powershell_tts_available */
bool _wsl_powershell_tts_available(void) {
    if (!_is_wsl2_env()) return false;
    return access("/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe", X_OK) == 0
        && access("/usr/bin/ffmpeg", X_OK) == 0;
}

/* ── _voice_debug_enabled ────────────────────────────────────────── */

/* PoP: _voice_debug_enabled @ tools/voice_mode.py:_voice_debug_enabled */
bool _voice_debug_enabled(void) {
    const char *v = getenv("HERMES_VOICE_DEBUG");
    return v && strcmp(v, "1") == 0;
}

/* ── _vad_log ────────────────────────────────────────────────────── */

/* PoP: _vad_log @ tools/voice_mode.py:_vad_log */
void _vad_log(const char *msg) {
    if (!msg) return;
    fprintf(stderr, "[voice-vad] %s\n", msg);
}

/* ── _load_voice_stop_phrases ────────────────────────────────────── */

/* PoP: _load_voice_stop_phrases @ tools/voice_mode.py:_load_voice_stop_phrases */
char **_load_voice_stop_phrases(const json_t *voice_cfg, int *out_count) {
    *out_count = 0;
    json_t *phrases = NULL;
    if (voice_cfg && voice_cfg->type == JSON_OBJECT)
        phrases = json_obj_get(voice_cfg, "stop_phrases");
    if (!phrases) {
        char **r = (char **)malloc(sizeof(char*) * 1);
        r[0] = strdup("stop");
        *out_count = 1;
        return r;
    }
    if (phrases->type == JSON_STRING) {
        char **r = (char **)malloc(sizeof(char*) * 1);
        r[0] = strdup(phrases->str_val);
        *out_count = 1;
        return r;
    }
    if (phrases->type != JSON_ARRAY) {
        char **r = (char **)malloc(sizeof(char*) * 1);
        r[0] = strdup("stop");
        *out_count = 1;
        return r;
    }
    int n = (int)phrases->c.count, valid = 0;
    char **r = (char **)calloc(n ? n : 1, sizeof(char*));
    for (int i = 0; i < n; i++) {
        json_t *p = json_get(phrases, (size_t)i);
        if (!p) continue;
        char buf[256];
        if (p->type == JSON_STRING) snprintf(buf, sizeof(buf), "%s", p->str_val);
        else if (p->type == JSON_NUMBER) snprintf(buf, sizeof(buf), "%g", p->num_val);
        else continue;
        char *s = strdup(buf);
        char *t = s; while (*t && isspace((unsigned char)*t)) t++;
        char *e = t + strlen(t);
        while (e > t && isspace((unsigned char)*(e-1))) { *(--e) = '\0'; }
        if (t == e || *t == '\0') { free(s); continue; }
        for (char *p2 = t; *p2; p2++) *p2 = tolower((unsigned char)*p2);
        r[valid++] = strdup(t);
        free(s);
    }
    *out_count = valid;
    return r;
}

void free_voice_stop_phrases(char **phrases, int count) {
    if (!phrases) return;
    for (int i = 0; i < count; i++) free(phrases[i]);
    free(phrases);
}

/* ── is_voice_stop_phrase ────────────────────────────────────────── */

/* PoP: is_voice_stop_phrase @ tools/voice_mode.py:is_voice_stop_phrase */
bool is_voice_stop_phrase(const char *transcript,
                          char **stop_phrases, int phrase_count) {
    if (!transcript || !*transcript) return false;
    char *s = strdup(transcript);
    char *t = s; while (*t && isspace((unsigned char)*t)) t++;
    char *e = t + strlen(t); while (e > t && isspace((unsigned char)*(e-1))) { *(--e) = '\0'; }
    for (char *p = t; *p; p++) *p = tolower((unsigned char)*p);
    char cleaned[1024];
    int ci = 0;
    const char *punct = ".,!?;:\t\n\"' ";
    for (const char *p2 = t; *p2 && ci < (int)sizeof(cleaned)-1; p2++) {
        if (strchr(punct, *p2)) continue;
        cleaned[ci++] = *p2;
    }
    cleaned[ci] = '\0';
    bool found = false;
    if (ci > 0) {
        for (int i = 0; i < phrase_count; i++) {
            if (stop_phrases && stop_phrases[i] && strcmp(cleaned, stop_phrases[i]) == 0) {
                found = true; break;
            }
        }
    }
    free(s);
    return found;
}

/* ── voice_stop_hint ─────────────────────────────────────────────── */

/* PoP: voice_stop_hint @ tools/voice_mode.py:voice_stop_hint */
char *voice_stop_hint(char **stop_phrases, int phrase_count) {
    if (phrase_count == 0 || !stop_phrases || !stop_phrases[0]) return strdup("");
    char *hint = (char *)malloc(256);
    snprintf(hint, 256, "Say \"%s\" to end the voice chat.", stop_phrases[0]);
    return hint;
}

/* ── thinking_sound_enabled ──────────────────────────────────────── */

/* PoP: thinking_sound_enabled @ tools/voice_mode.py:thinking_sound_enabled */
bool thinking_sound_enabled(const json_t *voice_cfg) {
    if (!voice_cfg || voice_cfg->type != JSON_OBJECT) return true;
    json_t *raw = json_obj_get(voice_cfg, "thinking_sound");
    if (!raw) return true;
    if (raw->type == JSON_BOOL) return raw->bool_val;
    if (raw->type == JSON_STRING) return is_truthy_value(raw->str_val, true);
    return true;
}

/* ── mark_audio_output_active / is_audio_output_active ───────────── */

static int g_audio_output_active_count = 0;
static pthread_mutex_t g_audio_output_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: mark_audio_output_active @ tools/voice_mode.py:mark_audio_output_active */
void mark_audio_output_active(bool active) {
    pthread_mutex_lock(&g_audio_output_lock);
    if (active) g_audio_output_active_count++;
    else g_audio_output_active_count = g_audio_output_active_count > 0
        ? g_audio_output_active_count - 1 : 0;
    pthread_mutex_unlock(&g_audio_output_lock);
}

/* PoP: is_audio_output_active @ tools/voice_mode.py:is_audio_output_active */
bool is_audio_output_active(void) {
    pthread_mutex_lock(&g_audio_output_lock);
    bool r = g_audio_output_active_count > 0;
    pthread_mutex_unlock(&g_audio_output_lock);
    return r;
}

/* PoP: _max_duration_reached @ tools/voice_mode.py:_max_duration_reached */
/* Whether the configured hard recording-length cap has elapsed.
 * cap: voice.max_recording_seconds (0 or <=0 disables). elapsed: seconds. */
bool audio_recorder_max_duration_reached(double cap, double elapsed) {
    return cap > 0 && elapsed >= cap;
}
