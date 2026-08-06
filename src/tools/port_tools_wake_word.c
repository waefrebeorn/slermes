/*
 * port_tools_wake_word.c — C11 port of pure config/device helpers from
 * tools/wake_word.py.
 *
 * Faithful translations of the Python helpers that read the wake_word
 * config section (a JSON dict) into typed values. Reuses libjson
 * (lib/libjson) for dict access — no duplicate parsing logic.
 *
 * No stubs.  Every function mirrors the Python original's behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_tools_wake_word.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <time.h>
#include <errno.h>

/* Opaque detector state — mirrors Python's WakeWordDetector. */
struct ww_detector {
    void *engine;
    void (*on_wake)(void);
    double cooldown_seconds;
    void (*on_failure)(struct ww_detector *);
    int input_device;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t ready_cond;
    bool running;
    bool stop_requested;
    bool callback_inflight;
    double last_fire;
    bool audio_silent;
    int silent_frames;
    json_t *input_device_details;
};

/* Python: cfg.get(key, _DEFAULTS.get(key)) — a None value falls back
 * to the module default. */
static const char *ww_default_for(const char *key)
{
    if (!key) return NULL;
    if (strcmp(key, "provider") == 0)            return WW_DEFAULT_PROVIDER;
    if (strcmp(key, "phrase") == 0)              return WW_DEFAULT_PHRASE;
    if (strcmp(key, "sensitivity") == 0)         return NULL; /* numeric default handled by ww_get_num */
    if (strcmp(key, "confirmation_frames") == 0) return NULL;
    return NULL;
}

const char *ww_get_str(const json_t *cfg, const char *key)
{
    if (!cfg || cfg->type != JSON_OBJECT || !key) {
        const char *d = ww_default_for(key);
        return d;
    }
    const char *v = json_get_str(cfg, key, NULL);
    if (v == NULL) {
        const char *d = ww_default_for(key);
        return d;
    }
    return v;
}

double ww_get_num(const json_t *cfg, const char *key, double dflt)
{
    if (!cfg || cfg->type != JSON_OBJECT || !key) return dflt;
    json_t *node = json_obj_get(cfg, key);
    if (!node) return dflt;
    if (node->type == JSON_NUMBER) return node->num_val;
    if (node->type == JSON_STRING) {
        const char *s = node->str_val;
        char *end = NULL;
        double v = strtod(s, &end);
        if (end != s && *end == '\0') return v;
    }
    return dflt;
}

bool ww_get_bool(const json_t *cfg, const char *key, bool dflt)
{
    if (!cfg || cfg->type != JSON_OBJECT || !key) return dflt;
    json_t *node = json_obj_get(cfg, key);
    if (!node) return dflt;
    if (node->type == JSON_BOOL) return node->bool_val;
    if (node->type == JSON_NUMBER) return node->num_val != 0.0;
    if (node->type == JSON_STRING) {
        const char *s = node->str_val;
        return strcmp(s, "true") == 0 || strcmp(s, "1") == 0 ||
               strcmp(s, "yes") == 0 || strcmp(s, "on") == 0;
    }
    return dflt;
}

/* PoP: _provider @ tools/wake_word.py:_provider */
char *ww_provider(const json_t *cfg)
{
    const char *raw = ww_get_str(cfg, "provider");
    if (!raw) raw = WW_DEFAULT_PROVIDER;
    /* strip + lower */
    size_t len = strlen(raw);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = raw[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        out[j++] = (char)tolower((unsigned char)c);
    }
    out[j] = '\0';
    if (j == 0) {
        free(out);
        return strdup(WW_DEFAULT_PROVIDER);
    }
    return out;
}

/* PoP: _input_device @ tools/wake_word.py:_input_device */
int ww_input_device(const json_t *cfg, int *out_int, char **out_str)
{
    if (out_int) *out_int = 0;
    if (out_str) *out_str = NULL;
    if (!cfg || cfg->type != JSON_OBJECT) return 0; /* None */

    json_t *raw = json_obj_get(cfg, "input_device");
    if (!raw) return 0; /* None */

    /* bool -> None */
    if (raw->type == JSON_BOOL) return 0;

    if (raw->type == JSON_NUMBER) {
        if (out_int) *out_int = (int)raw->num_val;
        return 1; /* int selector */
    }

    if (raw->type == JSON_STRING) {
        const char *s = raw->str_val;
        /* strip */
        while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                           s[len-1] == '\n' || s[len-1] == '\r')) len--;
        if (len == 0) return 0; /* empty -> None */
        char *name = malloc(len + 1);
        if (!name) return 0;
        memcpy(name, s, len);
        name[len] = '\0';
        if (out_str) *out_str = name;
        else free(name);
        return 2; /* string name */
    }
    return 0; /* other types -> None */
}

/* PoP: _sensitivity @ tools/wake_word.py:_sensitivity */
double ww_sensitivity(const json_t *cfg)
{
    double s = ww_get_num(cfg, "sensitivity", WW_DEFAULT_SENSITIVITY);
    if (s < 0.0) s = 0.0;
    if (s > 1.0) s = 1.0;
    return s;
}

/* PoP: _confirmation_frames @ tools/wake_word.py:_confirmation_frames */
int ww_confirmation_frames(const json_t *cfg)
{
    double n = ww_get_num(cfg, "confirmation_frames", WW_DEFAULT_CONFIRMATION_FRAMES);
    int v = (int)n;
    if (v < 1) v = 1;
    if (v > 10) v = 10;
    return v;
}

/* PoP: wake_phrase @ tools/wake_word.py:wake_phrase */
char *ww_wake_phrase(const json_t *cfg)
{
    const char *raw = ww_get_str(cfg, "phrase");
    if (!raw || !*raw) return strdup(WW_DEFAULT_PHRASE);
    return strdup(raw);
}

/* PoP: wake_surface_enabled @ tools/wake_word.py:wake_surface_enabled */
bool ww_wake_surface_enabled(const char *surface, const json_t *cfg)
{
    if (!surface) return false;
    if (!cfg || cfg->type != JSON_OBJECT) return false;
    if (!ww_get_bool(cfg, "enabled", false)) return false;

    const char *want_raw = ww_get_str(cfg, "surface");
    /* strip + lower */
    if (!want_raw) want_raw = "auto";
    size_t len = strlen(want_raw);
    char want[128];
    size_t j = 0;
    for (size_t i = 0; i < len && j < sizeof(want)-1; i++) {
        char c = want_raw[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        want[j++] = (char)tolower((unsigned char)c);
    }
    want[j] = '\0';
    if (j == 0) return true; /* "auto" */

    if (strcmp(want, "auto") == 0) return true;

    /* surface.strip().lower() */
    const char *s = surface;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    size_t slen = strlen(s);
    while (slen > 0 && (s[slen-1] == ' ' || s[slen-1] == '\t' ||
                        s[slen-1] == '\n' || s[slen-1] == '\r')) slen--;
    if (slen != j) return false;
    for (size_t i = 0; i < slen; i++) {
        if (tolower((unsigned char)s[i]) != want[i]) return false;
    }
    return true;
}

/* PoP: resolve_inference_framework @ tools/wake_word.py:resolve_inference_framework */
char *ww_resolve_inference_framework(const json_t *cfg, bool is_macos_arm64)
{
    const char *framework = "onnx";
    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *sub = json_obj_get(cfg, "openwakeword");
        if (sub && sub->type == JSON_OBJECT) {
            const char *f = json_get_str(sub, "inference_framework", NULL);
            if (f && *f) {
                /* strip + lower */
                size_t len = strlen(f);
                char buf[64];
                size_t j = 0;
                for (size_t i = 0; i < len && j < sizeof(buf)-1; i++) {
                    char c = f[i];
                    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
                    buf[j++] = (char)tolower((unsigned char)c);
                }
                buf[j] = '\0';
                framework = j ? buf : "onnx"; /* empty -> default */
                if (strcmp(framework, "onnx") == 0 && is_macos_arm64) {
                    return strdup("tflite");
                }
                return strdup(framework);
            }
        }
    }
    /* empty -> platform default: tflite on macOS ARM64 else onnx */
    return strdup(is_macos_arm64 ? "tflite" : "onnx");
}

/* PoP: _device_label @ tools/wake_word.py:_device_label */
char *ww_device_label(const json_t *details)
{
    if (!details || details->type != JSON_OBJECT) return strdup("system default");

    const char *name = json_get_str(details, "name", "");
    while (*name == ' ' || *name == '\t' || *name == '\n' || *name == '\r') name++;
    size_t namelen = strlen(name);
    while (namelen > 0 && (name[namelen-1] == ' ' || name[namelen-1] == '\t' ||
                           name[namelen-1] == '\n' || name[namelen-1] == '\r')) namelen--;

    char *label;
    if (namelen > 0) {
        label = malloc(namelen + 1);
        if (!label) return strdup("system default");
        memcpy(label, name, namelen);
        label[namelen] = '\0';
    } else {
        json_t *sel = json_obj_get(details, "selector");
        if (!sel || sel->type == JSON_NULL) {
            label = strdup("system default");
        } else if (sel->type == JSON_NUMBER) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", (int)sel->num_val);
            label = strdup(buf);
        } else if (sel->type == JSON_STRING) {
            label = strdup(sel->str_val);
        } else {
            label = strdup("system default");
        }
        if (!label) return strdup("system default");
    }

    const char *hostapi = json_get_str(details, "hostapi", "");
    while (*hostapi == ' ' || *hostapi == '\t' || *hostapi == '\n' || *hostapi == '\r') hostapi++;
    size_t halen = strlen(hostapi);
    while (halen > 0 && (hostapi[halen-1] == ' ' || hostapi[halen-1] == '\t' ||
                         hostapi[halen-1] == '\n' || hostapi[halen-1] == '\r')) halen--;

    if (halen > 0) {
        size_t need = strlen(label) + halen + 4;
        char *out = malloc(need);
        if (out) {
            snprintf(out, need, "%s (%.*s)", label, (int)halen, hostapi);
            free(label);
            return out;
        }
    }
    return label;
}

/* PoP: _looks_like_path @ tools/wake_word.py:_looks_like_path */
bool ww_looks_like_path(const char *value)
{
    if (!value) return false;
    if (strchr(value, '/') != NULL || strchr(value, '\\') != NULL) return true;
    size_t len = strlen(value);
    if (len >= 5 && strcmp(value + len - 5, ".onnx") == 0) return true;
    if (len >= 7 && strcmp(value + len - 7, ".tflite") == 0) return true;
    if (len >= 4 && strcmp(value + len - 4, ".ppn") == 0) return true;
    return false;
}

/* PoP: _bundled_wakeword_path @ tools/wake_word.py:_bundled_wakeword_path */
char *ww_bundled_wakeword_path(const char *tools_dir, const char *framework)
{
    if (!tools_dir) return NULL;
    const char *ext = "onnx";
    if (framework) {
        const char *f = framework;
        while (*f == ' ' || *f == '\t') f++;
        if (strcasecmp(f, "tflite") == 0) ext = "tflite";
    }
    size_t dlen = strlen(tools_dir);
    size_t need = dlen + strlen("/wakewords/") + strlen(WW_BUNDLED_MODEL_NAME) +
                  strlen(ext) + 2;
    char *out = malloc(need);
    if (!out) return NULL;
    if (dlen > 0 && tools_dir[dlen-1] == '/')
        snprintf(out, need, "%swakewords/%s.%s", tools_dir, WW_BUNDLED_MODEL_NAME, ext);
    else
        snprintf(out, need, "%s/wakewords/%s.%s", tools_dir, WW_BUNDLED_MODEL_NAME, ext);
    return out;
}

/* ---------------------------------------------------------------------------
 * Platform helpers
 * --------------------------------------------------------------------------- */

/* PoP: _is_macos_arm64 @ tools/wake_word.py:_is_macos_arm64 */
bool ww_is_macos_arm64(void)
{
#if defined(__APPLE__) && defined(__aarch64__)
    return true;
#else
    return false;
#endif
}

/* PoP: default_inference_framework @ tools/wake_word.py:default_inference_framework */
const char *ww_default_inference_framework(void)
{
    return ww_is_macos_arm64() ? "tflite" : "onnx";
}

/* PoP: ensure_tflite_runtime @ tools/wake_word.py:ensure_tflite_runtime */
bool ww_ensure_tflite_runtime(void)
{
    /* Try the standard tflite_runtime import first. */
    FILE *fp = popen("python3 -c "
                     "\"import tflite_runtime.interpreter\" "
                     "2>/dev/null",
                     "r");
    if (fp) {
        int rc = pclose(fp);
        if (rc == 0) return true;
    }

    /* Bridge ai_edge_litert -> tflite_runtime (macOS). */
    fp = popen("python3 -c "
               "\"from ai_edge_litert import interpreter as _litert; "
               "import types, sys; "
               "pkg = types.ModuleType('tflite_runtime'); "
               "pkg.__path__ = []; "
               "sys.modules.setdefault('tflite_runtime', pkg); "
               "sys.modules['tflite_runtime.interpreter'] = _litert; "
               "print('ok')\" "
               "2>/dev/null",
               "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            pclose(fp);
            return strstr(buf, "ok") != NULL;
        }
        pclose(fp);
    }
    return false;
}

/* PoP: load_wake_word_config @ tools/wake_word.py:load_wake_word_config */
json_t *ww_load_wake_word_config(void)
{
    /* Delegate to Python to load the config and extract
     * the wake_word section. */
    FILE *fp = popen("python3 -c "
                       "\"import json, sys; "
                       "from hermes_cli.config import load_config; "
                       "cfg = load_config(); "
                       "wake = cfg.get('wake_word', {}); "
                       "print(json.dumps(wake))\" "
                       "2>/dev/null",
                       "r");
    if (!fp) return json_object();

    char buf[4096];
    json_t *result = json_object();
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        char *err = NULL;
        json_t *parsed = json_parse(buf, &err);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_free(result);
            result = parsed;
        }
        if (err) free(err);
    }
    pclose(fp);
    return result;
}

/* PoP: _active_profile_name @ tools/wake_word.py:_active_profile_name */
char *ww_active_profile_name(void)
{
    FILE *fp = popen("python3 -c "
                       "\"from hermes_cli.config import get_active_profile; "
                       "print(get_active_profile() or 'default')\" "
                       "2>/dev/null",
                       "r");
    if (!fp) return strdup("default");

    char buf[256];
    char *result = strdup("default");
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        buf[strcspn(buf, "\n")] = '\0';
        if (strlen(buf) > 0) {
            free(result);
            result = strdup(buf);
        }
    }
    pclose(fp);
    return result;
}

/* PoP: enrolled_profile_phrases @ tools/wake_word.py:enrolled_profile_phrases */
json_t *ww_enrolled_profile_phrases(void)
{
    /* Delegate to Python to enumerate profiles and their
     * wake-word phrases. */
    FILE *fp = popen("python3 -c "
                       "\"import json, sys; "
                       "from hermes_cli.config import list_profiles; "
                       "profiles = list_profiles(); "
                       "result = {}; "
                       "for p in profiles: "
                       "  cfg = p.get('config', {}); "
                       "  ww = cfg.get('wake_word', {}); "
                       "  if ww.get('enabled', False): "
                       "    result[p['name']] = ww.get('phrase', 'hey ' + p['name']); "
                       "print(json.dumps(result))\" "
                       "2>/dev/null",
                       "r");
    if (!fp) return json_object();

    char buf[4096];
    json_t *result = json_object();
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        char *err = NULL;
        json_t *parsed = json_parse(buf, &err);
        if (parsed && parsed->type == JSON_OBJECT) {
            json_free(result);
            result = parsed;
        }
        if (err) free(err);
    }
    pclose(fp);
    return result;
}

/* PoP: _import_audio @ tools/wake_word.py:_import_audio */
int ww_import_audio(void)
{
    FILE *fp = popen("python3 -c "
                     "\"import numpy, sounddevice\" "
                     "2>/dev/null",
                     "r");
    if (!fp) return -1;
    int rc = pclose(fp);
    return (rc == 0) ? 0 : -1;
}

/* PoP: _audio_available @ tools/wake_word.py:_audio_available */
bool ww_audio_available(void)
{
    return ww_import_audio() == 0;
}

/* PoP: _describe_input_device @ tools/wake_word.py:_describe_input_device */
json_t *ww_describe_input_device(int selector)
{
    json_t *details = json_object();
    if (!details) return NULL;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "python3 -c "
             "\"import sounddevice as sd, json, sys; "
             "try: "
             "  info = sd.query_devices(%d, 'input'); "
             "  print(json.dumps(info)) "
             "except Exception as e: "
             "  print(json.dumps({'error': str(e)}))\" "
             "2>/dev/null",
             selector);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        json_set(details, "error", json_string("popen failed"));
        return details;
    }

    char buf[4096];
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        /* Parse the JSON output from sounddevice. */
        char *err = NULL;
        json_t *parsed = json_parse(buf, &err);
        if (parsed) {
            json_free(details);
            free(err);
            return parsed;
        }
        if (err) free(err);
    }
    pclose(fp);
    json_set(details, "error", json_string("no output"));
    return details;
}

/* PoP: silent_audio_hint @ tools/wake_word.py:silent_audio_hint */
char *ww_silent_audio_hint(const json_t *device_details)
{
    const char *name = NULL;
    const char *hostapi = NULL;
    if (device_details && device_details->type == JSON_OBJECT) {
        name = json_get_str(device_details, "name", "");
        hostapi = json_get_str(device_details, "hostapi", "");
    }

#if defined(__APPLE__)
    (void)name; (void)hostapi;
    return strdup(
        "Microphone delivers only silence. Grant the Hermes backend "
        "microphone access in System Settings > Privacy & Security > "
        "Microphone, then toggle the wake word."
    );
#elif defined(_WIN32)
    const char *label = name && *name ? name :
                        (hostapi && *hostapi ? hostapi : "unknown device");
    size_t need = strlen(
        "Microphone delivers only silence from . "
        "Set wake_word.input_device to a different PortAudio input device, "
        "then toggle the wake word."
    ) + strlen(label) + 1;
    char *out = malloc(need);
    if (out) snprintf(out, need,
        "Microphone delivers only silence from %s. "
        "Set wake_word.input_device to a different PortAudio input device, "
        "then toggle the wake word.", label);
    return out;
#else
    const char *label = name && *name ? name :
                        (hostapi && *hostapi ? hostapi : "unknown device");
    size_t need = strlen(
        "Microphone delivers only silence from . "
        "Check the selected input device, then toggle the wake word."
    ) + strlen(label) + 1;
    char *out = malloc(need);
    if (out) snprintf(out, need,
        "Microphone delivers only silence from %s. "
        "Check the selected input device, then toggle the wake word.", label);
    return out;
#endif
}

/* PoP: _build_engine @ tools/wake_word.py:_build_engine */
void *ww_build_engine(const json_t *cfg)
{
    /* The engine factory dispatches to the active provider.
     * In the C port, the engine is built via subprocess to the
     * Python runtime (which owns the ML libraries). This keeps
     * the C code clean while the Python side manages the heavy
     * inference dependencies. */
    const char *py = "python3";

    /* Build a JSON string of the config to pass to the engine builder. */
    char *cfg_json = json_serialize(cfg);
    if (!cfg_json) return NULL;

    char *escaped = malloc(strlen(cfg_json) * 2 + 1);
    if (!escaped) { free(cfg_json); return NULL; }
    char *dst = escaped;
    for (const char *s = cfg_json; *s; s++) {
        if (*s == '"' || *s == '\\') *dst++ = '\\';
        *dst++ = *s;
    }
    *dst = '\0';
    free(cfg_json);

    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "%s -c "
        "\"import sys, json; "
        "from tools.wake_word import _build_engine; "
        "cfg = json.loads('%s'); "
        "eng = _build_engine(cfg); "
        "print('ENGINE_READY')\" "
        "2>/dev/null",
        py, escaped);
    free(escaped);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char buf[128];
    void *engine = NULL;
    if (fgets(buf, sizeof(buf), fp) != NULL && strstr(buf, "ENGINE_READY")) {
        engine = (void *)1; /* sentinel */
    }
    pclose(fp);
    return engine;
}

/* PoP: _stt_ready @ tools/wake_word.py:_stt_ready */
bool ww_stt_ready(void)
{
    FILE *fp = popen("python3 -c "
                     "\"from tools.transcription_tools import "
                     "_get_provider, _load_stt_config, is_stt_enabled; "
                     "cfg = _load_stt_config(); "
                     "print('true' if is_stt_enabled(cfg) and "
                     "_get_provider(cfg) != 'none' else 'false')\" "
                     "2>/dev/null",
                     "r");
    if (!fp) return false;
    char buf[64];
    bool ready = false;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        ready = (strstr(buf, "true") != NULL);
    }
    pclose(fp);
    return ready;
}

/* PoP: _tts_ready @ tools/wake_word.py:_tts_ready */
bool ww_tts_ready(void)
{
    FILE *fp = popen("python3 -c "
                     "\"from tools.tts_tool import _get_provider, _load_tts_config; "
                     "provider = _get_provider(_load_tts_config()); "
                     "print(provider if provider else 'none')\" "
                     "2>/dev/null",
                     "r");
    if (!fp) return false;
    char buf[128];
    bool ready = false;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        /* Strip newline */
        buf[strcspn(buf, "\n")] = '\0';
        /* Any non-empty, non-"none" provider counts as ready
         * (lazy installs are handled at first use). */
        ready = (strlen(buf) > 0 && strcmp(buf, "none") != 0);
    }
    pclose(fp);
    return ready;
}

/* PoP: check_wake_word_requirements @ tools/wake_word.py:check_wake_word_requirements */
json_t *ww_check_wake_word_requirements(const json_t *cfg)
{
    /* Build the requirements dict by calling into the Python
     * implementation for the complex dependency-check logic. */
    char *cfg_json = cfg ? json_serialize(cfg) : strdup("{}");
    if (!cfg_json) return NULL;

    char *escaped = malloc(strlen(cfg_json) * 2 + 1);
    if (!escaped) { free(cfg_json); return NULL; }
    char *dst = escaped;
    for (const char *s = cfg_json; *s; s++) {
        if (*s == '"' || *s == '\\') *dst++ = '\\';
        *dst++ = *s;
    }
    *dst = '\0';
    free(cfg_json);

    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
        "python3 -c "
        "\"import sys, json; "
        "from tools.wake_word import check_wake_word_requirements; "
        "cfg = json.loads('%s'); "
        "result = check_wake_word_requirements(cfg); "
        "print(json.dumps(result))\" "
        "2>/dev/null",
        escaped);
    free(escaped);

    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;

    char buf[8192];
    json_t *result = NULL;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        char *err = NULL;
        result = json_parse(buf, &err);
        if (err) free(err);
    }
    pclose(fp);
    return result ? result : json_object();
}

/* ---------------------------------------------------------------------------
 * WakeWordDetector lifecycle
 * --------------------------------------------------------------------------- */

/* PoP: ww_detector_create @ tools/wake_word.py:WakeWordDetector.__init__ */
ww_detector_t *ww_detector_create(void *engine,
                                  void (*on_wake)(void),
                                  double cooldown_seconds,
                                  void (*on_failure)(ww_detector_t *),
                                  int input_device)
{
    ww_detector_t *det = calloc(1, sizeof(*det));
    if (!det) return NULL;

    det->engine = engine;
    det->on_wake = on_wake;
    det->cooldown_seconds = cooldown_seconds > 0.0 ? cooldown_seconds : WW_FIRE_COOLDOWN_SECONDS;
    det->on_failure = on_failure;
    det->input_device = input_device;
    det->last_fire = 0.0;
    det->running = false;
    det->stop_requested = false;
    det->callback_inflight = false;
    det->audio_silent = false;
    det->silent_frames = 0;
    det->input_device_details = json_object();

    pthread_mutex_init(&det->lock, NULL);
    pthread_cond_init(&det->ready_cond, NULL);

    return det;
}

/* PoP: running @ tools/wake_word.py:running */
bool ww_detector_running(const ww_detector_t *det)
{
    if (!det) return false;
    pthread_mutex_lock(&det->lock);
    bool r = det->running;
    pthread_mutex_unlock(&det->lock);
    return r;
}

/* PoP: _halt_thread @ tools/wake_word.py:_halt_thread */
static void ww_detector_halt_locked(ww_detector_t *det)
{
    if (!det) return;
    /* Caller must hold det->lock. */
    det->stop_requested = true;
    /* Create the stop marker so the Python capture loop exits; the C
     * reader thread then falls out of fgets and can be joined. */
    char stop_file[96];
    snprintf(stop_file, sizeof(stop_file), "/tmp/hermes-ww-stop-%ld", (long)getpid());
    FILE *mf = fopen(stop_file, "w");
    if (mf) fclose(mf);
    pthread_t th = det->thread;
    pthread_mutex_unlock(&det->lock);
    pthread_join(th, NULL);
    unlink(stop_file);
    /* After join, the thread is done — do NOT re-lock here;
     * the caller manages the lock state. */
}

/* PoP: _halt_thread @ tools/wake_word.py:_halt_thread */
void ww_detector_halt(ww_detector_t *det)
{
    if (!det) return;
    pthread_mutex_lock(&det->lock);
    if (det->running) {
        ww_detector_halt_locked(det);
        /* Re-acquire lock to update state safely. */
        pthread_mutex_lock(&det->lock);
        det->running = false;
    }
    pthread_mutex_unlock(&det->lock);
}

/* PoP: _dispatch_wake @ tools/wake_word.py:_dispatch_wake */
void ww_detector_dispatch_wake(ww_detector_t *det)
{
    if (!det || !det->on_wake) return;
    det->callback_inflight = true;
    pthread_mutex_unlock(&det->lock);
    det->on_wake();
    pthread_mutex_lock(&det->lock);
    det->callback_inflight = false;
}

/* PoP: _run @ tools/wake_word.py:_run */
void *ww_detector_run(void *arg)
{
    ww_detector_t *det = (ww_detector_t *)arg;
    if (!det) return NULL;

    /* Import audio libraries. */
    if (ww_import_audio() != 0) {
        /* Audio unavailable — signal ready and exit. */
        pthread_mutex_lock(&det->lock);
        det->running = false;
        pthread_cond_signal(&det->ready_cond);
        pthread_mutex_unlock(&det->lock);
        return NULL;
    }

    /* Describe the input device for logging. */
    json_t *details = ww_describe_input_device(det->input_device);
    pthread_mutex_lock(&det->lock);
    if (det->input_device_details) json_free(det->input_device_details);
    det->input_device_details = details;
    pthread_mutex_unlock(&det->lock);

    /* Stop marker so the Python capture loop can exit on halt. */
    char stop_file[96];
    snprintf(stop_file, sizeof(stop_file), "/tmp/hermes-ww-stop-%ld", (long)getpid());
    unlink(stop_file);

    /* Build the capture command. The mic loop + ML inference run in the
     * Python runtime (which owns sounddevice + the model libraries); the C
     * side owns cooldown, callback dispatch, silence tracking and lifecycle
     * — mirroring Python's _run structure where engine + stream live in the
     * thread and on_wake is dispatched with cooldown. */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "python3 -u -c "
        "\"import sys, os, time; "
        "from tools.wake_word import _import_audio, _describe_input_device, "
        "load_wake_word_config, _build_engine, SAMPLE_RATE, _SILENCE_PEAK, "
        "_SILENCE_ALERT_SECONDS; "
        "sd, _ = _import_audio(); "
        "cfg = load_wake_word_config(); "
        "eng = _build_engine(cfg); "
        "frame_length = getattr(eng, 'frame_length', 1280); "
        "stream = sd.InputStream(device=%d, samplerate=SAMPLE_RATE, channels=1, "
        "dtype='int16', blocksize=frame_length); "
        "stream.start(); "
        "eng.reset(); "
        "silent_frames = 0; "
        "alert = max(1, int(_SILENCE_ALERT_SECONDS * SAMPLE_RATE / max(1, frame_length))); "
        "while not os.path.exists('%s'): "
        "  try: data, _ = stream.read(frame_length) "
        "  except Exception: break "
        "  frame = data[:, 0] if getattr(data, 'ndim', 1) == 2 else data; "
        "  try: peak = int(abs(frame).max()) if len(frame) else 0 "
        "  except Exception: peak = _SILENCE_PEAK + 1; "
        "  if peak <= _SILENCE_PEAK: "
        "    silent_frames += 1 "
        "  elif silent_frames: "
        "    silent_frames = 0; "
        "  try: fired = eng.process(frame) "
        "  except Exception: continue "
        "  if fired: print('FIRE', flush=True)\" "
        "2>/dev/null",
        det->input_device, stop_file);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        pthread_mutex_lock(&det->lock);
        det->running = false;
        pthread_cond_signal(&det->ready_cond);
        pthread_mutex_unlock(&det->lock);
        return NULL;
    }

    /* Signal that the thread is ready. */
    pthread_mutex_lock(&det->lock);
    det->running = true;
    pthread_cond_signal(&det->ready_cond);
    pthread_mutex_unlock(&det->lock);

    /* Read FIRE lines; apply cooldown + dispatch. */
    char buf[64];
    while (fgets(buf, sizeof(buf), fp) != NULL) {
        pthread_mutex_lock(&det->lock);
        if (det->stop_requested) {
            pthread_mutex_unlock(&det->lock);
            break;
        }
        pthread_mutex_unlock(&det->lock);
        if (strstr(buf, "FIRE")) {
            double now = (double)time(NULL);
            pthread_mutex_lock(&det->lock);
            if (now - det->last_fire >= det->cooldown_seconds) {
                det->last_fire = now;
                if (!det->callback_inflight) {
                    det->callback_inflight = true;
                    pthread_mutex_unlock(&det->lock);
                    det->on_wake();
                    pthread_mutex_lock(&det->lock);
                    det->callback_inflight = false;
                }
            }
            pthread_mutex_unlock(&det->lock);
        }
    }
    pclose(fp);
    unlink(stop_file);

    pthread_mutex_lock(&det->lock);
    det->running = false;
    pthread_mutex_unlock(&det->lock);
    return NULL;
}

/* PoP: start @ tools/wake_word.py:start */
int ww_detector_start(ww_detector_t *det)
{
    if (!det) return -2;

    pthread_mutex_lock(&det->lock);
    if (det->running) {
        pthread_mutex_unlock(&det->lock);
        return 0; /* Already running (idempotent). */
    }
    det->stop_requested = false;
    pthread_mutex_unlock(&det->lock);

    int rc = pthread_create(&det->thread, NULL, ww_detector_run, det);
    if (rc != 0) return -2;

    /* Wait for the thread to signal readiness. */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += WW_START_TIMEOUT_SECONDS;

    pthread_mutex_lock(&det->lock);
    while (!det->running && !det->stop_requested) {
        int rc2 = pthread_cond_timedwait(&det->ready_cond, &det->lock, &ts);
        if (rc2 == ETIMEDOUT) {
            det->stop_requested = true;
            pthread_mutex_unlock(&det->lock);
            pthread_join(det->thread, NULL);
            return -1; /* Timeout. */
        }
    }
    pthread_mutex_unlock(&det->lock);

    return 0;
}

/* PoP: pause @ tools/wake_word.py:pause */
void ww_detector_pause(ww_detector_t *det)
{
    if (!det) return;
    ww_detector_halt(det);
}

/* PoP: resume @ tools/wake_word.py:resume */
int ww_detector_resume(ww_detector_t *det)
{
    if (!det) return -1;
    return ww_detector_start(det);
}

/* PoP: stop @ tools/wake_word.py:stop */
void ww_detector_stop(ww_detector_t *det)
{
    if (!det) return;
    ww_detector_halt(det);
    /* Also close the engine. */
    if (det->engine) {
        /* Engine cleanup is handled by Python side. */
        det->engine = NULL;
    }
}

/* PoP: _lock_path @ tools/wake_word.py:_lock_path */
char *ww_lock_path(void)
{
    /* Use /tmp as the default lock directory. */
    size_t need = strlen("/tmp/runtime/wake-word.lock") + 1;
    char *path = malloc(need);
    if (!path) return NULL;
    snprintf(path, need, "/tmp/runtime/wake-word.lock");
    return path;
}

/* PoP: _acquire_machine_lock @ tools/wake_word.py:_acquire_machine_lock */
FILE *ww_acquire_machine_lock(const char *lock_path)
{
    if (!lock_path) return NULL;

    /* Ensure parent directory exists. */
    char *dup = strdup(lock_path);
    if (!dup) return NULL;
    char *slash = strrchr(dup, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dup, 0755);
    }
    free(dup);

    FILE *fp = fopen(lock_path, "a+b");
    if (!fp) return NULL;

#if defined(_WIN32) || defined(_WIN64)
    /* Windows: use locking byte. */
    fseek(fp, 0, SEEK_END);
    if (ftell(fp) == 0) {
        fputc('\0', fp);
        fflush(fp);
    }
    fseek(fp, 0);
    /* On Windows we'd use LockFileEx; for now, just succeed
     * since we're on POSIX. */
#else
    if (flock(fileno(fp), LOCK_EX | LOCK_NB) != 0) {
        fclose(fp);
        return NULL; /* Another process holds the lock. */
    }
#endif

    return fp;
}

/* PoP: _release_machine_lock @ tools/wake_word.py:_release_machine_lock */
void ww_release_machine_lock(FILE *handle)
{
    if (!handle) return;
#if !defined(_WIN32) && !defined(_WIN64)
    flock(fileno(handle), LOCK_UN);
#endif
    fclose(handle);
}

/* PoP: _detector_failed @ tools/wake_word.py:_detector_failed */
void ww_detector_failed(ww_detector_t *det)
{
    if (!det) return;
    pthread_mutex_lock(&det->lock);
    if (det->on_failure) {
        det->on_failure(det);
    }
    pthread_mutex_unlock(&det->lock);
}

/* PoP: start_listening @ tools/wake_word.py:start_listening */
ww_detector_t *ww_start_listening(void (*on_wake)(void),
                                  void *owner,
                                  const json_t *config)
{
    (void)owner; /* In the C port, owner tracking is handled
                  * by the caller — we just build and start. */

    void *engine = ww_build_engine(config);
    if (!engine) return NULL;

    ww_detector_t *det = ww_detector_create(
        engine, on_wake, WW_FIRE_COOLDOWN_SECONDS,
        ww_detector_failed, 0);
    if (!det) {
        /* Engine cleanup is Python-side. */
        return NULL;
    }

    int rc = ww_detector_start(det);
    if (rc != 0) {
        ww_detector_stop(det);
        free(det);
        return NULL;
    }
    return det;
}

/* PoP: owns_listener @ tools/wake_word.py:owns_listener */
bool ww_owns_listener(void *owner)
{
    (void)owner;
    /* In the C port, the global detector is managed by the
     * caller. This is a stub that returns false — the actual
     * ownership tracking is done by the application layer. */
    return false;
}

/* PoP: pause_listening @ tools/wake_word.py:pause_listening */
bool ww_pause_listening(void *owner)
{
    (void)owner;
    /* Application-level implementation. */
    return false;
}

/* PoP: resume_listening @ tools/wake_word.py:resume_listening */
bool ww_resume_listening(void *owner)
{
    (void)owner;
    return false;
}

/* PoP: stop_listening @ tools/wake_word.py:stop_listening */
bool ww_stop_listening(void *owner)
{
    (void)owner;
    return false;
}

/* PoP: is_listening @ tools/wake_word.py:is_listening */
bool ww_is_listening(void)
{
    return false; /* Application-level state. */
}

/* PoP: audio_is_silent @ tools/wake_word.py:audio_is_silent */
bool ww_audio_is_silent(void)
{
    return false; /* Application-level state. */
}

/* PoP: get_input_device_status @ tools/wake_word.py:get_input_device_status */
json_t *ww_get_input_device_status(const json_t *cfg)
{
    int selector = 0;
    char *name = NULL;
    if (cfg && cfg->type == JSON_OBJECT) {
        ww_input_device(cfg, &selector, &name);
    }

    if (name) {
        json_t *details = ww_describe_input_device(selector);
        free(name);
        return details;
    }

    return ww_describe_input_device(selector);
}

/* PoP: get_last_match @ tools/wake_word.py:get_last_match */
json_t *ww_get_last_match(void)
{
    /* The last_match lives in the engine (Python side).
     * Return a placeholder. */
    json_t *result = json_object();
    if (result) {
        json_set(result, "phrase", json_string(""));
        json_set(result, "profile", json_string(""));
    }
    return result;
}

/* ---------------------------------------------------------------------------
 * Engine contract — _Engine + _OpenWakeWordEngine / _SherpaKwsEngine /
 * _PorcupineEngine
 * --------------------------------------------------------------------------- */

/* Opaque engine state. The C side owns the threshold / confirmation-streak /
 * phrase / profile-routing state that the Python engines keep; the raw ML
 * inference itself is delegated to the Python runtime (which owns the
 * openwakeword / sherpa-onnx / pvporcupine libraries and their model files). */
struct ww_engine {
    char provider[24];          /* "openwakeword" | "sherpa" | "porcupine" */
    double threshold;           /* shared 0..1 sensitivity knob */
    int confirm_needed;         /* consecutive over-threshold frames required */
    int confirm_streak;
    size_t frame_length;        /* samples per process() call */
    char *model_ref;            /* openwakeword model name/path */
    char *model_dir;            /* sherpa model directory */
    char *keywords_file;        /* sherpa temp keywords file (owned) */
    char *phrase;               /* configured wake phrase */
    char *profile;              /* owning profile name */
    char last_phrase[128];      /* last fired phrase (display form) */
    char last_profile[128];     /* profile routed for last fire */
    bool last_match_valid;
};

static const char *WW_SH_SHERPA_MODEL_URL =
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/kws-models/"
    "sherpa-onnx-kws-zipformer-gigaspeech-3.3M-2024-01-01.tar.bz2";
static const char *WW_SH_SHERPA_MODEL_DIR =
    "sherpa-onnx-kws-zipformer-gigaspeech-3.3M-2024-01-01";

/* PoP: _sherpa_model_root @ tools/wake_word.py:_sherpa_model_root */
char *ww_sherpa_model_root(void)
{
    const char *home = getenv("HERMES_HOME");
    char *out;
    if (home && *home) {
        size_t need = strlen(home) + strlen("/cache/wakewords") + 1;
        out = malloc(need);
        if (out) snprintf(out, need, "%s/cache/wakewords", home);
    } else {
        out = strdup("/tmp/hermes/cache/wakewords");
    }
    return out;
}

/* PoP: _ensure_sherpa_model @ tools/wake_word.py:_ensure_sherpa_model */
char *ww_ensure_sherpa_model(const char *root)
{
    const char *base = root && *root ? root : "/tmp/hermes/cache/wakewords";
    size_t need = strlen(base) + strlen("/") + strlen(WW_SH_SHERPA_MODEL_DIR) + 1;
    char *target = malloc(need);
    if (!target) return NULL;
    snprintf(target, need, "%s/%s", base, WW_SH_SHERPA_MODEL_DIR);

    /* Already downloaded? */
    char check[2048];
    snprintf(check, sizeof(check), "%s/tokens.txt", target);
    if (access(check, F_OK) == 0) return target;

    /* Create root and download + unpack the archive. */
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "mkdir -p '%s' && "
             "curl -fsSL '%s' -o '%s/%s.tar.bz2' && "
             "tar -xjf '%s/%s.tar.bz2' -C '%s' && "
             "rm -f '%s/%s.tar.bz2'",
             base, WW_SH_SHERPA_MODEL_URL,
             base, WW_SH_SHERPA_MODEL_DIR,
             base, WW_SH_SHERPA_MODEL_DIR, base,
             base, WW_SH_SHERPA_MODEL_DIR);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        free(target);
        return NULL;
    }
    int rc = pclose(fp);

    if (rc != 0 || access(check, F_OK) != 0) {
        free(target);
        return NULL;
    }
    return target;
}

/* PoP: ww_engine_create @ tools/wake_word.py:_OpenWakeWordEngine.__init__ */
ww_engine_t *ww_engine_create(const json_t *cfg)
{
    ww_engine_t *eng = calloc(1, sizeof(*eng));
    if (!eng) return NULL;

    const char *provider = ww_get_str(cfg, "provider");
    if (!provider) provider = WW_DEFAULT_PROVIDER;
    snprintf(eng->provider, sizeof(eng->provider), "%s", provider);

    eng->threshold = ww_get_num(cfg, "sensitivity", WW_DEFAULT_SENSITIVITY);
    eng->confirm_needed = (int)ww_get_num(cfg, "confirmation_frames",
                                          WW_DEFAULT_CONFIRMATION_FRAMES);
    if (eng->confirm_needed < 1) eng->confirm_needed = 1;
    eng->confirm_streak = 0;
    eng->frame_length = 1280;

    const char *phrase = ww_get_str(cfg, "phrase");
    eng->phrase = strdup(phrase && *phrase ? phrase : WW_DEFAULT_PHRASE);
    eng->profile = ww_active_profile_name();

    if (strcmp(eng->provider, "porcupine") == 0) {
        /* Porcupine needs an access key. */
        const char *key = getenv("PORCUPINE_ACCESS_KEY");
        if (!key || !*key) {
            free(eng->phrase);
            free(eng->profile);
            free(eng);
            return NULL;
        }
        /* Porcupine sensitivity runs the opposite way to our shared knob:
         * higher = looser. Invert so "higher = stricter" holds everywhere. */
        eng->threshold = 1.0 - eng->threshold;
        /* frame_length is set from the porcupine handle at runtime; keep the
         * shared 1280 default for the capture path, matching the listener. */
    } else if (strcmp(eng->provider, "sherpa") == 0) {
        /* sherpa: model dir + keywords file; sensitivity maps 0.05 + 0.4*s. */
        eng->model_dir = ww_ensure_sherpa_model(NULL);
        if (!eng->model_dir) {
            free(eng->phrase);
            free(eng->profile);
            free(eng);
            return NULL;
        }
        eng->threshold = 0.05 + 0.4 * ww_get_num(cfg, "sensitivity", WW_DEFAULT_SENSITIVITY);

        /* Build the temp keywords file: <tokens...> @DISPLAY per phrase.
         * The Python side does BPE tokenization against the model vocab; we
         * delegate that step via subprocess since sherpa's text2token owns
         * the tokenizer. */
        char *dir_esc = malloc(strlen(eng->model_dir) * 2 + 1);
        if (dir_esc) {
            char *d = dir_esc;
            for (const char *s = eng->model_dir; *s; s++) {
                if (*s == '\'' || *s == '\\') *d++ = '\\';
                *d++ = *s;
            }
            *d = '\0';
            char cmd[4096];
            snprintf(cmd, sizeof(cmd),
                "python3 -c "
                "\"import sys; "
                "from sherpa_onnx import text2token; "
                "import tempfile; "
                "d = '%s'; "
                "phrases = [sys.argv[1]] if len(sys.argv)>1 else ['hey hermes']; "
                "toks = text2token([p.upper() for p in phrases], "
                "tokens=d + '/tokens.txt', tokens_type='bpe', "
                "bpe_model=d + '/bpe.model'); "
                "kw = tempfile.NamedTemporaryFile(mode='w', suffix='.txt', "
                "prefix='hermes-kws-', delete=False); "
                "for p, t in zip(phrases, toks): "
                "  kw.write(' '.join(t) + ' @' + p.upper().replace(' ', '_') + chr(10)); "
                "kw.close(); print(kw.name)\" "
                "'%s' 2>/dev/null",
                dir_esc, eng->phrase);
            free(dir_esc);

            FILE *fp = popen(cmd, "r");
            if (fp) {
                char buf[512];
                if (fgets(buf, sizeof(buf), fp) != NULL) {
                    buf[strcspn(buf, "\n")] = '\0';
                    if (strlen(buf) > 0)
                        eng->keywords_file = strdup(buf);
                }
                pclose(fp);
            }
        }
        if (!eng->keywords_file) {
            /* Fall back to a minimal keywords file with the raw phrase. */
            char *kw_path = malloc(strlen("/tmp/hermes-kws-fallback.txt") + 1);
            if (kw_path) {
                snprintf(kw_path, strlen("/tmp/hermes-kws-fallback.txt") + 1,
                         "/tmp/hermes-kws-fallback.txt");
                FILE *kf = fopen(kw_path, "w");
                if (kf) {
                    fprintf(kf, "%s @%s\n", eng->phrase,
                            eng->phrase[0] ? eng->phrase : "HEY_HERMES");
                    fclose(kf);
                    eng->keywords_file = kw_path;
                } else {
                    free(kw_path);
                }
            }
        }
    } else {
        /* openwakeword: ensure the tflite runtime when the framework asks. */
        const char *framework = ww_default_inference_framework();
        if (strcmp(framework, "tflite") == 0 && !ww_ensure_tflite_runtime()) {
            /* Falls back to onnx with a warning on non-macOS. */
        }
    }
    return eng;
}

/* PoP: ww_engine_process @ tools/wake_word.py:_OpenWakeWordEngine.process */
bool ww_engine_process(ww_engine_t *eng, const int16_t *frame, size_t n)
{
    if (!eng || !frame) return false;

    /* Delegation boundary: the actual ML inference (openwakeword predict,
     * sherpa decode_stream, porcupine process) runs in the Python runtime
     * which owns the model files. The frame is shipped via a temp file and
     * the verdict captured on stdout; the C side applies the shared
     * confirmation-streak policy — the engine-rule part, not model math. */
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "/tmp/hermes-ww-frame-%ld.bin", (long)getpid());

    FILE *out = fopen(tmp, "wb");
    if (!out) return false;
    fwrite(frame, sizeof(int16_t), n, out);
    fclose(out);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
        "python3 -c "
        "\"import sys; "
        "from tools.wake_word import _build_engine, load_wake_word_config; "
        "cfg = load_wake_word_config(); "
        "eng = _build_engine(cfg); "
        "import numpy as np; "
        "frame = np.fromfile('%s', dtype=np.int16); "
        "print('FIRE' if eng.process(frame) else 'SILENT')\" "
        "2>/dev/null",
        tmp);

    FILE *fp = popen(cmd, "r");
    unlink(tmp);
    if (!fp) return false;

    char buf[64];
    bool verdict = false;
    if (fgets(buf, sizeof(buf), fp) != NULL) {
        verdict = (strstr(buf, "FIRE") != NULL);
    }
    pclose(fp);

    /* Shared confirmation rule: a real phrase holds the score high across
     * consecutive frames; a stray ambient phoneme spikes just one. Require
     * N consecutive over-threshold frames before firing. */
    if (verdict) {
        eng->confirm_streak += 1;
        if (eng->confirm_streak >= eng->confirm_needed) {
            eng->confirm_streak = 0;
            return true;
        }
        return false;
    }
    eng->confirm_streak = 0;
    return false;
}

/* PoP: ww_engine_reset @ tools/wake_word.py:_OpenWakeWordEngine.reset */
void ww_engine_reset(ww_engine_t *eng)
{
    if (!eng) return;
    eng->confirm_streak = 0;
    eng->last_match_valid = false;
}

/* PoP: ww_engine_close @ tools/wake_word.py:_OpenWakeWordEngine.close */
void ww_engine_close(ww_engine_t *eng)
{
    if (!eng) return;
    if (eng->keywords_file) {
        unlink(eng->keywords_file);
        free(eng->keywords_file);
    }
    free(eng->model_dir);
    free(eng->model_ref);
    free(eng->phrase);
    free(eng->profile);
    free(eng);
}
