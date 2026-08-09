/*
 * voice_mode_pure.h — Pure/platform/config helpers ported from
 * tools/voice_mode.py that have no audio-runtime dependency (no numpy,
 * no sounddevice, no ALSA). These close pure helper REAL_GAPs and are
 * oracle-verifiable.
 */
#ifndef VOICE_MODE_PURE_H
#define VOICE_MODE_PURE_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"  /* json_t (opaque struct) */

/* PoP: _is_nan @ tools/voice_mode.py:_is_nan */
bool is_nan(double value);

/* PoP: _get_beep_volume @ tools/voice_mode.py:_get_beep_volume
 * voice_cfg is the "voice" section JSON object (or NULL). Returns the clamped
 * beep_volume in 0.0-1.0, defaulting to 0.3. */
double voice_get_beep_volume(const json_t *voice_cfg);

/* PoP: _sounddevice_output_allowed @ tools/voice_mode.py:_sounddevice_output_allowed
 * False on macOS (Darwin) — sounddevice output triggers a TCC prompt there. */
bool _sounddevice_output_allowed(void);

/* PoP: _is_wsl @ tools/voice_mode.py:_is_wsl
 * True when running inside Windows Subsystem for Linux. */
bool _is_wsl(void);

/* PoP: _is_wsl2_env @ tools/voice_mode.py:_is_wsl2_env
 * True when running inside WSL2. */
bool _is_wsl2_env(void);

/* PoP: _wsl_powershell_tts_available @ tools/voice_mode.py:_wsl_powershell_tts_available
 * Whether the WSL2 PowerShell TTS playback fallback can be used. */
bool _wsl_powershell_tts_available(void);

/* PoP: _voice_debug_enabled @ tools/voice_mode.py:_voice_debug_enabled
 * True when HERMES_VOICE_DEBUG=1. */
bool _voice_debug_enabled(void);

/* PoP: _vad_log @ tools/voice_mode.py:_vad_log
 * VAD diagnostic logging to stderr. */
void _vad_log(const char *msg);

/* PoP: _load_voice_stop_phrases @ tools/voice_mode.py:_load_voice_stop_phrases
 * voice_cfg is the "voice" section JSON. Returns a malloc'd char** array
 * (caller frees via free_voice_stop_phrases) of lowercased stripped phrases. */
char **_load_voice_stop_phrases(const json_t *voice_cfg, int *out_count);

/* Caller frees the array + each string. */
void free_voice_stop_phrases(char **phrases, int count);

/* PoP: is_voice_stop_phrase @ tools/voice_mode.py:is_voice_stop_phrase
 * Returns True when transcript EXACTLY equals a configured stop phrase
 * (after lowercasing + stripping surrounding punctuation/whitespace). */
bool is_voice_stop_phrase(const char *transcript,
                          char **stop_phrases, int phrase_count);

/* PoP: voice_stop_hint @ tools/voice_mode.py:voice_stop_hint
 * Returns 'Say "<phrase>" to end the voice chat.' or "" when disabled.
 * Caller frees. */
char *voice_stop_hint(char **stop_phrases, int phrase_count);

/* PoP: thinking_sound_enabled @ tools/voice_mode.py:thinking_sound_enabled
 * Config gate: voice.thinking_sound (default True). */
bool thinking_sound_enabled(const json_t *voice_cfg);

/* PoP: mark_audio_output_active @ tools/voice_mode.py:mark_audio_output_active
 * Reference-count real audio output. */
void mark_audio_output_active(bool active);

/* PoP: is_audio_output_active @ tools/voice_mode.py:is_audio_output_active
 * True while TTS/file audio is actively playing. */
bool is_audio_output_active(void);

/* PoP: _max_duration_reached @ tools/voice_mode.py:_max_duration_reached
 * cap: voice.max_recording_seconds (0 or <=0 disables). elapsed: seconds. */
bool audio_recorder_max_duration_reached(double cap, double elapsed);

/* ---- final voice_mode.py REAL_GAP closures (tools/voice_mode.py) ---- */

/* PoP: _import_numpy @ tools/voice_mode.py:_import_numpy — 0 if numpy importable. */
int voice_import_numpy(void);

/* PoP: _default_input_samplerate @ tools/voice_mode.py:_default_input_samplerate */
int voice_default_input_samplerate(void);

/* PoP: _play_int16_via_tempfile @ tools/voice_mode.py:_play_int16_via_tempfile */
void voice_play_int16_via_tempfile(const int16_t *samples, size_t n, int sample_rate);

/* PoP: _synth_thinking_blip @ tools/voice_mode.py:_synth_thinking_blip */
int16_t *voice_synth_thinking_blip(float frequency, size_t *out_n);

/* PoP: start_thinking_sound @ tools/voice_mode.py:start_thinking_sound */
bool voice_start_thinking_sound(void);

/* PoP: stop_thinking_sound @ tools/voice_mode.py:stop_thinking_sound */
void voice_stop_thinking_sound(void);

/* PoP: _play_audio_file_impl @ tools/voice_mode.py:_play_audio_file_impl */
bool voice_play_audio_file_impl(const char *file_path);

/* PoP: full_duplex_listen @ tools/voice_mode.py:full_duplex_listen
 * phase callback receives "generation" or "playback". Returns malloc'd wav path
 * or NULL. */
char *voice_full_duplex_listen(int (*should_stop)(void),
                               bool (*is_playing)(void),
                               void (*on_trigger)(const char *phase),
                               double multiplier,
                               int sustained_ms, int calibration_ms, int grace_ms,
                               int pre_roll_ms, int endpoint_silence_ms,
                               int max_utterance_ms);

/* PoP: _check_plugin_stt_provider @ tools/voice_mode.py:_check_plugin_stt_provider */
bool voice_check_plugin_stt_provider(const char *provider);

#endif /* VOICE_MODE_PURE_H */
