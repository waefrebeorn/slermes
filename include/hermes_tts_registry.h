#ifndef HERMES_TTS_REGISTRY_H
#define HERMES_TTS_REGISTRY_H

/*
 * hermes_tts_registry.h — TTS provider registry for Hermes C.
 * Port of Python agent/tts_registry.py.
 */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register a TTS provider. Rejects built-in names (edge, openai, elevenlabs, etc.). */
bool tts_register_provider(const char *name, const char *provider_type);

/* List registered provider names. Returns malloc'd comma-separated string (caller free). */
char *tts_list_providers(void);

/* Get a registered provider by name (case-insensitive). Returns provider type or NULL. */
const char *tts_get_provider(const char *name);

/* Clear all registered providers. Test-only. */
void tts_reset_for_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TTS_REGISTRY_H */
