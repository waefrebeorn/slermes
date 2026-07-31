/*
 * transcribe_helpers.h — Declarations for tools/transcription_tools.py helpers.
 *
 * This header intentionally stays narrow: only the helpers needed by the
 * transcription tool registration + runtime. Provider-specific dispatch
 * lives in its own port files.
 */
#ifndef TRANSCRIBE_HELPERS_H
#define TRANSCRIBE_HELPERS_H

#include <stdbool.h>
#include <stdio.h>

typedef struct json_t json_t;

#ifdef __cplusplus
extern "C" {
#endif

const char *transcribe_default_stt_config(void);
bool transcribe_is_stt_enabled(const char *config_json);
const char *transcribe_find_binary(const char *name);
const char *transcribe_find_ffmpeg_binary(void);
const char *transcribe_find_whisper_binary(void);
const char *transcribe_extract_transcript_text(const char *json_text);
bool transcribe_looks_like_cuda_lib_error(const char *msg);
char *transcribe_openai_compatible(const char *url,
                                   const char *api_key,
                                   const char *file_path,
                                   const char *model,
                                   const char *language);
char *transcribe_validate_audio_file(const char *file_path);
json_t *transcribe_get_stt_section(const char *config_json, const char *name);
bool transcribe_is_command_stt_provider_config(const char *config_json);
int transcribe_get_command_stt_timeout(const char *config_json);
const char *transcribe_get_command_stt_output_format(const char *config_json);
char *transcribe_transcribe_groq(const char *file_path, const char *model);
char *transcribe_transcribe_mistral(const char *file_path, const char *model);
char *transcribe_transcribe_xai(const char *file_path, const char *model);
char *transcribe_transcribe_elevenlabs(const char *file_path, const char *model);
char *transcribe_transcribe_deepinfra(const char *file_path, const char *model);

#ifdef __cplusplus
}
#endif

#endif /* TRANSCRIBE_HELPERS_H */
