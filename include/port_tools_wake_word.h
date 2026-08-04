/*
 * port_tools_wake_word.h — C11 port of pure config/device helpers from
 * tools/wake_word.py.
 *
 * Ports the deterministic, I/O-free helpers that translate the
 * wake_word config section (a JSON dict, exactly like Python's
 * Dict[str, Any]) into typed values with the same defaults and
 * clamping as the Python originals:
 *
 *   _get, _provider, _input_device, _sensitivity,
 *   _confirmation_frames, wake_phrase, wake_surface_enabled,
 *   resolve_inference_framework (platform flag passed in),
 *   _device_label, _looks_like_path, _bundled_wakeword_path.
 *
 * Heavy machinery (ONNX/tflite/sherpa/porcupine engines, sounddevice
 * capture, machine lock, listener lifecycle) remains in Python; this
 * header only covers the pure logic that transforms config JSON into
 * values the rest of the C tree can consume.
 *
 * Memory: string-returning functions return malloc'd strings (caller
 * frees) or NULL. All other functions return a value by value.
 */

#ifndef PORT_TOOLS_WAKE_WORD_H
#define PORT_TOOLS_WAKE_WORD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque json_t* is enough — the caller passes a parsed wake_word
 * config object (libjson) or NULL, matching Python's
 * cfg: Optional[Dict[str, Any]]. */
typedef struct json_t json_t;

/* PoP: _get @ tools/wake_word.py:_get */
/* cfg.get(key, default) with the Python _DEFAULTS fallback:
 * returns the value for key, or the module default, or NULL when
 * neither exists. */
const char *ww_get_str(const json_t *cfg, const char *key);
double       ww_get_num(const json_t *cfg, const char *key, double dflt);
bool         ww_get_bool(const json_t *cfg, const char *key, bool dflt);

/* PoP: _provider @ tools/wake_word.py:_provider */
/* str(_get(cfg,"provider")).strip().lower() or "openwakeword".
 * Returns a malloc'd string (caller frees). */
char *ww_provider(const json_t *cfg);

/* PoP: _input_device @ tools/wake_word.py:_input_device */
/* Configured PortAudio input selector, preserving indices and names.
 * Returns: 1 -> *out_int set (int selector),
 *          0 -> *out_str set to malloc'd name (or NULL for None).
 * Mirrors Python's int | str | None return. */
int ww_input_device(const json_t *cfg, int *out_int, char **out_str);

/* PoP: _sensitivity @ tools/wake_word.py:_sensitivity */
/* float with default 0.6, clamped to [0.0, 1.0]. */
double ww_sensitivity(const json_t *cfg);

/* PoP: _confirmation_frames @ tools/wake_word.py:_confirmation_frames */
/* int with default 3, clamped to [1, 10]. */
int ww_confirmation_frames(const json_t *cfg);

/* PoP: wake_phrase @ tools/wake_word.py:wake_phrase */
/* str(_get(cfg,"phrase")) or "hey hermes". malloc'd string. */
char *ww_wake_phrase(const json_t *cfg);

/* PoP: wake_surface_enabled @ tools/wake_word.py:wake_surface_enabled */
/* True when wake_word.enabled and the configured surface is "auto" or
 * exactly `surface`. cfg may be NULL (treated as {}). */
bool ww_wake_surface_enabled(const char *surface, const json_t *cfg);

/* PoP: resolve_inference_framework @ tools/wake_word.py:resolve_inference_framework */
/* Effective openWakeWord backend from config. Honors explicit
 * inference_framework except "onnx" on macOS ARM64 (coerced to tflite).
 * is_macos_arm64 is passed in so the decision stays pure; the caller
 * computes it from uname/registry. malloc'd string. */
char *ww_resolve_inference_framework(const json_t *cfg, bool is_macos_arm64);

/* PoP: _device_label @ tools/wake_word.py:_device_label */
/* Human label for an input-device details dict: name, or "system
 * default" when selector is None, or the selector; plus " (hostapi)"
 * when a hostapi is present. malloc'd string. */
char *ww_device_label(const json_t *details);

/* PoP: _looks_like_path @ tools/wake_word.py:_looks_like_path */
/* True when value contains a path separator or ends with a known
 * model extension (.onnx/.tflite/.ppn). The Python original also
 * checks os.path.exists(value); that I/O is intentionally left to
 * the caller (pure function here). */
bool ww_looks_like_path(const char *value);

/* PoP: _bundled_wakeword_path @ tools/wake_word.py:_bundled_wakeword_path */
/* "tools_dir/wakewords/hey_hermes.<ext>" where ext is "tflite" when
 * framework is "tflite" (case-insensitive, stripped), else "onnx".
 * tools_dir is the directory of the calling binary/tool. malloc'd. */
char *ww_bundled_wakeword_path(const char *tools_dir, const char *framework);

/* --- Constants shared with the Python original --- */
#define WW_SAMPLE_RATE               16000
#define WW_FIRE_COOLDOWN_SECONDS     2.0
#define WW_START_TIMEOUT_SECONDS     5.0
#define WW_DEFAULT_CONFIRMATION_FRAMES 3
#define WW_SILENCE_PEAK              10
#define WW_SILENCE_ALERT_SECONDS     10
#define WW_DEFAULT_PROVIDER          "openwakeword"
#define WW_DEFAULT_PHRASE            "hey hermes"
#define WW_DEFAULT_SENSITIVITY       0.6
#define WW_BUNDLED_MODEL_NAME        "hey_hermes"

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_WAKE_WORD_H */
