/*
 * lmstudio_reasoning.h — LM Studio reasoning-effort resolution.
 *
 * Port of Python agent/lmstudio_reasoning.py
 *
 * Maps user's reasoning_config dict onto LM Studio's OpenAI-compatible
 * vocabulary, then clamps against the model's allowed_options.
 */

#ifndef HERMES_LMSTUDIO_REASONING_H
#define HERMES_LMSTUDIO_REASONING_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Config type ───────────────────────────────────────────── */

typedef struct {
    bool enabled;               /* true if reasoning is enabled, false to force "none" */
    const char *effort;         /* User-provided effort string or NULL */
} lmstudio_reasoning_config_t;

/* ── Pure helpers ──────────────────────────────────────────── */

/** Return true if @p effort is a valid LM Studio reasoning_effort value. */
bool lmstudio_is_valid_effort(const char *effort);

/**
 * lmstudio_map_effort_alias — Map UI aliases to LM Studio vocabulary.
 *
 *   off  -> none
 *   on   -> medium
 *   other strings are returned unchanged.
 *
 * Caller must not free the returned pointer; it points into static storage.
 */
const char *lmstudio_map_effort_alias(const char *effort);

/*
 * resolve_lmstudio_effort — Return the reasoning_effort string to send to
 * LM Studio, or NULL meaning "omit the field".
 *
 * Mirrors Python:
 *   def resolve_lmstudio_effort(
 *       reasoning_config: Optional[dict],
 *       allowed_options: Optional[List[str]],
 *   ) -> Optional[str]:
 */
const char *resolve_lmstudio_effort(const lmstudio_reasoning_config_t *reasoning_config, const char *const *allowed_options);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LMSTUDIO_REASONING_H */
