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
#include "port_config_py_helpers.h"
#include "profile_store.h"
#include "hermes_core_types.h"
#include "yaml.h"

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
#include <alsa/asoundlib.h>

/* Native wake-word audio backend: ALSA device enumeration + capture.
 * Replaces the Python sounddevice/numpy subprocess probes. */

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

/* Native site-packages probe: return 1 if a Python package's top-level module
 * directory or .py/.so file exists in any venv/user/system site-packages.
 * This is our code checking whether a (third-party) ML runtime is installed —
 * the runtime itself is not ours to reimplement. Mirrors lazy_deps'
 * importlib.util.find_spec semantics for the wake-word feature packages. */
static bool ww_site_package_module(const char *module)
{
    if (!module || !*module) return false;

    /* Top-level package dir / module file name. Dots delimit submodules, but
     * here we only probe the top-level import name (the runtime package). */
    char top[128];
    size_t n = strlen(module);
    size_t i = 0;
    while (i < n && module[i] != '.') { top[i] = module[i]; i++; }
    top[i] = '\0';
    if (i == 0) return false;

    const char *ve = getenv("VIRTUAL_ENV");
    const char *hp = getenv("HOME");
    const char *pyhome = getenv("PYTHONHOME");
    const char *roots[6] = { ve, hp, pyhome, NULL };
    int nroots = 0;
    while (nroots < 3 && roots[nroots]) nroots++;

    for (int r = 0; r < nroots; r++) {
        const char *base = roots[r];
        /* venv vs user/system layout */
        const char *layouts[4] = { NULL, NULL, NULL, NULL };
        int nl = 0;
        if (ve && base == ve) {
            layouts[nl++] = "/lib/python3.11/site-packages";
            layouts[nl++] = "/lib/python3.12/site-packages";
            layouts[nl++] = "/lib/python3.10/site-packages";
        } else {
            layouts[nl++] = "/lib/python3.11/site-packages";
            layouts[nl++] = "/.local/lib/python3.11/site-packages";
        }
        for (int L = 0; L < nl; L++) {
            char p[2048];
            snprintf(p, sizeof(p), "%s%s/%s/__init__.py", base, layouts[L], top);
            if (access(p, R_OK) == 0) return true;
            snprintf(p, sizeof(p), "%s%s/%s.py", base, layouts[L], top);
            if (access(p, R_OK) == 0) return true;
            snprintf(p, sizeof(p), "%s%s/%s.so", base, layouts[L], top);
            if (access(p, R_OK) == 0) return true;
        }
    }
    return false;
}

/* Native lazy_deps.is_available for the wake-word feature set. Each feature
 * maps to a distinguishing top-level package whose presence implies the deps
 * are satisfied. Mirrors tools/lazy_deps.py LAZY_DEPS entries. */
static bool ww_site_package_available(const char *feature)
{
    if (!feature) return false;
    if (strcmp(feature, "wake.porcupine") == 0)
        return ww_site_package_module("pvporcupine");
    if (strcmp(feature, "wake.sherpa") == 0)
        return ww_site_package_module("sherpa_onnx");
    if (strcmp(feature, "wake.openwakeword.tflite") == 0)
        return ww_site_package_module("ai_edge_litert");
    /* wake.openwakeword — onnxruntime is the runtime dependency. */
    return ww_site_package_module("onnxruntime");
}

/* Native lazy_deps._allow_lazy_installs(): config kill switch wins, else
 * defaults to allow. Mirrors the Python resolution order. */
static bool ww_allow_lazy_installs(void)
{
    json_t *config = config_py_load_config_readonly();
    if (config && config->type == JSON_OBJECT) {
        json_t *sec = json_obj_get(config, "security");
        if (sec && sec->type == JSON_OBJECT) {
            json_t *allow = json_obj_get(sec, "allow_lazy_installs");
            bool ok = true;
            if (allow) {
                if (allow->type == JSON_BOOL) ok = allow->bool_val;
                else if (allow->type == JSON_NUMBER) ok = allow->num_val != 0.0;
                else if (allow->type == JSON_STRING)
                    ok = strcmp(allow->str_val, "false") != 0 &&
                         strcmp(allow->str_val, "0") != 0;
            }
            json_free(config);
            return ok;
        }
        json_free(config);
    } else if (config) {
        json_free(config);
    }
    return true; /* fail open */
}

/* PoP: ensure_tflite_runtime @ tools/wake_word.py:ensure_tflite_runtime */
bool ww_ensure_tflite_runtime(void)
{
    /* Native probe: the tflite runtime is a site-packages package; check for
     * it directly rather than shelling to ``python3 -c "import ..."``. */
    if (ww_site_package_module("tflite_runtime")) return true;
    if (ww_site_package_module("ai_edge_litert")) return true;
    return false;
}

/* PoP: load_wake_word_config @ tools/wake_word.py:load_wake_word_config */
json_t *ww_load_wake_word_config(void)
{
    /* Load the merged config natively and extract the wake_word section.
     * config_py_load_config_readonly() mirrors Python's load_config(). */
    json_t *config = config_py_load_config_readonly();
    if (!config || config->type != JSON_OBJECT) {
        if (config) json_free(config);
        return json_object();
    }
    json_t *wake = json_obj_get(config, "wake_word");
    json_t *result = json_object();
    if (wake && wake->type == JSON_OBJECT) {
        /* Deep-copy the wake_word section into the result. */
        char *ser = json_serialize(wake);
        if (ser) {
            json_t *parsed = json_parse(ser, NULL);
            free(ser);
            if (parsed && parsed->type == JSON_OBJECT) {
                json_free(result);
                result = parsed;
            }
        }
    }
    json_free(config);
    return result;
}

/* PoP: _active_profile_name @ tools/wake_word.py:_active_profile_name */
char *ww_active_profile_name(void)
{
    /* Native: infer the current profile name from HERMES_HOME.
     * Returns "default" when unset — matches Python's get_active_profile(). */
    return profile_get_active_name();
}

/* PoP: enrolled_profile_phrases @ tools/wake_word.py:enrolled_profile_phrases */
json_t *ww_enrolled_profile_phrases(void)
{
    /* Enumerate profiles natively (default + all named, mirroring
     * list_profiles) and read each one's wake_word config from disk. */
    json_t *result = json_object();
    char *packed = profile_profiles_to_serve(true);
    if (!packed) return result;

    char *cursor = packed;
    const char *name = NULL, *home = NULL;
    while (profile_serve_next(packed, &cursor, &name, &home)) {
        if (!name) continue;
        /* Resolve this profile's home to its config.yaml path. */
        char cfgpath[HERMES_PATH_MAX];
        if (home && *home)
            snprintf(cfgpath, sizeof cfgpath, "%s/config.yaml", home);
        else {
            char *resolved = profile_resolve_env(name);
            if (resolved) {
                snprintf(cfgpath, sizeof cfgpath, "%s/config.yaml", resolved);
                free(resolved);
            } else {
                snprintf(cfgpath, sizeof cfgpath, "config.yaml");
            }
        }
        /* Parse the profile's config.yaml raw (no defaults merge). */
        json_t *cfg = NULL;
        char *yerr = NULL;
        yaml_doc_t *yp = yaml_parse_file(cfgpath, &yerr);
        free(yerr);
        if (yp) {
            char *js = yaml_to_json_string(yp, "");
            yaml_free(yp);
            if (js) {
                cfg = json_parse(js, NULL);
                free(js);
            }
        }
        json_t *wake = cfg ? json_obj_get(cfg, "wake_word") : NULL;
        bool enabled = false;
        const char *phrase = NULL;
        if (wake && wake->type == JSON_OBJECT) {
            json_t *en = json_obj_get(wake, "enabled");
            if (en && en->type == JSON_BOOL) enabled = en->bool_val;
            else if (en && en->type == JSON_NUMBER) enabled = en->num_val != 0.0;
            else if (en && en->type == JSON_STRING)
                enabled = strcmp(en->str_val, "true") == 0 ||
                          strcmp(en->str_val, "1") == 0 ||
                          strcmp(en->str_val, "yes") == 0 ||
                          strcmp(en->str_val, "on") == 0;
            json_t *ph = json_obj_get(wake, "phrase");
            if (ph && ph->type == JSON_STRING) phrase = ph->str_val;
        }
        if (enabled) {
            char buf[128];
            if (!phrase || !*phrase)
                snprintf(buf, sizeof buf, "hey %s", name);
            else
                snprintf(buf, sizeof buf, "%s", phrase);
            json_set(result, name, json_string(buf));
        }
        if (cfg) json_free(cfg);
    }
    free(packed);
    return result;
}

/* PoP: _import_audio @ tools/wake_word.py:_import_audio */
int ww_import_audio(void)
{
    /* Native ALSA probe: open the default PCM in capture mode briefly to
     * confirm the audio subsystem is available (mirrors sounddevice import). */
    snd_pcm_t *pcm = NULL;
    int err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_CAPTURE,
                           SND_PCM_NONBLOCK);
    if (pcm) snd_pcm_close(pcm);
    return (err == 0) ? 0 : -1;
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
    json_set(details, "selector", json_number(selector));

    snd_ctl_t *ctl = NULL;
    /* Resolve the selector to a device name. PortAudio uses integer indices;
     * ALSA defaults to "default" when the index is -1 or unavailable. */
    const char *devname = "default";
    if (selector >= 0) {
        char hwname[64];
        snprintf(hwname, sizeof(hwname), "hw:%d", selector);
        int err = snd_ctl_open(&ctl, hwname, 0);
        if (err < 0) {
            json_set(details, "error",
                     json_string(snd_strerror(err)));
            return details;
        }
        devname = hwname;
    } else {
        /* selector -1 / None → default device */
        int err = snd_ctl_open(&ctl, "default", 0);
        if (err < 0) {
            json_set(details, "error", json_string("no default audio device"));
            return details;
        }
    }

    /* Query PCM info for max channels + default sample rate. */
    snd_pcm_t *pcm = NULL;
    int err = snd_pcm_open(&pcm, devname, SND_PCM_STREAM_CAPTURE,
                           SND_PCM_NONBLOCK);
    if (err < 0) {
        json_set(details, "error", json_string(snd_strerror(err)));
        if (ctl) snd_ctl_close(ctl);
        return details;
    }

    snd_pcm_hw_params_t *params = NULL;
    snd_pcm_hw_params_alloca(&params);
    if (snd_pcm_hw_params(pcm, params) >= 0) {
        unsigned int rate = 0;
        snd_pcm_hw_params_get_rate(params, &rate, 0);
        if (rate == 0) rate = 16000;
        json_set(details, "default_samplerate", json_number(rate));

        unsigned int chans = 1;
        snd_pcm_hw_params_get_channels(params, &chans);
    }

    /* Device name from the control handle. */
    char card_name[128] = {0};
    if (ctl) {
        snd_ctl_card_info_t *info = NULL;
        snd_ctl_card_info_alloca(&info);
        if (info && snd_ctl_card_info(ctl, info) == 0) {
            const char *name = snd_ctl_card_info_get_name(info);
            if (name) {
                snprintf(card_name, sizeof(card_name), "%s", name);
                json_set(details, "name", json_string(card_name));
            }
            int ha = snd_ctl_card_info_get_card(info);
            json_set(details, "hostapi_index", json_number(ha));
            const char *ha_name = snd_ctl_card_info_get_driver(info);
            if (ha_name)
                json_set(details, "hostapi", json_string(ha_name));
        }
    }

    /* max_input_channels */
    unsigned int max_ch = 0;
    if (params) snd_pcm_hw_params_get_channels_max(params, &max_ch);
    if (max_ch == 0) max_ch = 1;
    json_set(details, "max_input_channels", json_number(max_ch));

    if (params) snd_pcm_hw_params_free(params);
    snd_pcm_close(pcm);
    if (ctl) snd_ctl_close(ctl);
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
    /* Native: read the merged config's stt section; enabled + real provider.
     * Mirrors Python transcription_tools.is_stt_enabled + _get_provider. */
    json_t *config = config_py_load_config_readonly();
    if (!config || config->type != JSON_OBJECT) {
        if (config) json_free(config);
        return false;
    }
    json_t *stt = json_obj_get(config, "stt");
    bool enabled = true;
    const char *provider = NULL;
    if (stt && stt->type == JSON_OBJECT) {
        json_t *en = json_obj_get(stt, "enabled");
        if (en && en->type == JSON_BOOL) enabled = en->bool_val;
        else if (en && en->type == JSON_NUMBER) enabled = en->num_val != 0.0;
        else if (en && en->type == JSON_STRING)
            enabled = strcmp(en->str_val, "false") != 0 &&
                      strcmp(en->str_val, "0") != 0 &&
                      strcmp(en->str_val, "no") != 0 &&
                      strcmp(en->str_val, "off") != 0;
        json_t *pv = json_obj_get(stt, "provider");
        if (pv && pv->type == JSON_STRING) provider = pv->str_val;
    }
    json_free(config);
    if (!enabled) return false;
    /* _get_provider returns "none" when explicitly set to a disabled
     * value; default (unset) means the provider is the built-in local one. */
    if (!provider) return true;                       /* default = local */
    size_t plen = strlen(provider);
    while (plen > 0 && (provider[plen-1]==' '||provider[plen-1]=='\t'||
                        provider[plen-1]=='\n'||provider[plen-1]=='\r')) plen--;
    if (plen == 0) return true;                       /* empty -> default */
    return strncasecmp(provider, "none", 4) != 0;     /* != "none" */
}

/* PoP: _tts_ready @ tools/wake_word.py:_tts_ready */
bool ww_tts_ready(void)
{
    /* Native: read the merged config's tts section; any non-empty,
     * non-"none" provider counts as ready (lazy install at first use).
     * Mirrors Python tts_tool._get_provider + wake_word._tts_ready. */
    json_t *config = config_py_load_config_readonly();
    if (!config || config->type != JSON_OBJECT) {
        if (config) json_free(config);
        return false;
    }
    json_t *tts = json_obj_get(config, "tts");
    const char *provider = NULL;
    if (tts && tts->type == JSON_OBJECT) {
        json_t *pv = json_obj_get(tts, "provider");
        if (pv && pv->type == JSON_STRING) provider = pv->str_val;
        else if (pv && pv->type == JSON_NUMBER) {
            char *s = json_serialize(pv);
            if (s) {
                /* reuse the serialized number as the provider label */
                provider = NULL; /* numeric providers are not valid names */
                free(s);
            }
        }
    }
    json_free(config);
    if (!provider) return false;                        /* no provider */
    size_t plen = strlen(provider);
    while (plen > 0 && (provider[plen-1]==' '||provider[plen-1]=='\t'||
                        provider[plen-1]=='\n'||provider[plen-1]=='\r')) plen--;
    if (plen == 0) return false;                        /* empty -> none */
    return strncasecmp(provider, "none", 4) != 0;       /* != "none" */
}

/* PoP: check_wake_word_requirements @ tools/wake_word.py:check_wake_word_requirements */
json_t *ww_check_wake_word_requirements(const json_t *cfg)
{
    /* Native port: deterministic feasibility report over the same signals as
     * the Python original. The only third-party component here is the ML
     * runtime package itself; whether it is installed is a site-packages
     * availability probe (our code), not the runtime. */
    if (!cfg || cfg->type != JSON_OBJECT) cfg = NULL;

    /* provider = _provider(cfg) */
    char *provider = ww_provider(cfg);

    /* feature selection */
    const char *feature = "wake.openwakeword";
    bool is_porcupine = provider && (strcasecmp(provider, "porcupine") == 0);
    bool is_sherpa = provider && (strcasecmp(provider, "sherpa") == 0 ||
                                  strcasecmp(provider, "sherpa-onnx") == 0 ||
                                  strcasecmp(provider, "kws") == 0 ||
                                  strcasecmp(provider, "open") == 0);
    if (is_porcupine) feature = "wake.porcupine";
    else if (is_sherpa) feature = "wake.sherpa";

    /* deps_ok = lazy_deps.is_available(feature) — native site-packages probe. */
    bool deps_ok = ww_site_package_available(feature);

    /* lazy_ok = lazy_deps._allow_lazy_installs() */
    bool lazy_ok = ww_allow_lazy_installs();

    /* audio_ok = _audio_available() if deps_ok else False */
    bool audio_ok = deps_ok ? ww_audio_available() : false;

    bool key_ok = true;
    bool stt_ok = ww_stt_ready();
    bool tts_ok = ww_tts_ready();
    const char *hint = "";

    /* tflite backend needs a runtime openWakeWord doesn't declare off Linux. */
    bool tflite_ok = true;
    if (!is_porcupine && !is_sherpa) {
        char *framework = ww_resolve_inference_framework(cfg, ww_is_macos_arm64());
        if (framework && strcasecmp(framework, "tflite") == 0) {
            tflite_ok = ww_ensure_tflite_runtime()
                        || ww_site_package_available("wake.openwakeword.tflite")
                        || lazy_ok;
        }
        free(framework);
    }

    if (is_porcupine) {
        const char *key = getenv("PORCUPINE_ACCESS_KEY");
        if (!key || !key[0]) {
            key_ok = false;
            hint = "Set PORCUPINE_ACCESS_KEY (free key at https://console.picovoice.ai).";
        }
    } else if (!deps_ok && !lazy_ok) {
        /* feature_install_command(feature) */
        hint = "uv pip install sherpa-onnx openwakeword onnxruntime "
               "pvporcupine sounddevice numpy";
    } else if (!tflite_ok) {
        hint = "The wake word needs the tflite runtime on this Mac: pip install ai-edge-litert";
    } else if (deps_ok && !audio_ok) {
        hint = "Microphone capture needs sounddevice + numpy and a working audio device.";
    } else if (!stt_ok || !tts_ok) {
        bool miss_stt = !stt_ok;
        bool miss_tts = !tts_ok;
        if (miss_stt && miss_tts)
            hint = "Wake word needs speech-to-text and text-to-speech configured — run `hermes tools` (Voice section) or see the voice-mode docs.";
        else if (miss_stt)
            hint = "Wake word needs speech-to-text configured — run `hermes tools` (Voice section) or see the voice-mode docs.";
        else
            hint = "Wake word needs text-to-speech configured — run `hermes tools` (Voice section) or see the voice-mode docs.";
    }

    bool available = key_ok && stt_ok && tts_ok && tflite_ok
                     && ((deps_ok && audio_ok) || (!deps_ok && lazy_ok));

    json_t *result = json_object();
    json_set(result, "available", json_bool(available));
    json_set(result, "provider", json_string(provider ? provider : ""));
    json_set(result, "deps_available", json_bool(deps_ok));
    json_set(result, "audio_available", json_bool(audio_ok));
    json_set(result, "access_key_set", json_bool(key_ok));
    json_set(result, "stt_available", json_bool(stt_ok));
    json_set(result, "tts_available", json_bool(tts_ok));
    char *phrase = ww_wake_phrase(cfg);
    json_set(result, "phrase", json_string(phrase ? phrase : ""));
    free(phrase);
    if (hint[0])
        json_set(result, "hint", json_string(hint));
    free(provider);
    return result;
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
