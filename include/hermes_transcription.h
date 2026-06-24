#ifndef HERMES_TRANSCRIPTION_H
#define HERMES_TRANSCRIPTION_H

/*
 * hermes_transcription.h — Transcription provider registry for Hermes C.
 * Port of Python agent/transcription_registry.py.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register a transcription provider. Returns true on success.
 * Rejects built-in names (local, groq, openai, mistral, xai).
 * Port of Python transcription_registry.py:register_provider(). */
bool transcription_register_provider(const char *name, const char *provider_type);

/* List registered provider names. Returns comma-separated malloc'd string (caller free).
 * Returns empty string if no providers registered.
 * Port of Python transcription_registry.py:list_providers(). */
char *transcription_list_providers(void);

/* Get a registered provider by name (case-insensitive). Returns provider type or NULL.
 * Port of Python transcription_registry.py:get_provider(). */
const char *transcription_get_provider(const char *name);

/* Clear all registered providers. Test-only.
 * Port of Python transcription_registry.py:_reset_for_tests(). */
void transcription_reset_for_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TRANSCRIPTION_H */
