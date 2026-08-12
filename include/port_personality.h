/*
 * port_personality.c — C11 port of hermes_cli/personality.py
 *
 * Single owner for personality overlays. Pure string/config transforms
 * operating on a raw config json_t dict. `persist_personality` (the file-op
 * atomic roundtrip) is NOT ported here — it needs the yaml roundtrip helper.
 */
#ifndef SLERMES_PORT_PERSONALITY_H
#define SLERMES_PORT_PERSONALITY_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Names that mean "no personality overlay". */
#define PER_NEUTRAL_NAMES {"", "none", "default", "neutral"}

/* Built-in personalities, available on every surface without config. */
typedef struct { const char *name; const char *prompt; } per_builtin_t;
extern const per_builtin_t per_BUILTIN_PERSONALITIES[];
extern const size_t per_BUILTIN_PERSONALITIES_COUNT;

/* Nested dict lookup tolerant of None/non-dict intermediate nodes.
 * PoP: _get @ hermes_cli/personality.py:_get */
const json_t *per_get(const json_t *cfg, const char *const *keys, size_t nkeys, const json_t *default_val);

/* PoP: prompt_text @ hermes_cli/personality.py:prompt_text
 * Normalize config prompt values (str | list | None) to text. Caller frees. */
char *per_prompt_text(const json_t *value);

/* PoP: render_personality_prompt @ hermes_cli/personality.py:render_personality_prompt
 * Render a string or structured personality definition to prompt text. Caller frees. */
char *per_render_personality_prompt(const json_t *value);

/* PoP: describe_personality @ hermes_cli/personality.py:describe_personality
 * Short preview line for list UIs. Caller frees. */
char *per_describe_personality(const json_t *value, int width);

/* PoP: normalize_personality_name @ hermes_cli/personality.py:normalize_personality_name
 * Canonical name ('' for any neutral spelling). Caller frees. */
char *per_normalize_personality_name(const json_t *value);

/* PoP: available_personalities @ hermes_cli/personality.py:available_personalities
 * Built-ins overlaid by user's agent.personalities (user wins).
 * Returns json_t* object; caller frees. */
json_t *per_available_personalities(const json_t *cfg);

/* PoP: resolve_personality @ hermes_cli/personality.py:resolve_personality
 * Resolve to (canonical_name, prompt_text). Returns 0 on success; -1 with
 * *out_err filled (malloc'd, caller frees) when unknown. Caller frees outputs. */
int per_resolve_personality(const json_t *value, const json_t *cfg,
                            char **out_name, char **out_prompt, char **out_err);

/* PoP: active_personality_name @ hermes_cli/personality.py:active_personality_name
 * Currently selected name ('' when none active). Caller frees. */
char *per_active_personality_name(const json_t *cfg);

/* PoP: resolve_ephemeral_system_prompt @ hermes_cli/personality.py:resolve_ephemeral_system_prompt
 * Session overlay from config. Caller frees. */
char *per_resolve_ephemeral_system_prompt(const json_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_PORT_PERSONALITY_H */
