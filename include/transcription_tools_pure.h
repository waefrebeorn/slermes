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

/* PoP: _is_local_or_private_url @ tools/transcription_tools.py:_is_local_or_private_url */
bool ts_is_local_or_private_url(const char *url);

#endif
