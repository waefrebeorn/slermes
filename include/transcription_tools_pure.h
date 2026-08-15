/*
 * transcription_tools_pure.h — Port of tools/transcription_tools.py pure helpers.
 */
#ifndef TRANSCRIPTION_TOOLS_PURE_H
#define TRANSCRIPTION_TOOLS_PURE_H

#include <stdbool.h>

/* PoP: _is_local_stt_provider @ tools/transcription_tools.py:_is_local_stt_provider */
bool ts_is_local_stt_provider(const char *provider, const char *stt_config_json);

/* PoP: _command_stt_env_passthrough @ tools/transcription_tools.py:_command_stt_env_passthrough */
char **ts_command_stt_env_passthrough(const char *config_json, int *out_count);

/* PoP: _command_provider_env_passthrough @ tools/tts_tool.py:_command_provider_env_passthrough */
/* Shared logic (identical to STT variant — one owner for env_passthrough). */
char **tts_command_provider_env_passthrough(const char *config_json, int *out_count);

/* PoP: _is_local_or_private_url @ tools/transcription_tools.py:_is_local_or_private_url */
bool ts_is_local_or_private_url(const char *url);

/* PoP: _confidence_thresholds @ tools/transcription_tools.py:_confidence_thresholds */
void ts_confidence_thresholds(const char *stt_config_json,
                               double *no_speech_out, double *logprob_out);

/* PoP: _is_hallucinated_segment @ tools/transcription_tools.py:_is_hallucinated_segment */
bool ts_is_hallucinated_segment(const char *segment_json,
                                 double no_speech_threshold,
                                 double logprob_threshold);

/* PoP: _get_idle_unload_seconds @ tools/transcription_tools.py:_get_idle_unload_seconds */
/* Resolve the idle unload timeout from stt.local config (0 = never). */
int ts_get_idle_unload_seconds(const char *local_cfg_json);

/* PoP: _cloud_trim_settings @ tools/transcription_tools.py:_cloud_trim_settings */
/* Resolve (enabled, threshold_db, keep_ms) for the cloud silence trim. */
void ts_cloud_trim_settings(const char *stt_config_json,
                            bool *enabled_out, int *threshold_db_out,
                            int *keep_ms_out);

#endif
