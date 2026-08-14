/* title_generator_helpers.h — pure/plumbing surface of the faithful C11 port
 * of agent/title_generator.py. The LLM/thread runtime half (generate_title,
 * auto_title_session, maybe_auto_title) lives with the agent runtime.
 * Implemented in src/agent/port_agent_title_generator.c.
 */

#ifndef SLERMES_TITLE_GENERATOR_HELPERS_H
#define SLERMES_TITLE_GENERATOR_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"        /* json_t (config object) */
#include "session_title.h"   /* db_t, session_title_result_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Configured title language, or "" to match the user. */
void title_language(char *out, size_t out_sz);

/* Whether automatic session title generation is enabled
 * (auxiliary.title_generation.enabled, default true; fail-open). */
/* _auto_title_enabled (config-driven): read-only config accessor + truthy
 * collapse. auto_title_from_config is the testable branch core. */
bool auto_title_from_config(const json_t *cfg, bool default_enabled);
bool auto_title_enabled(void);

/* Collapse a slash-skill-expanded turn back to what the user typed.
 * Caller frees. */
char *summarize_user_message(const char *user_message);

/* Persist a generated title with the only-if-empty predicate and duplicate-
 * collision (#N) recovery. Returns the malloc'd title actually persisted, or
 * NULL when a concurrent manual title won / the write failed. */
char *persist_session_title(db_t *session_db, const char *session_id,
                            const char *title,
                            session_title_result_t *result);

/* PoP: strip_control_wrappers @ agent/title_generator.py:strip_control_wrappers */
/* Remove leading machine-authored control wrappers (nested), returning the
 * user's prose. Caller frees. */
char *title_strip_control_wrappers(const char *text);

/* PoP: _clean_title @ agent/title_generator.py:_clean_title */
/* Normalize a model-produced title, or NULL when nothing usable remains.
 * Caller frees. */
char *title_clean_title(const char *text);

/* PoP: _extract_title_text @ agent/title_generator.py:_extract_title_text */
/* Pull the title out of a model response (JSON object, loose scan, prose
 * fallback). Caller frees. */
char *title_extract_title_text(const char *content);

/* PoP: is_titleable_user_message @ agent/title_generator.py:is_titleable_user_message */
/* True when a user message carries real intent to title from (not machine
 * scaffolding, not empty after stripping control wrappers). */
bool title_is_titleable_user_message(const char *user_message);

/* PoP: derive_title @ agent/title_generator.py:derive_title */
/* Build an instant (dumb, word-boundary) title from a user message. Caller
 * frees; NULL when nothing usable. */
char *title_derive_title(const char *user_message);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_TITLE_GENERATOR_HELPERS_H */
