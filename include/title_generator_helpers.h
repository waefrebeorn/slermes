/* title_generator_helpers.h — pure/plumbing surface of the faithful C11 port
 * of agent/title_generator.py. The LLM/thread runtime half (generate_title,
 * auto_title_session, maybe_auto_title) lives with the agent runtime.
 * Implemented in src/agent/port_agent_title_generator.c.
 */

#ifndef SLERMES_TITLE_GENERATOR_HELPERS_H
#define SLERMES_TITLE_GENERATOR_HELPERS_H

#include <stdbool.h>
#include <stddef.h>
#include "session_title.h"   /* db_t, session_title_result_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Configured title language, or "" to match the user. */
void title_language(char *out, size_t out_sz);

/* Whether automatic session title generation is enabled
 * (auxiliary.title_generation.enabled, default true; fail-open). */
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

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_TITLE_GENERATOR_HELPERS_H */
