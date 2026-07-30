/*
 * lmstudio_reasoning.h — LM Studio reasoning-effort resolution.
 *
 * Maps user's reasoning_config onto LM Studio's OpenAI-compatible
 * vocabulary, then clamps against the model's allowed_options so the
 * server doesn't 400 on an unsupported effort.
 *
 * Port of Python agent/lmstudio_reasoning.py
 */

#ifndef HERMES_LMSTUDIO_REASONING_H
#define HERMES_LMSTUDIO_REASONING_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Valid effort names ────────────────────────────────────── */

/** Return true if *effort* is a valid LM Studio reasoning_effort value. */
bool lmstudio_is_valid_effort(const char *effort);

/** Map an effort alias (off→none, on→medium) or return raw string. */
const char *lmstudio_map_effort_alias(const char *effort);

/* ── Resolution ────────────────────────────────────────────── */

/**
 * resolve_lmstudio_effort — Return the reasoning_effort string to send to
 * LM Studio, or NULL meaning "omit the field".
 *
 * @param reasoning_config  Dict with 'enabled' (bool) and 'effort' (str) keys,
 *                          or NULL. If 'enabled' is False, returns "none".
 *                          If 'effort' is not set, returns "medium" (default).
 * @param allowed_options   NULL-terminated array of model's allowed options
 *                          (e.g. {"off","on",NULL}), or NULL to skip clamping.
 * @return                  Interned string (do NOT free) or NULL.
 */
const char *resolve_lmstudio_effort(void *reasoning_config, const char *const *allowed_options);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_LMSTUDIO_REASONING_H */