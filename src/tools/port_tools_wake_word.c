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
