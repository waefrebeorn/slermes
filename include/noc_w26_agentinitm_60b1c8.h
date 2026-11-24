/*
 * noc_w26_agentinitm_60b1c8.h - C surface for upstream
 * agent/agent_init_memory_provider.py (gate w26_agentinitm_60b1c8).
 *
 * The scoping kwargs ``MemoryProvider.initialize`` receives at agent init: the
 * session/platform/home identity, the resumed-transcript facts, the gateway
 * identity params, the title and the profile name.  The agent object, its
 * session store and the profile lookup are seams -- their values arrive in a
 * state object, and what those values decide is ported here.  The unavailable
 * advisory is also ported, as the exact text the user is shown once per home.
 */
#ifndef NOC_W26_AGENTINITM_60B1C8_H
#define NOC_W26_AGENTINITM_60B1C8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Number of per-chat gateway identity params. */
#define W26_AGENTINITM_60B1C8_IDENT_PARAMS 8

/* _memory_provider_init_kwargs(agent, platform), with the agent seam as state:
 * {session_id, platform, hermes_home, memory_agent_context, parent_session_id,
 *  have_db, row, count, has_assistant, session_title, session_title_source,
 *  session_cwd, identities{...}, profile_set, profile}.  Callbacks are emitted
 *  as {"__callable__": "<name>"} -- the agent's sinks, not values. */
json_t *w26_agentinitm_60b1c8_init_kwargs(const json_t *state);

/* _stored_transcript_kwargs -> {"resumed": bool, "message_count": int};
 * resumed means the store holds an assistant reply for the session. */
json_t *w26_agentinitm_60b1c8_stored_transcript(const json_t *state);

/* _warn_memory_provider_unavailable's text for (name, reason). */
json_t *w26_agentinitm_60b1c8_unavailable_message(const json_t *state);

/* The module's literal constants, as an object. */
json_t *w26_agentinitm_60b1c8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
