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
#include <stdint.h>
#include <stdio.h>

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

/* --- Platform & audio helpers --- */

/* PoP: _is_macos_arm64 @ tools/wake_word.py:_is_macos_arm64 */
/* True when running on macOS ARM64 (Apple Silicon). */
bool ww_is_macos_arm64(void);

/* PoP: default_inference_framework @ tools/wake_word.py:default_inference_framework */
/* "tflite" on macOS ARM64, "onnx" everywhere else. */
const char *ww_default_inference_framework(void);

/* PoP: ensure_tflite_runtime @ tools/wake_word.py:ensure_tflite_runtime */
/* Try to import tflite_runtime.interpreter; if missing, bridge
 * ai_edge_litert -> tflite_runtime (macOS). Returns true on success. */
bool ww_ensure_tflite_runtime(void);

/* PoP: load_wake_word_config @ tools/wake_word.py:load_wake_word_config */
/* Return the wake_word config section as a JSON object. NULL on
 * failure (returns an empty object reference via the json_t API). */
json_t *ww_load_wake_word_config(void);

/* PoP: _active_profile_name @ tools/wake_word.py:_active_profile_name */
/* Return the active profile name, or "default". malloc'd string. */
char *ww_active_profile_name(void);

/* PoP: enrolled_profile_phrases @ tools/wake_word.py:enrolled_profile_phrases */
/* Map profile name -> wake phrase for every wake-enabled profile.
 * Returns a JSON object (malloc'd, caller frees). */
json_t *ww_enrolled_profile_phrases(void);

/* PoP: _import_audio @ tools/wake_word.py:_import_audio */
/* Attempt to import sounddevice + numpy. Returns 0 on success,
 * -1 on ImportError/OSError. */
int ww_import_audio(void);

/* PoP: _audio_available @ tools/wake_word.py:_audio_available */
/* True when sounddevice + numpy can be imported. */
bool ww_audio_available(void);

/* PoP: _describe_input_device @ tools/wake_word.py:_describe_input_device */
/* Resolve a PortAudio selector into a JSON details dict. */
json_t *ww_describe_input_device(int selector);

/* PoP: silent_audio_hint @ tools/wake_word.py:silent_audio_hint */
/* Platform-specific remediation string for a silent microphone.
 * Returns a malloc'd string. */
char *ww_silent_audio_hint(const json_t *device_details);

/* PoP: _build_engine @ tools/wake_word.py:_build_engine */
/* Build the active wake-word engine from config. Returns a
 * pointer to an opaque engine handle, or NULL on failure. */
void *ww_build_engine(const json_t *cfg);

/* PoP: _stt_ready @ tools/wake_word.py:_stt_ready */
/* True when a speech-to-text provider is configured and enabled. */
bool ww_stt_ready(void);

/* PoP: _tts_ready @ tools/wake_word.py:_tts_ready */
/* True when the configured TTS provider is ready or can auto-install. */
bool ww_tts_ready(void);

/* PoP: check_wake_word_requirements @ tools/wake_word.py:check_wake_word_requirements */
/* Report whether wake-word detection can run, with a remediation hint.
 * Returns a JSON dict (malloc'd, caller frees). */
json_t *ww_check_wake_word_requirements(const json_t *cfg);

/* --- WakeWordDetector lifecycle --- */

/* Opaque detector handle (mirrors Python's WakeWordDetector). */
typedef struct ww_detector ww_detector_t;

/* PoP: WakeWordDetector.__init__ @ tools/wake_word.py:WakeWordDetector */
/* Create a detector with the given engine and callback. */
ww_detector_t *ww_detector_create(void *engine,
                                  void (*on_wake)(void),
                                  double cooldown_seconds,
                                  void (*on_failure)(ww_detector_t *),
                                  int input_device);

/* PoP: running @ tools/wake_word.py:running */
/* True when the detector thread is alive. */
bool ww_detector_running(const ww_detector_t *det);

/* PoP: start @ tools/wake_word.py:start */
/* Open the mic and begin listening. Returns 0 on success,
 * -1 on timeout, -2 on runtime error. */
int ww_detector_start(ww_detector_t *det);

/* PoP: pause @ tools/wake_word.py:pause */
/* Stop the mic stream but keep the engine alive. */
void ww_detector_pause(ww_detector_t *det);

/* PoP: resume @ tools/wake_word.py:resume */
/* Re-open the mic stream. Returns 0 on success, -1 on failure. */
int ww_detector_resume(ww_detector_t *det);

/* PoP: stop @ tools/wake_word.py:stop */
/* Fully stop the detector and tear down the engine. */
void ww_detector_stop(ww_detector_t *det);

/* PoP: _halt_thread @ tools/wake_word.py:_halt_thread */
/* Internal: signal the thread to stop and join it. */
void ww_detector_halt(ww_detector_t *det);

/* PoP: _dispatch_wake @ tools/wake_word.py:_dispatch_wake */
/* Internal: invoke the on_wake callback with error protection. */
void ww_detector_dispatch_wake(ww_detector_t *det);

/* PoP: _run @ tools/wake_word.py:_run */
/* Internal: the detector thread entry point. */
void *ww_detector_run(void *arg);

/* PoP: _lock_path @ tools/wake_word.py:_lock_path */
/* Return the path to the cross-process wake-word lock file.
 * malloc'd string. */
char *ww_lock_path(void);

/* PoP: _acquire_machine_lock @ tools/wake_word.py:_acquire_machine_lock */
/* Acquire the cross-process microphone lease. Returns a FILE*
 * handle on success, NULL and raises WakeWordInUse on failure. */
FILE *ww_acquire_machine_lock(const char *lock_path);

/* PoP: _release_machine_lock @ tools/wake_word.py:_release_machine_lock */
/* Release and close the machine lock handle. */
void ww_release_machine_lock(FILE *handle);

/* PoP: _detector_failed @ tools/wake_word.py:_detector_failed */
/* Callback when the active mic stream dies: release ownership. */
void ww_detector_failed(ww_detector_t *det);

/* PoP: start_listening @ tools/wake_word.py:start_listening */
/* Claim, build, and start the detector. Returns the detector
 * on success, NULL on failure. */
ww_detector_t *ww_start_listening(void (*on_wake)(void),
                                  void *owner,
                                  const json_t *config);

/* PoP: owns_listener @ tools/wake_word.py:owns_listener */
/* True when the given owner holds the active detector. */
bool ww_owns_listener(void *owner);

/* PoP: pause_listening @ tools/wake_word.py:pause_listening */
/* Release the mic only when owner holds the lease. */
bool ww_pause_listening(void *owner);

/* PoP: resume_listening @ tools/wake_word.py:resume_listening */
/* Re-open the mic only when owner holds the lease. */
bool ww_resume_listening(void *owner);

/* PoP: stop_listening @ tools/wake_word.py:stop_listening */
/* Fully stop the detector only when owner holds the lease. */
bool ww_stop_listening(void *owner);

/* PoP: is_listening @ tools/wake_word.py:is_listening */
/* True when the detector thread is running. */
bool ww_is_listening(void);

/* PoP: audio_is_silent @ tools/wake_word.py:audio_is_silent */
/* True when the armed stream has delivered only silence. */
bool ww_audio_is_silent(void);

/* PoP: get_input_device_status @ tools/wake_word.py:get_input_device_status */
/* Return PortAudio input diagnostics for status UIs. */
json_t *ww_get_input_device_status(const json_t *cfg);

/* PoP: get_last_match @ tools/wake_word.py:get_last_match */
/* (matched phrase, profile) of the most recent wake fire. */
json_t *ww_get_last_match(void);

/* --- Engine contract (_Engine + provider engines) --- */

/* Opaque hotword engine handle (mirrors Python's _Engine subclasses). */
typedef struct ww_engine ww_engine_t;

/* PoP: ww_engine_create @ tools/wake_word.py:_OpenWakeWordEngine.__init__ */
/* Build the engine selected by cfg["provider"]:
 * openwakeword / sherpa / porcupine. Returns an opaque handle
 * on success, NULL on failure. Mirrors _OpenWakeWordEngine.__init__,
 * _SherpaKwsEngine.__init__ and _PorcupineEngine.__init__. */
ww_engine_t *ww_engine_create(const json_t *cfg);

/* PoP: ww_engine_process @ tools/wake_word.py:_OpenWakeWordEngine.process */
/* Feed one int16 frame (frame_length samples at 16 kHz); returns true
 * when the phrase is heard (with the provider's confirmation-streak
 * logic applied). Mirrors _Engine.process + all engine subclasses. */
bool ww_engine_process(ww_engine_t *eng, const int16_t *frame, size_t n);

/* PoP: ww_engine_reset @ tools/wake_word.py:_OpenWakeWordEngine.reset */
/* Clear any internal audio/feature buffer (called on every restart).
 * Mirrors _Engine.reset + _OpenWakeWordEngine.reset + _SherpaKwsEngine.reset. */
void ww_engine_reset(ww_engine_t *eng);

/* PoP: ww_engine_close @ tools/wake_word.py:_OpenWakeWordEngine.close */
/* Tear down the engine and release resources.
 * Mirrors _Engine.close + all engine subclasses' close. */
void ww_engine_close(ww_engine_t *eng);

/* PoP: ww_sherpa_model_root @ tools/wake_word.py:_sherpa_model_root */
/* Return the directory where sherpa KWS models are cached.
 * malloc'd string. */
char *ww_sherpa_model_root(void);

/* PoP: ww_ensure_sherpa_model @ tools/wake_word.py:_ensure_sherpa_model */
/* Download + unpack the sherpa KWS model once; return its
 * directory. malloc'd string. */
char *ww_ensure_sherpa_model(const char *root);

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
