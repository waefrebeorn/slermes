/*
 * transcription_provider.h — C equivalent of the Python TranscriptionProvider ABC
 * (agent/transcription_provider.py).
 *
 * Opaque handle + vtable-style accessors, backed by lib/libtranscribe.
 */
#ifndef HERMES_TRANSCRIPTION_PROVIDER_H
#define HERMES_TRANSCRIPTION_PROVIDER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct transcription_provider transcription_provider_t;

/* Create a provider backed by a registered libtranscribe provider type.
 * Returns NULL if name is empty. */
transcription_provider_t *transcription_provider_create(const char *name,
                                                         const char *display_name,
                                                         const char *model);
void transcription_provider_free(transcription_provider_t *p);

const char *transcription_provider_name(const transcription_provider_t *p);
const char *transcription_provider_display_name(const transcription_provider_t *p);
bool transcription_provider_is_available(const transcription_provider_t *p);
char *transcription_provider_transcribe(transcription_provider_t *p,
                                         const char *audio_path);
char *transcription_provider_get_setup_schema(const transcription_provider_t *p);

/* Register a provider in the global transcription registry. */
void transcription_provider_register(const char *name, const char *type);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TRANSCRIPTION_PROVIDER_H */
